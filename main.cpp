// SPDX-License-Identifier: GPL-2.0-or-later
#include "desktop.h"
#include "system.h"

#include <algorithm>
#include <memory>

#include <LayerShellQt/Window>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QHash>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QProcess>
#include <QScreen>
#include <QSettings>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickImageProvider>
#include <QSize>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QWindow>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

class Icons final : public QQuickImageProvider
{
public:
    explicit Icons(const GPtrArray *apps = nullptr) : QQuickImageProvider(Pixmap)
    {
        if (!apps)
            return;
        for (guint i = 0; i < apps->len; ++i) {
            const auto *app = static_cast<const ZavoApp *>(apps->pdata[i]);
            GIcon *icon = app->info ? g_app_info_get_icon(app->info) : nullptr;
            if (G_IS_FILE_ICON(icon)) {
                g_autofree char *path = g_file_get_path(g_file_icon_get_file(G_FILE_ICON(icon)));
                if (path)
                    appIcons.insert(QString::fromUtf8(app->id), QIcon(QString::fromUtf8(path)));
            } else if (G_IS_THEMED_ICON(icon)) {
                for (const char *const *name = g_themed_icon_get_names(G_THEMED_ICON(icon)); *name; ++name) {
                    const QIcon themed = QIcon::fromTheme(QString::fromUtf8(*name));
                    if (!themed.isNull()) {
                        appIcons.insert(QString::fromUtf8(app->id), themed);
                        break;
                    }
                }
            }
        }
    }

    bool hasAppIcon(const QString &id) const { return appIcons.contains(id); }

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requested) override
    {
        const QSize target = requested.isValid() ? requested : QSize(44, 44);
        const QString decoded = QUrl::fromPercentEncoding(id.toUtf8());
        QPixmap pixmap = appIcons.value(decoded).pixmap(target);
        if (pixmap.isNull()) {
            const QString state = id.section(u'/', 0, 0);
            const QString name = id.section(u'/', 1, 1);
            const QIcon icon = QIcon::fromTheme(name + QStringLiteral("-symbolic"), QIcon::fromTheme(name));
            QColor accent(QStringLiteral("#") + id.section(u'/', 2, 2));
            if (!accent.isValid())
                accent = QColor("#91f5c8");
            const bool pale = 0.2126 * accent.redF() + 0.7152 * accent.greenF() + 0.0722 * accent.blueF() > 0.48;
            const QColor tint = state == "dialog" ? QColor("#151515") :
                state == "active" ? QColor(pale ? "#122920" : "#f7f2ff") : pale ? accent : accent.lighter(175);
            pixmap = icon.pixmap(target);
            if (pixmap.isNull()) {
                pixmap = QPixmap(target);
                pixmap.fill(Qt::transparent);
                QPainter painter(&pixmap);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(Qt::NoPen);
                painter.setBrush(tint);
                painter.drawRoundedRect(pixmap.rect().adjusted(10, 10, -10, -10), 5, 5);
            } else {
                QPainter painter(&pixmap);
                painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                painter.fillRect(pixmap.rect(), tint);
            }
        }
        if (size)
            *size = pixmap.size();
        return pixmap;
    }

private:
    QHash<QString, QIcon> appIcons;
};

class Desktop final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool inSession READ inSession CONSTANT)
    Q_PROPERTY(QVariantList windows READ windows NOTIFY windowsChanged)
    Q_PROPERTY(QVariantMap radios READ radios NOTIFY radiosChanged)
    Q_PROPERTY(bool systemBusy READ systemBusy NOTIFY systemBusyChanged)
    Q_PROPERTY(QVariantMap appearance READ appearance NOTIFY appearanceChanged)
    Q_PROPERTY(bool canChangeLayout READ canChangeLayout NOTIFY layoutChanged)
    Q_PROPERTY(bool wideLayout READ wideLayout NOTIFY layoutChanged)

public:
    explicit Desktop(bool inSession, bool trackWindows)
        : session(inSession), tracking(inSession && trackWindows), apps(zavo_apps_new()),
          preferences(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                      QStringLiteral("/zavo/settings.ini"), QSettings::IniFormat)
    {
        connect(&radioQuery, &QProcess::finished, this, &Desktop::updateRadios);
        connect(&systemAction, &QProcess::stateChanged, this, &Desktop::systemBusyChanged);
        connect(&systemAction, &QProcess::finished, this, &Desktop::systemActionFinished);
        connect(&systemAction, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart)
                emit systemError(systemAction.errorString());
        });
        const QString configDir = QFileInfo(preferences.fileName()).absolutePath();
        QDir().mkpath(configDir);
        settingsWatcher.addPath(configDir);
        connect(&settingsWatcher, &QFileSystemWatcher::directoryChanged, this, &Desktop::reloadAppearance);
        connect(&orientationSensor, &QProcess::readyReadStandardOutput, this, &Desktop::updateRotation);
        rotationDebounce.setInterval(275);
        rotationDebounce.setSingleShot(true);
        connect(&rotationDebounce, &QTimer::timeout, this, [this] {
            if (!appearanceValues.value("rotationLock").toBool())
                applyRotation(pendingRotation);
        });
        reloadAppearance();
        if (!tracking)
            return;
        connect(&query, &QProcess::finished, this, &Desktop::updateWindows);
        connect(&timer, &QTimer::timeout, this, &Desktop::refresh);
        timer.start(500);
        refresh();
    }

    ~Desktop() override
    {
        for (QProcess *process : { &query, &radioQuery, &systemAction, &orientationSensor }) {
            if (process->state() != QProcess::NotRunning) {
                process->disconnect(this);
                process->kill();
                process->waitForFinished(100);
            }
        }
        g_ptr_array_unref(apps);
    }

    bool inSession() const { return session; }
    QVariantList windows() const { return openWindows; }
    QVariantMap radios() const { return radioStates; }
    bool systemBusy() const { return systemAction.state() != QProcess::NotRunning; }
    QVariantMap appearance() const { return appearanceValues; }
    bool canChangeLayout() const { return currentLayout.focused && currentLayout.active_window; }
    bool wideLayout() const
    {
        return windowLayouts.value(currentLayout.active_window, currentLayout.wide_default ||
            tabletLayout(currentLayout.width, currentLayout.height, appearanceValues.value("layoutMode").toString()));
    }

    Q_INVOKABLE bool tabletLayout(int width, int height, const QString &mode) const
    {
        return zavo_tablet_layout(mode.toUtf8().constData(), width, height);
    }

    Q_INVOKABLE void toggleLayout()
    {
        if (!canChangeLayout())
            return;
        windowLayouts.insert(currentLayout.active_window, !wideLayout());
        applyWindowLayout();
        emit layoutChanged();
    }

    Q_INVOKABLE QStringList cursorThemes() const
    {
        QStringList names{"default"};
        QStringList roots = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
        for (QString &path : roots)
            path += QStringLiteral("/icons");
        roots.append(QDir::homePath() + QStringLiteral("/.icons"));
        for (const QString &path : roots) {
            QDir directory(path);
            for (const QString &entry : directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                if (QFileInfo(directory.filePath(entry + QStringLiteral("/cursors"))).isDir() &&
                    !names.contains(entry))
                    names.append(entry);
            }
        }
        names.sort(Qt::CaseInsensitive);
        return names;
    }

    Q_INVOKABLE void saveAppearance(const QString &key, const QVariant &value)
    {
        QVariant stored;
        if (key == QStringLiteral("accent")) {
            const QColor color(value.toString());
            if (color.isValid())
                stored = color.name();
        } else if (key == QStringLiteral("wallpaper")) {
            const QString path = value.toString();
            const QUrl file(path);
            if (path.isEmpty() || (file.isLocalFile() && QFileInfo(file.toLocalFile()).isReadable()))
                stored = path;
        } else if (key == QStringLiteral("cursorTheme") && cursorThemes().contains(value.toString())) {
            stored = value.toString();
        } else if (key == QStringLiteral("cursorSize") && value.toInt() >= 16 && value.toInt() <= 64) {
            stored = value.toInt();
        } else if (key == QStringLiteral("animations")) {
            stored = value.toBool();
        } else if (key == QStringLiteral("layoutMode")) {
            const QString mode = value.toString();
            if (mode == "auto" || mode == "phone" || mode == "tablet")
                stored = mode;
        }
        if (!stored.isValid()) {
            emit systemError(QStringLiteral("Couldn’t use that appearance setting"));
            return;
        }
        preferences.setValue(key, stored);
        reloadAppearance();
    }

    Q_INVOKABLE void setRotation(int rotation)
    {
        if (!session || (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270))
            return;
        preferences.setValue(QStringLiteral("rotation"), rotation);
        preferences.setValue(QStringLiteral("rotationLock"), true);
        reloadAppearance();
    }

    Q_INVOKABLE void setRotationLock(bool locked)
    {
        if (!session)
            return;
        if (locked) {
            const int rotation = zavo_current_rotation();
            if (rotation >= 0)
                preferences.setValue(QStringLiteral("rotation"), rotation);
        }
        preferences.setValue(QStringLiteral("rotationLock"), locked);
        reloadAppearance();
    }

    Q_INVOKABLE void setLevel(const QString &device, int percent)
    {
        if (!systemBusy())
            systemAction.start(QCoreApplication::applicationFilePath(),
                               {"--set-level", device, QString::number(percent)});
    }

    Q_INVOKABLE void setPowerSaver(bool enabled)
    {
        if (!systemBusy())
            systemAction.start(QCoreApplication::applicationFilePath(), {"--power-saver", enabled ? "on" : "off"});
    }

    Q_INVOKABLE void refreshRadios()
    {
        if (!systemBusy() && radioQuery.state() == QProcess::NotRunning)
            radioQuery.start(QCoreApplication::applicationFilePath(), {"--radio-state"});
    }

    Q_INVOKABLE void setRadio(const QString &name, bool enabled)
    {
        if (!systemBusy())
            systemAction.start(QCoreApplication::applicationFilePath(),
                               {"--set-radio", name, enabled ? "on" : "off"});
    }

    Q_INVOKABLE void power(const QString &action)
    {
        if (!systemBusy())
            systemAction.start(QCoreApplication::applicationFilePath(), {"--power", action});
    }
    Icons *createIcons() const { return new Icons(apps); }

    QVariantList installedApps(const Icons &icons) const
    {
        QVariantList entries;
        for (guint i = 0; i < apps->len; ++i) {
            const auto *app = static_cast<const ZavoApp *>(apps->pdata[i]);
            const QString id = QString::fromUtf8(app->id);
            const QString source = icons.hasAppIcon(id)
                ? QStringLiteral("image://theme/") + QString::fromLatin1(QUrl::toPercentEncoding(id))
                : QString();
            entries.append(QVariantMap{
                {"appId", id}, {"name", QString::fromUtf8(app->name)},
                {"iconName", QString::fromUtf8(app->fallback_icon)}, {"iconUrl", source},
                {"shortcut", bool(app->shortcut)},
            });
        }
        return entries;
    }

    Q_INVOKABLE bool launch(const QString &id)
    {
        return zavo_launch_app(apps, id.toUtf8().constData(), false, session, layoutMode().constData());
    }

    Q_INVOKABLE bool launchPinned(const QString &name)
    {
        return zavo_launch_app(apps, name.toUtf8().constData(), true, session, layoutMode().constData());
    }

    Q_INVOKABLE bool openMaps()
    {
        return zavo_open_builtin(QFile::encodeName(QCoreApplication::applicationFilePath()).constData(),
                                 "--maps", session, layoutMode().constData());
    }

    Q_INVOKABLE bool openSettings()
    {
        return zavo_open_builtin(QFile::encodeName(QCoreApplication::applicationFilePath()).constData(),
                                 "--settings", session, layoutMode().constData());
    }

    Q_INVOKABLE bool focusWindow(qint64 id) { return session && zavo_focus_window(id); }
    Q_INVOKABLE bool closeWindow(qint64 id)
    {
        if (!session || !zavo_close_window(id))
            return false;
        closingWindow = id;
        refresh();
        return true;
    }
    Q_INVOKABLE bool showHome() { return session && zavo_show_home(); }

    Q_INVOKABLE bool closeFocusedWindow(const QString &output)
    {
        if (!session)
            return false;
        closingWindow = zavo_close_focused_window(output.toUtf8().constData());
        if (closingWindow)
            refresh();
        return closingWindow != 0;
    }

    Q_INVOKABLE void refreshWindows() { if (session) refresh(); }

    Q_INVOKABLE QString photoPath()
    {
        const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (pictures.isEmpty() || !QDir().mkpath(pictures))
            return {};
        return QDir(pictures).filePath(QStringLiteral("Zavo-") +
                                       QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz") +
                                       QStringLiteral(".jpg"));
    }

    Q_INVOKABLE bool endSession() { return session && zavo_end_session(); }

signals:
    void windowsChanged();
    void radiosChanged();
    void systemBusyChanged();
    void systemError(const QString &error);
    void appearanceChanged();
    void layoutChanged();
    void volumeAdjusted(int percent);

private:
    QByteArray layoutMode() const { return appearanceValues.value("layoutMode").toString().toUtf8(); }

    void reloadAppearance()
    {
        preferences.sync();
        const QVariantMap values{
            {"accent", preferences.value("accent", "#91f5c8").toString()},
            {"wallpaper", preferences.value("wallpaper").toString()},
            {"cursorTheme", preferences.value("cursorTheme", "default").toString()},
            {"cursorSize", preferences.value("cursorSize", 24).toInt()},
            {"animations", preferences.value("animations", true).toBool()},
            {"rotationLock", preferences.value("rotationLock", false).toBool()},
            {"rotation", preferences.value("rotation", 0).toInt()},
            {"layoutMode", preferences.value("layoutMode", "auto").toString()},
        };
        if (values == appearanceValues)
            return;
        const auto old = appearanceValues;
        appearanceValues = values;
        if (tracking) {
            if (old.value("layoutMode") != values.value("layoutMode")) {
                applyWindowLayout();
                emit layoutChanged();
            }
            if (old.value("cursorTheme") != values.value("cursorTheme") ||
                old.value("cursorSize") != values.value("cursorSize"))
                zavo_set_cursor(values.value("cursorTheme").toString().toUtf8().constData(),
                                values.value("cursorSize").toInt());
            const bool locked = values.value("rotationLock").toBool();
            if (locked) {
                rotationDebounce.stop();
                if (orientationSensor.state() != QProcess::NotRunning) {
                    orientationSensor.kill();
                    orientationSensor.waitForFinished(100);
                }
                if (old.value("rotationLock") != values.value("rotationLock") ||
                    old.value("rotation") != values.value("rotation"))
                    applyRotation(values.value("rotation").toInt());
            } else if (old.value("rotationLock") != values.value("rotationLock")) {
                orientationSensor.start(QCoreApplication::applicationFilePath(), {"--monitor-orientation"});
            }
        }
        emit appearanceChanged();
    }

    void applyRotation(int rotation)
    {
        if (rotation == lastRotation)
            return;
        if (zavo_rotate_display(rotation))
            lastRotation = rotation;
    }

    void updateRotation()
    {
        while (orientationSensor.canReadLine()) {
            bool valid = false;
            const int rotation = orientationSensor.readLine().trimmed().toInt(&valid);
            if (valid && !appearanceValues.value("rotationLock").toBool()) {
                pendingRotation = rotation;
                rotationDebounce.start();
            }
        }
    }

    void applyWindowLayout()
    {
        const QSize size(currentLayout.width, currentLayout.height);
        const bool wide = wideLayout();
        if (currentLayout.first_window &&
            (currentLayout.workspace_id != portraitWorkspace || size != portraitArea || wide != appliedWide) &&
            zavo_apply_layout(&currentLayout, wide)) {
            portraitWorkspace = currentLayout.workspace_id;
            portraitArea = size;
            appliedWide = wide;
        }
    }

    void updateRadios(int code, QProcess::ExitStatus status)
    {
        const auto values = radioQuery.readAllStandardOutput().trimmed().split(' ');
        if (systemBusy() || code != 0 || status != QProcess::NormalExit || values.size() != 8)
            return;
        const QVariantMap states{
            {"wifi", values[0].toInt()}, {"bluetooth", values[1].toInt()},
            {"mobile", values[2].toInt()}, {"gps", values[3].toInt()},
            {"battery", values[4].toInt()}, {"saver", values[5].toInt()},
            {"brightness", values[6].toInt()}, {"volume", values[7].toInt()},
        };
        if (states != radioStates) {
            radioStates = states;
            emit radiosChanged();
        }
    }

    void systemActionFinished(int code, QProcess::ExitStatus status)
    {
        if (code != 0 || status != QProcess::NormalExit) {
            const QString message = QString::fromUtf8(systemAction.readAllStandardError()).trimmed();
            emit systemError(message.isEmpty() ? QStringLiteral("The system rejected this action") : message);
        } else {
            const QStringList args = systemAction.arguments();
            if (args.size() == 3 && args[0] == "--set-level" && args[1] == "volume")
                emit volumeAdjusted(args[2].toInt());
        }
        refreshRadios();
    }

    void refresh()
    {
        if (query.state() == QProcess::NotRunning)
            query.start(QStringLiteral("swaymsg"), {"-r", "-t", "get_tree"});
    }

    void updateWindows(int code, QProcess::ExitStatus status)
    {
        if (code != 0 || status != QProcess::NormalExit)
            return;
        const QByteArray json = query.readAllStandardOutput();
        ZavoLayout layout;
        g_autoptr(GPtrArray) windows = zavo_windows_parse(json.constData(), json.size(), &layout);
        if (!windows)
            return;
        const bool oldWide = wideLayout();
        const bool oldCanChange = canChangeLayout();
        currentLayout = layout;
        applyWindowLayout();
        if (wideLayout() != oldWide || canChangeLayout() != oldCanChange)
            emit layoutChanged();
        bool closingWindowStillExists = false;
        QVariantList entries;
        for (guint i = 0; i < windows->len; ++i) {
            const auto *window = static_cast<const ZavoWindow *>(windows->pdata[i]);
            closingWindowStillExists |= window->id == closingWindow;
            entries.append(QVariantMap{
                {"windowId", QVariant::fromValue<qint64>(window->id)},
                {"name", QString::fromUtf8(window->title)},
                {"appId", QString::fromUtf8(window->description)},
            });
        }
        if (closingWindow && !closingWindowStillExists) {
            closingWindow = 0;
            if (layout.focused && !layout.first_window)
                zavo_show_home();
        }
        if (entries != openWindows) {
            openWindows = entries;
            emit windowsChanged();
        }
    }

    const bool session;
    const bool tracking;
    GPtrArray *const apps;
    QSettings preferences;
    QFileSystemWatcher settingsWatcher;
    QVariantMap appearanceValues;
    QProcess orientationSensor;
    int lastRotation = -1;
    int pendingRotation = 0;
    QTimer rotationDebounce;
    QVariantList openWindows;
    qint64 closingWindow = 0;
    qint64 portraitWorkspace = 0;
    QSize portraitArea;
    bool appliedWide = false;
    ZavoLayout currentLayout{};
    QHash<qint64, bool> windowLayouts;
    QProcess query;
    QProcess radioQuery;
    QProcess systemAction;
    QVariantMap radioStates;
    QTimer timer;
};

static bool showNavigation(QGuiApplication &app, QQmlApplicationEngine &engine, Desktop *desktop)
{
    auto *component = new QQmlComponent(&engine, QUrl(QStringLiteral("qrc:/Navigation.qml")), &engine);
    if (!component->isReady()) {
        qWarning().noquote() << component->errorString();
        return false;
    }

    auto addPanel = [&app, component, desktop](QScreen *screen) {
        QObject *object = component->createWithInitialProperties(
            {{"desktop", QVariant::fromValue(desktop)}});
        auto *window = qobject_cast<QWindow *>(object);
        if (!window) {
            qWarning().noquote() << component->errorString();
            delete object;
            return false;
        }
        object->setParent(component);
        window->setScreen(screen);
        LayerShellQt::Window::get(window)->setScreen(screen);
        QObject::connect(&app, &QGuiApplication::screenRemoved, window, [window, screen](QScreen *removed) {
            if (removed == screen)
                window->deleteLater();
        });
        window->show();
        return true;
    };
    for (QScreen *screen : app.screens()) {
        if (!addPanel(screen))
            return false;
    }
    QObject::connect(&app, &QGuiApplication::screenAdded, component, addPanel);
    return true;
}

int main(int argc, char **argv)
{
    const int systemStatus = zavo_system_command(argc, argv);
    if (systemStatus >= 0)
        return systemStatus;
    const auto hasArg = [argc, argv](const char *option) {
        return std::any_of(argv + 1, argv + argc, [option](const char *arg) { return qstrcmp(arg, option) == 0; });
    };
    const bool maps = hasArg("--maps");
    const bool settings = hasArg("--settings");
    if (maps) {
        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
        QtWebEngineQuick::initialize();
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Zavo DE"));
    QPalette dialogPalette;
    dialogPalette.setColor(QPalette::Window, Qt::white);
    dialogPalette.setColor(QPalette::WindowText, Qt::black);
    dialogPalette.setColor(QPalette::Base, Qt::white);
    dialogPalette.setColor(QPalette::Text, Qt::black);
    dialogPalette.setColor(QPalette::Button, QColor("#f2f2f5"));
    dialogPalette.setColor(QPalette::ButtonText, Qt::black);
    app.setPalette(dialogPalette);
    app.setDesktopFileName(maps ? QStringLiteral("org.zavo.Maps") :
                           settings ? QStringLiteral("org.zavo.Settings") : QStringLiteral("org.zavo.Shell"));
    const bool session = qEnvironmentVariableIsSet("ZAVO_LAUNCHER") && !qEnvironmentVariableIsEmpty("SWAYSOCK");
    if (session || QIcon::themeName().isEmpty())
        QIcon::setThemeName(QStringLiteral("breeze"));
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));
    QStringList iconPaths = QIcon::themeSearchPaths();
    for (const QString &path : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        iconPaths.append(path + QStringLiteral("/icons"));
    iconPaths.removeDuplicates();
    QIcon::setThemeSearchPaths(iconPaths);

    std::unique_ptr<Desktop> desktop;
    QQmlApplicationEngine engine;
    if (!maps) {
        desktop = std::make_unique<Desktop>(session, !settings);
        auto *icons = desktop->createIcons();
        engine.addImageProvider(QStringLiteral("theme"), icons);
        QVariantMap initial{{"desktop", QVariant::fromValue(desktop.get())}};
        if (!settings) {
            initial.insert("installedApps", desktop->installedApps(*icons));
            initial.insert("startFullscreen", app.arguments().contains(QStringLiteral("--fullscreen")));
        }
        engine.setInitialProperties(initial);
    }
    engine.load(QUrl(maps ? QStringLiteral("qrc:/Maps.qml") :
                     settings ? QStringLiteral("qrc:/Settings.qml") : QStringLiteral("qrc:/Shell.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    if (!maps && !settings && session && !showNavigation(app, engine, desktop.get()))
        return 1;
    return app.exec();
}

#include "main.moc"
