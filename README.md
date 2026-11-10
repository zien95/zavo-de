# Zavo DE

Zavo DE is a touch-first Qt 6 shell. QML defines the interface, C implements app
launching and Wayland window management, and a C++ bridge connects the C backend
to Qt. Sway runs the Wayland session directly on Linux display and input devices.

## Build and start

Install the build and runtime packages on Ubuntu or Kubuntu 26.04:

```bash
sudo apt install build-essential cmake pkg-config libglib2.0-dev libjson-glib-dev \
  qt6-base-dev qt6-declarative-dev qt6-webengine-dev qt6-wayland qt6-svg-plugins \
  liblayershellqtinterface-dev qml6-module-qtmultimedia qml6-module-qtquick-dialogs \
  qml6-module-org-kde-layershell layer-shell-qt kf6-breeze-icon-theme sway xwayland dbus-daemon
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
touch keyboard. Choose **Settings → Display → Interface layout** to select
**Auto**, **Phone**, or **Tablet**. Auto uses the tablet layout when both usable
screen dimensions are at least 660 logical pixels. Tablet mode expands the app
grid and Quick Settings, and opens apps wide in either orientation. Phone mode
opens apps in a centered portrait workspace on wide displays.
Firefox, Cubic, and Minecraft Launcher start wide so their desktop controls
have room. The bottom gesture line stays available over fullscreen apps.

- Tap, left-click, or swipe up quickly on the line to close the foreground app.
- Swipe up slowly or right-click the line to show active apps. Tap an app to
  switch to it, tap its **×** to close it, or tap **Home** to return to the shell.
  The switcher can toggle the foreground app between portrait and wide layout.

Quick Settings controls Wi-Fi and mobile data through NetworkManager, Bluetooth
through BlueZ, and GPS through a supported ModemManager GPS device. Tiles show
when the radio is unavailable. It also shows battery level and power saver when
UPower and Power Profiles Daemon are available. Brightness controls the built-in
backlight, and the volume slider uses WirePlumber and plays a short cue as you drag.

Open **Settings** for accent color, wallpaper, cursor theme and size, and shell
animations. Under **Display**, choose an orientation or use **Rotation lock**
to hold the current orientation. Unlock it for automatic rotation on devices
with an accelerometer supported by `iio-sensor-proxy`.

**Quick Settings → Power** offers suspend, restart, shutdown, and **End session**,
each with a confirmation. You can also leave Zavo with **Super+Shift+E**.

For a windowed preview inside another desktop, explicitly run
`/bin/bash ./launch.sh --preview`.

## Pinned apps

The pinned shortcuts open installed applications. To add the requested apps on
Ubuntu 26.04:

```bash
sudo apt install qmlkonsole dolphin plasma-discover firefox libreoffice \
  gimp inkscape vlc merkuro plasma-camera koko elisa kwrite
```

Terminal prefers KDE's QMLKonsole and falls back to Konsole or Foot. Files uses
Dolphin, Settings opens the Zavo Settings app, and Software opens Discover.
Contacts and Calendar use Merkuro. Camera uses Plasma Camera when installed.
The built-in camera, also available from Quick Settings, has a **Take photo**
button that saves JPEGs in Pictures. Photos prefers Koko and falls back to
Gwenview. Notes opens KWrite. Google Maps opens in a separate Qt WebEngine
window with a touch search field.

Firefox uses a separate profile inside the standalone session so it can open
alongside Firefox in another desktop session. Snap apps use the systemd user
bus to start their app scope.

## License

The Zavo DE source is available under [GPL version 2 or later](LICENSE). Qt and
the other system dependencies retain their respective licenses.
