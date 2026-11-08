#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ ${1:-} == --session ]]; then
    if ! command -v sway >/dev/null || ! command -v swaymsg >/dev/null; then
        printf 'Install Sway before starting a Zavo DE session.\n' >&2
        exit 1
    fi
    export ZAVO_LAUNCHER="$project_dir/launch.sh"
    export XDG_CURRENT_DESKTOP=sway XDG_SESSION_DESKTOP=zavo XDG_SESSION_TYPE=wayland
    exec dbus-run-session -- env -u SWAYSOCK -u I3SOCK sway --config "$project_dir/sway.conf"
fi

export QT_QUICK_CONTROLS_STYLE=Basic
exec /usr/bin/python3 "$project_dir/main.py" "$@"
