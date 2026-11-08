# Zavo DE

Zavo DE is a touch-first Qt 6 shell. QML defines the interface, C implements app
launching and Wayland window management, and a C++ bridge connects the C backend
to Qt. Sway runs the Wayland session directly on Linux display and input devices.

## Build and start

Install the build and runtime packages on Ubuntu or Kubuntu 26.04:

```bash
sudo apt install build-essential cmake pkg-config libglib2.0-dev libjson-glib-dev \
  qt6-base-dev qt6-declarative-dev qt6-webengine-dev qt6-wayland qt6-svg-plugins \
  qml6-module-qtmultimedia kf6-breeze-icon-theme sway xwayland dbus-daemon
```

Build from the checkout. This step also works from a terminal inside Plasma:

```bash
/bin/bash ./launch.sh --build
```

Press **Ctrl+Alt+F2** to reach a text console, log in as a regular user, then run
this command from the checkout:

```bash
/bin/bash ./launch.sh
```

The default launcher requires a local text console. It starts a fresh Wayland
session using the DRM and libinput backends. The command also accepts
`--session` for scripts that used the previous launch syntax.

Swipe between pages or tap the bottom buttons. Tap the search field to use the
touch keyboard. In the Sway workspace bar, tap **Home** to return to the shell.
The Apps page lists native Wayland and XWayland windows and lets you switch to or
close them. To leave Zavo, use **Quick Settings → End session** or
**Super+Shift+E**. Switch back to the virtual terminal where your original
desktop is running.

For a windowed preview inside another desktop, explicitly run
`/bin/bash ./launch.sh --preview`.

## Pinned apps

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

## License

The Zavo DE source is available under [GPL version 2 or later](LICENSE). Qt and
the other system dependencies retain their respective licenses.
