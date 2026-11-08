#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${ZAVO_BUILD_DIR:-$project_dir/build}"
binary="$build_dir/zavo"

build_zavo() {
    cmake -S "$project_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$build_dir" --parallel
}

export QT_QUICK_CONTROLS_STYLE=Basic

case ${1:---session} in
    --build)
        build_zavo
        ;;
    --preview)
        shift
        build_zavo
        exec env -u ZAVO_LAUNCHER "$binary" "$@"
        ;;
    --client)
        if [[ ${ZAVO_LAUNCHER:-} != "$project_dir/launch.sh" || ! -S ${SWAYSOCK:-} ]]; then
            printf 'The Zavo client must be started by its Sway session.\n' >&2
            exit 1
        fi
        if command -v dbus-update-activation-environment >/dev/null; then
            dbus-update-activation-environment WAYLAND_DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP || true
            if [[ -n ${DISPLAY:-} ]]; then
                dbus-update-activation-environment DISPLAY || true
            fi
        fi
        exec "$binary" -platform wayland
        ;;
    --session)
        console="$(tty 2>/dev/null || true)"
        if [[ ! "$console" =~ ^/dev/tty[0-9]+$ || ${EUID} -eq 0 ]]; then
            printf 'Start Zavo DE from a local text console (Ctrl+Alt+F2) as your normal user.\n' >&2
            exit 1
        fi
        if [[ -z ${XDG_RUNTIME_DIR:-} || ! -d "$XDG_RUNTIME_DIR" || ! -O "$XDG_RUNTIME_DIR" ]]; then
            printf 'Log in on the text console to start a local user session.\n' >&2
            exit 1
        fi
        if ! command -v sway >/dev/null || ! command -v swaymsg >/dev/null; then
            printf 'Install Sway before starting a Zavo DE session.\n' >&2
            exit 1
        fi
        build_zavo
        export ZAVO_LAUNCHER="$project_dir/launch.sh" ZAVO_BUILD_DIR="$build_dir"
        export XDG_CURRENT_DESKTOP=sway XDG_SESSION_DESKTOP=zavo XDG_SESSION_TYPE=wayland
        export WLR_BACKENDS=drm,libinput
        exec dbus-run-session -- env -u WAYLAND_DISPLAY -u WAYLAND_SOCKET -u DISPLAY -u XAUTHORITY \
            -u SWAYSOCK -u I3SOCK -u QT_QPA_PLATFORM sway --config "$project_dir/sway.conf"
        ;;
    *)
        printf 'Usage: %s [--session | --build | --preview [--fullscreen]]\n' "$0" >&2
        exit 2
        ;;
esac
