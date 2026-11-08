# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from urllib.parse import quote, unquote

from gi.repository import Gio, GioUnix, GLib
from PyQt6.QtCore import (
    QObject, QProcess, QSize, QStandardPaths, Qt, QTimer, QUrl, pyqtProperty, pyqtSignal, pyqtSlot,
)
from PyQt6.QtGui import QColor, QGuiApplication, QIcon, QPainter, QPixmap
from PyQt6.QtQml import QQmlApplicationEngine
from PyQt6.QtQuick import QQuickImageProvider


class AppIcons(QQuickImageProvider):
    def __init__(self, icons):
        super().__init__(QQuickImageProvider.ImageType.Pixmap)
        self.icons = icons

    def requestPixmap(self, identifier, requested_size):
        icon = self.icons.get(unquote(identifier), QIcon())
        size = requested_size if requested_size.isValid() else QSize(48, 48)
        return icon.pixmap(size), size


class ThemeIcons(QQuickImageProvider):
    def __init__(self):
        super().__init__(QQuickImageProvider.ImageType.Pixmap)

    def requestPixmap(self, identifier, requested_size):
        style, _, name = identifier.partition("/")
        icon = QIcon.fromTheme(name + "-symbolic", QIcon.fromTheme(name))
        size = requested_size if requested_size.isValid() else QSize(44, 44)
        pixmap = icon.pixmap(size)
        color = QColor("#122920" if style == "active" else "#91f5c8")
        if pixmap.isNull():
            pixmap = QPixmap(size)
            pixmap.fill(Qt.GlobalColor.transparent)
            painter = QPainter(pixmap)
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(color)
            painter.drawRoundedRect(pixmap.rect().adjusted(10, 10, -10, -10), 5, 5)
        else:
            painter = QPainter(pixmap)
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceIn)
            painter.fillRect(pixmap.rect(), color)
        painter.end()
        return pixmap, size


class Desktop(QObject):
    windowsChanged = pyqtSignal()

    pinned = (
        ("Terminal", "utilities-terminal", ("org.kde.qmlkonsole",)),
        ("Files", "folder", ("org.kde.dolphin",)),
        ("Settings", "preferences-system", ()),
        ("Software", "system-software-install", ("org.kde.discover",)),
        ("Firefox", "internet-web-browser", ("firefox", "firefox_firefox", "org.mozilla.firefox")),
        ("LibreOffice", "libreoffice-startcenter", ("libreoffice-startcenter", "org.libreoffice.LibreOffice")),
        ("GIMP", "gimp", ("gimp", "org.gimp.GIMP")),
        ("Inkscape", "inkscape", ("org.inkscape.Inkscape", "inkscape")),
        ("VLC", "vlc", ("vlc", "org.videolan.VLC")),
        ("Maps", "mark-location", ()),
        ("Contacts", "x-office-address-book", ("org.kde.merkuro.contact", "org.kde.kaddressbook")),
        ("Calendar", "x-office-calendar", ("org.kde.merkuro.calendar",)),
        ("Camera", "camera-photo", ("org.kde.plasma.camera",)),
        ("Photos", "folder-pictures", ("org.kde.koko", "org.kde.gwenview")),
        ("Music", "audio-x-generic", ("org.kde.elisa",)),
        ("Notes", "accessories-text-editor", ("org.kde.kwrite",)),
    )

    settings = {
        "Network": "kcm_networkmanagement",
        "Bluetooth": "kcm_bluetooth",
        "Sound": "kcm_pulseaudio",
        "Display": "kcm_kscreen",
        "Power": "kcm_powerdevilprofilesconfig",
        "Notifications": "kcm_notifications",
    }

    def __init__(self):
        super().__init__()
        self.in_session = bool(os.environ.get("ZAVO_LAUNCHER") and os.environ.get("SWAYSOCK"))
        self.swaymsg = QStandardPaths.findExecutable("swaymsg") if self.in_session else ""
        self._windows = []
        if self.swaymsg:
            self.tree_query = QProcess(self)
            self.tree_query.finished.connect(self.read_windows)
            self.timer = QTimer(self, interval=1200)
            self.timer.timeout.connect(self.refresh_windows)
            self.timer.start()
            self.refresh_windows()

        self.apps = {
            app.get_id(): app for app in Gio.AppInfo.get_all()
            if app.get_id() and app.should_show()
        }
        self.icons = {}
        self.entries = []
        self.pinned_apps = {}
        used_app_ids = set()
        for label, icon, candidates in self.pinned:
            for candidate in candidates:
                app_id = candidate + ".desktop"
                app = self.desktop_app(app_id)
                if app is not None:
                    self.pinned_apps[label] = app
                    used_app_ids.update(other + ".desktop" for other in candidates)
                    break
            app = self.pinned_apps.get(label)
            self.entries.append(self.app_entry(label, app.get_id() if app else f"zavo.{label}",
                                               self.app_icon(app) if app else QIcon(), icon, True))
        for app_id, app in sorted(
            self.apps.items(), key=lambda entry: entry[1].get_display_name().casefold()
        ):
            if app_id in used_app_ids:
                continue
            self.entries.append(self.app_entry(app.get_display_name(), app_id, self.app_icon(app),
                                               "application-x-executable", False))

    def app_entry(self, label, app_id, icon, fallback, shortcut):
        if not icon.isNull():
            self.icons[app_id] = icon
        return {
            "appId": app_id,
            "name": label,
            "iconUrl": "image://apps/" + quote(app_id, safe="") if app_id in self.icons else "",
            "iconName": fallback,
            "shortcut": shortcut,
        }

    @pyqtProperty(bool, constant=True)
    def inSession(self):
        return self.in_session

    @pyqtProperty("QVariantList", notify=windowsChanged)
    def windows(self):
        return self._windows

    def refresh_windows(self):
        if self.tree_query.state() == QProcess.ProcessState.NotRunning:
            self.tree_query.start(self.swaymsg, ["-r", "-t", "get_tree"])

    @staticmethod
    def open_windows(node):
        app_id = node.get("app_id")
        if node.get("shell") in ("xdg_shell", "xwayland") and app_id != "org.zavo.Shell":
            protocol = "Wayland" if node["shell"] == "xdg_shell" else "XWayland"
            yield {
                "windowId": node["id"],
                "name": node.get("name") or app_id or "Untitled window",
                "appId": f"{protocol} · {app_id}" if app_id else protocol,
            }
        for child in node.get("nodes", []) + node.get("floating_nodes", []):
            yield from Desktop.open_windows(child)

    def read_windows(self, exit_code, _status):
        if exit_code != 0:
            return
        try:
            tree = json.loads(bytes(self.tree_query.readAllStandardOutput()))
        except ValueError:
            return
        windows = list(self.open_windows(tree))
        if windows != self._windows:
            self._windows = windows
            self.windowsChanged.emit()

    def sway_command(self, command):
        if not self.swaymsg:
            return False
        try:
            result = subprocess.run(
                [self.swaymsg, "-q", "--", command], timeout=1, check=False,
            )
            return result.returncode == 0
        except (OSError, subprocess.TimeoutExpired):
            return False

    def launch_on_desktop(self, launch):
        if self.in_session and not self.sway_command("workspace 2:Windows"):
            return False
        try:
            if launch():
                return True
        except GLib.Error as error:
            print(error.message, file=sys.stderr)
        if self.in_session:
            self.sway_command("workspace 1:Home")
        return False

    @pyqtSlot(int, result=bool)
    def focusWindow(self, window_id):
        return self.sway_command(f"[con_id={window_id}] focus")

    @pyqtSlot(int, result=bool)
    def closeWindow(self, window_id):
        return self.sway_command(f"[con_id={window_id}] kill")

    @pyqtSlot(result=bool)
    def endSession(self):
        return self.sway_command("exit")

    @staticmethod
    def app_icon(app):
        icon = app.get_icon()
        if isinstance(icon, Gio.FileIcon):
            path = icon.get_file().get_path()
            if path:
                return QIcon(path)
        if isinstance(icon, Gio.ThemedIcon):
            for name in icon.get_names():
                themed_icon = QIcon.fromTheme(name)
                if not themed_icon.isNull():
                    return themed_icon
        return QIcon()

    @staticmethod
    def desktop_app(app_id):
        try:
            return GioUnix.DesktopAppInfo.new(app_id)
        except TypeError:
            return None

    @pyqtSlot(str, result=bool)
    def launch(self, app_id):
        app = self.apps.get(app_id)
        return app is not None and self.launch_on_desktop(lambda: app.launch([], None))

    @pyqtSlot(str, result=bool)
    def launchPinned(self, label):
        app = self.pinned_apps.get(label)
        return app is not None and self.launch_on_desktop(lambda: app.launch([], None))

    @pyqtSlot(result=bool)
    def openMaps(self):
        launcher = Path(__file__).with_name("launch.sh")
        return self.launch_on_desktop(
            lambda: QProcess.startDetached("/bin/bash", [str(launcher), "--maps"])[0]
        )

    @pyqtSlot(str, result=bool)
    def openSetting(self, label):
        module = self.settings.get(label)
        command = QStandardPaths.findExecutable("kcmshell6")
        if not module or not command:
            return False
        started, _ = QProcess.startDetached(command, [module])
        return started


def main():
    parser = argparse.ArgumentParser(description="Zavo DE touch shell")
    parser.add_argument("--fullscreen", action="store_true")
    parser.add_argument("--maps", action="store_true", help=argparse.SUPPRESS)
    options = parser.parse_args()

    if options.maps:
        QGuiApplication.setAttribute(Qt.ApplicationAttribute.AA_ShareOpenGLContexts)

    qt_args = [sys.argv[0]]
    if os.environ.get("ZAVO_LAUNCHER"):
        qt_args += ["-platform", "wayland"]
    app = QGuiApplication(qt_args)
    app.setApplicationName("Zavo DE")
    app.setDesktopFileName("org.zavo.Maps" if options.maps else "org.zavo.Shell")
    QIcon.setThemeSearchPaths([
        *QIcon.themeSearchPaths(), str(Path.home() / ".local/share/icons"), "/usr/share/icons",
    ])
    if os.environ.get("ZAVO_LAUNCHER") or not QIcon.themeName():
        QIcon.setThemeName("breeze")
    QIcon.setFallbackThemeName("hicolor")

    engine = QQmlApplicationEngine()
    if not options.maps:
        desktop = Desktop()
        engine.addImageProvider("apps", AppIcons(desktop.icons))
        engine.addImageProvider("theme", ThemeIcons())
        engine.setInitialProperties({
            "desktop": desktop,
            "installedApps": desktop.entries,
            "startFullscreen": options.fullscreen,
        })
    filename = "Maps.qml" if options.maps else "Shell.qml"
    engine.load(QUrl.fromLocalFile(str(Path(__file__).with_name(filename))))
    if not engine.rootObjects():
        return 1
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
