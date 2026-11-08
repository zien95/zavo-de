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
    char *output;
} ZavoWindow;

typedef struct {
    gint64 first_window;
    gint64 focused_window;
    gint64 active_window;
    gint64 workspace_id;
    int width;
    int height;
    gboolean focused;
    gboolean active_visible;
    gboolean wide_default;
} ZavoLayout;

GPtrArray *zavo_apps_new(void);
GPtrArray *zavo_windows_parse(const char *data, gssize length, ZavoLayout *layout);
gboolean zavo_apply_layout(const ZavoLayout *layout, gboolean wide);
gboolean zavo_set_cursor(const char *theme, int size);
int zavo_current_rotation(void);
gboolean zavo_rotate_display(int rotation);
gint64 zavo_close_focused_window(const char *output);
gboolean zavo_show_home(void);
gboolean zavo_launch_app(const GPtrArray *apps, const char *key, gboolean shortcut,
                         gboolean in_session);
gboolean zavo_open_builtin(const char *executable, const char *mode, gboolean in_session);
gboolean zavo_focus_window(gint64 id);
gboolean zavo_close_window(gint64 id);
gboolean zavo_end_session(void);

G_END_DECLS
