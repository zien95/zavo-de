// SPDX-License-Identifier: GPL-2.0-or-later
#include "desktop.h"

#include <algorithm>
#include <memory>

#include <QFile>
#include <QGuiApplication>
#include <QHash>
#include <QIcon>
#include <QPainter>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVariant>
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
            const bool active = id.startsWith(QStringLiteral("active/"));
            const QString name = id.section(u'/', 1);
            const QIcon icon = QIcon::fromTheme(name + QStringLiteral("-symbolic"), QIcon::fromTheme(name));
            const QColor tint(active ? "#122920" : "#91f5c8");
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

public:
    explicit Desktop(bool inSession) : session(inSession), apps(zavo_apps_new())
    {
        if (!session)
            return;
        connect(&query, &QProcess::finished, this, &Desktop::updateWindows);
        connect(&timer, &QTimer::timeout, this, &Desktop::refresh);
        timer.start(1200);
        refresh();
    }

    ~Desktop() override
    {
        if (query.state() != QProcess::NotRunning) {
            query.kill();
            query.waitForFinished(100);
        }
        g_ptr_array_unref(apps);
    }

    bool inSession() const { return session; }
    QVariantList windows() const { return openWindows; }
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
        return zavo_launch_app(apps, id.toUtf8().constData(), false, session);
    }

    Q_INVOKABLE bool launchPinned(const QString &name)
    {
        return zavo_launch_app(apps, name.toUtf8().constData(), true, session);
    }

    Q_INVOKABLE bool openMaps()
    {
        return zavo_open_maps(QFile::encodeName(QCoreApplication::applicationFilePath()).constData(), session);
    }

    Q_INVOKABLE bool openSetting(const QString &name)
    {
        return zavo_open_setting(name.toUtf8().constData());
    }

    Q_INVOKABLE bool focusWindow(qint64 id) { return session && zavo_focus_window(id); }
    Q_INVOKABLE bool closeWindow(qint64 id) { return session && zavo_close_window(id); }
    Q_INVOKABLE bool endSession() { return session && zavo_end_session(); }

signals:
    void windowsChanged();

private:
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
        g_autoptr(GPtrArray) windows = zavo_windows_parse(json.constData(), json.size());
        if (!windows)
            return;
        QVariantList entries;
        for (guint i = 0; i < windows->len; ++i) {
            const auto *window = static_cast<const ZavoWindow *>(windows->pdata[i]);
            entries.append(QVariantMap{
                {"windowId", QVariant::fromValue<qint64>(window->id)},
                {"name", QString::fromUtf8(window->title)},
                {"appId", QString::fromUtf8(window->description)},
            });
        }
        if (entries != openWindows) {
            openWindows = entries;
            emit windowsChanged();
        }
    }

    const bool session;
    GPtrArray *const apps;
    QVariantList openWindows;
    QProcess query;
    QTimer timer;
};

int main(int argc, char **argv)
{
    const bool maps = std::find_if(argv + 1, argv + argc, [](const char *arg) {
        return qstrcmp(arg, "--maps") == 0;
    }) != argv + argc;
    if (maps) {
        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
        QtWebEngineQuick::initialize();
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Zavo DE"));
    app.setDesktopFileName(maps ? QStringLiteral("org.zavo.Maps") : QStringLiteral("org.zavo.Shell"));
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
        desktop = std::make_unique<Desktop>(session);
        auto *icons = desktop->createIcons();
        engine.addImageProvider(QStringLiteral("theme"), icons);
        engine.setInitialProperties({
            {"desktop", QVariant::fromValue(desktop.get())},
            {"installedApps", desktop->installedApps(*icons)},
            {"startFullscreen", app.arguments().contains(QStringLiteral("--fullscreen"))},
        });
    }
    engine.load(QUrl(maps ? QStringLiteral("qrc:/Maps.qml") : QStringLiteral("qrc:/Shell.qml")));
    return engine.rootObjects().isEmpty() ? 1 : app.exec();
}

#include "main.moc"
