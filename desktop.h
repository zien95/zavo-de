// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct {
    char *id;
    char *name;
    const char *fallback_icon;
    GAppInfo *info;
    gboolean shortcut;
} ZavoApp;

typedef struct {
    gint64 id;
    char *title;
    char *description;
} ZavoWindow;

GPtrArray *zavo_apps_new(void);
GPtrArray *zavo_windows_parse(const char *data, gssize length);
gboolean zavo_launch_app(const GPtrArray *apps, const char *key, gboolean shortcut,
                         gboolean in_session);
gboolean zavo_open_maps(const char *executable, gboolean in_session);
gboolean zavo_open_setting(const char *name);
gboolean zavo_focus_window(gint64 id);
gboolean zavo_close_window(gint64 id);
gboolean zavo_end_session(void);

G_END_DECLS
