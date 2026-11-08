# Zavo DE

Zavo DE is a touch-first Linux shell built with Qt 6 and QML. It has a swipeable
home screen, an app drawer with a touch keyboard, and quick settings. It can run
in a window on Kubuntu or start its own Wayland session with Sway.

## Install the runtime

On Ubuntu or Kubuntu 26.04:

```bash
sudo apt install python3-pyqt6.qtquick python3-gi gir1.2-glib-2.0 \
  qt6-wayland qt6-svg-plugins qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-window \
  qml6-module-qtqml-workerscript qml6-module-qtmultimedia \
  qml6-module-qtwebengine kf6-breeze-icon-theme
```

The pinned shortcuts open installed applications. To add the requested apps on
Ubuntu 26.04:

```bash
sudo apt install qmlkonsole dolphin plasma-discover firefox libreoffice \
  gimp inkscape vlc merkuro plasma-camera koko elisa kwrite
```

Terminal uses KDE's QMLKonsole. Files uses standard Dolphin. Settings opens Zavo
Quick Settings and Software opens Discover. Contacts and Calendar use Merkuro.
Camera uses Plasma Camera when installed, with a built-in live preview as a
fallback. Photos prefers Koko and falls back to Gwenview. Notes opens KWrite.
Google Maps opens in a separate Qt WebEngine window with a touch search field.

## Preview on Kubuntu

```bash
/bin/bash ./launch.sh
```

Swipe between pages or tap the bottom buttons. Tap the search field to type with
the on-screen keyboard. Use the button in the top right or F11 for fullscreen.

## Start a standalone Wayland session

```bash
sudo apt install sway xwayland dbus-daemon
```

Log in as a regular user on a local TTY, change to the checkout, and run:

```bash
/bin/bash ./launch.sh --session
```

Sway handles the display and touchscreen. Home is always available in the
compositor's workspace bar while ordinary app windows are open. The Apps page
lists open windows by protocol and lets you switch to or close them. Wayland
apps run natively and XWayland can host older X11 apps. To leave, open **Quick
Settings → End session**. The keyboard shortcut is **Super+Shift+E**.

You can also run the same command inside a Wayland desktop to test it in a nested
compositor. Running windows in the Apps page belong to that Zavo session.

## License

Zavo DE source is available under [GPL version 2 or later](LICENSE). PyQt6's
GPLv3 terms apply when distributing a combined build that includes PyQt6.
