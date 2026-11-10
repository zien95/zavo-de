// SPDX-License-Identifier: GPL-2.0-or-later
#include "desktop.h"

#include <gio/gdesktopappinfo.h>
#include <json-glib/json-glib.h>
#include <string.h>

static const struct {
    const char *name;
    const char *icon;
    const char *ids[5];
} shortcuts[] = {
    { "Terminal", "utilities-terminal", { "org.kde.qmlkonsole", "org.kde.konsole", "foot" } },
    { "Files", "folder", { "org.kde.dolphin" } },
    { "Settings", "preferences-system", { NULL } },
    { "Software", "system-software-install", { "org.kde.discover" } },
    { "Firefox", "internet-web-browser", { "firefox", "firefox_firefox", "org.mozilla.firefox" } },
    { "LibreOffice", "libreoffice-startcenter", { "libreoffice-startcenter", "org.libreoffice.LibreOffice" } },
    { "GIMP", "gimp", { "gimp", "org.gimp.GIMP" } },
    { "Inkscape", "inkscape", { "org.inkscape.Inkscape", "inkscape" } },
    { "VLC", "vlc", { "vlc", "org.videolan.VLC" } },
    { "Maps", "mark-location", { NULL } },
    { "Contacts", "x-office-address-book", { "org.kde.merkuro.contact", "org.kde.kaddressbook" } },
    { "Calendar", "x-office-calendar", { "org.kde.merkuro.calendar" } },
    { "Camera", "camera-photo", { "org.kde.plasma.camera" } },
    { "Photos", "folder-pictures", { "org.kde.koko", "org.kde.gwenview" } },
    { "Music", "audio-x-generic", { "org.kde.elisa" } },
    { "Notes", "accessories-text-editor", { "org.kde.kwrite" } },
};

static void free_app(gpointer data)
{
    ZavoApp *app = data;
    g_free(app->id);
    g_free(app->name);
    g_clear_object(&app->info);
    g_free(app);
}

static gint compare_apps(gconstpointer a, gconstpointer b)
{
    return g_utf8_collate(g_app_info_get_display_name(G_APP_INFO(a)),
                          g_app_info_get_display_name(G_APP_INFO(b)));
}

GPtrArray *zavo_apps_new(void)
{
    GPtrArray *apps = g_ptr_array_new_with_free_func(free_app);
    g_autoptr(GHashTable) pinned_ids = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    for (gsize i = 0; i < G_N_ELEMENTS(shortcuts); ++i) {
        ZavoApp *app = g_new0(ZavoApp, 1);
        app->name = g_strdup(shortcuts[i].name);
        app->fallback_icon = shortcuts[i].icon;
        app->shortcut = TRUE;
        for (gsize j = 0; shortcuts[i].ids[j]; ++j) {
            char *id = g_strconcat(shortcuts[i].ids[j], ".desktop", NULL);
            if (!app->info)
                app->info = (GAppInfo *)g_desktop_app_info_new(id);
            g_hash_table_add(pinned_ids, id);
        }
        app->id = app->info ? g_strdup(g_app_info_get_id(app->info))
                            : g_strconcat("zavo.", app->name, NULL);
        g_ptr_array_add(apps, app);
    }

    GList *installed = g_list_sort(g_app_info_get_all(), compare_apps);
    for (GList *entry = installed; entry; entry = entry->next) {
        GAppInfo *info = entry->data;
        const char *id = g_app_info_get_id(info);
        if (!id || g_hash_table_contains(pinned_ids, id) || !g_app_info_should_show(info))
            continue;
        ZavoApp *app = g_new0(ZavoApp, 1);
        app->id = g_strdup(id);
        app->name = g_strdup(g_app_info_get_display_name(info));
        app->fallback_icon = "application-x-executable";
        app->info = g_object_ref(info);
        g_ptr_array_add(apps, app);
    }
    g_list_free_full(installed, g_object_unref);
    return apps;
}

static void free_window(gpointer data)
{
    ZavoWindow *window = data;
    g_free(window->title);
    g_free(window->description);
    g_free(window->output);
    g_free(window);
}

static gboolean prefers_wide(const char *identifier)
{
    g_autofree char *name = identifier ? g_ascii_strdown(identifier, -1) : NULL;
    return name && (strstr(name, "firefox") || strstr(name, "cubic") || strstr(name, "minecraft"));
}

static void collect_windows(JsonObject *node, GPtrArray *windows, ZavoLayout *layout,
                            gboolean in_apps, int output_width, const char *output_name)
{
    if (!node)
        return;
    const char *type = json_object_get_string_member_with_default(node, "type", NULL);
    if (g_strcmp0(type, "output") == 0) {
        output_name = json_object_get_string_member_with_default(node, "name", NULL);
        JsonObject *rect = json_object_get_object_member(node, "rect");
        output_width = rect ? json_object_get_int_member_with_default(rect, "width", 0) : 0;
    }
    if (g_strcmp0(type, "workspace") == 0 &&
        g_strcmp0(json_object_get_string_member_with_default(node, "name", NULL), "2:Windows") == 0) {
        in_apps = TRUE;
        layout->workspace_id = json_object_get_int_member_with_default(node, "id", 0);
        JsonObject *rect = json_object_get_object_member(node, "rect");
        if (rect) {
            layout->width = output_width > 0 ? output_width :
                json_object_get_int_member_with_default(rect, "width", 0);
            layout->height = json_object_get_int_member_with_default(rect, "height", 0);
        }
    }
    const gboolean focused = json_object_get_boolean_member_with_default(node, "focused", FALSE);
    if (in_apps && focused)
        layout->focused = TRUE;
    const char *app_id = json_object_get_string_member_with_default(node, "app_id", NULL);
    const char *shell = json_object_get_string_member_with_default(node, "shell", NULL);
    const gboolean wayland = g_strcmp0(shell, "xdg_shell") == 0;
    const gboolean xwayland = g_strcmp0(shell, "xwayland") == 0;
    if ((wayland || xwayland) && g_strcmp0(app_id, "org.zavo.Shell") != 0) {
        const char *title = json_object_get_string_member_with_default(node, "name", NULL);
        const char *protocol = wayland ? "Wayland" : "XWayland";
        ZavoWindow *window = g_new0(ZavoWindow, 1);
        window->id = json_object_get_int_member_with_default(node, "id", 0);
        window->output = g_strdup(output_name);
        if (in_apps && layout->first_window == 0)
            layout->first_window = window->id;
        if (focused)
            layout->focused_window = window->id;
        const gboolean visible = json_object_get_boolean_member_with_default(node, "visible", FALSE);
        if (in_apps && (focused || !layout->active_window || (visible && !layout->active_visible))) {
            JsonObject *xprops = json_object_get_object_member(node, "window_properties");
            const char *xclass = xprops ? json_object_get_string_member_with_default(xprops, "class", NULL) : NULL;
            layout->active_window = window->id;
            layout->active_visible = visible || focused;
            layout->wide_default = prefers_wide(app_id ? app_id : xclass ? xclass : title);
        }
        window->title = g_strdup(title && *title ? title : app_id ? app_id : "Untitled window");
        window->description = app_id ? g_strdup_printf("%s · %s", protocol, app_id)
                                     : g_strdup(protocol);
        g_ptr_array_add(windows, window);
    }

    const char *collections[] = { "nodes", "floating_nodes" };
    for (gsize i = 0; i < G_N_ELEMENTS(collections); ++i) {
        JsonArray *children = json_object_get_array_member(node, collections[i]);
        if (!children)
            continue;
        for (guint j = 0; j < json_array_get_length(children); ++j)
            collect_windows(json_array_get_object_element(children, j), windows, layout, in_apps, output_width, output_name);
    }
}

GPtrArray *zavo_windows_parse(const char *data, gssize length, ZavoLayout *layout)
{
    *layout = (ZavoLayout){0};
    g_autoptr(JsonParser) parser = json_parser_new();
    if (!json_parser_load_from_data(parser, data, length, NULL))
        return NULL;
    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_OBJECT(root))
        return NULL;
    GPtrArray *windows = g_ptr_array_new_with_free_func(free_window);
    collect_windows(json_node_get_object(root), windows, layout, FALSE, 0, NULL);
    return windows;
}

enum { SWAY_COMMAND = 0, SWAY_GET_OUTPUTS = 3, SWAY_GET_TREE = 4, SWAY_GET_INPUTS = 100 };

static gboolean sway_exchange(guint32 type, const char *command, gboolean allow_disconnect,
                               char **reply, gsize *reply_size)
{
    *reply = NULL;
    *reply_size = 0;
    const char *path = g_getenv("SWAYSOCK");
    if (!path || !*path)
        return FALSE;

    g_autoptr(GSocketClient) client = g_socket_client_new();
    g_socket_client_set_timeout(client, 1);
    g_autoptr(GSocketAddress) address = g_unix_socket_address_new(path);
    g_autoptr(GSocketConnection) connection =
        g_socket_client_connect(client, G_SOCKET_CONNECTABLE(address), NULL, NULL);
    if (!connection)
        return FALSE;

    /* Sway IPC header: magic, little-endian length and message type. */
    guint8 header[14] = { 'i', '3', '-', 'i', 'p', 'c' };
    const gsize length = strlen(command);
    guint32 encoded_length = GUINT32_TO_LE(length);
    memcpy(header + 6, &encoded_length, sizeof encoded_length);
    const guint32 encoded_type = GUINT32_TO_LE(type);
    memcpy(header + 10, &encoded_type, sizeof encoded_type);
    GIOStream *stream = G_IO_STREAM(connection);
    GOutputStream *output = g_io_stream_get_output_stream(stream);
    if (!g_output_stream_write_all(output, header, sizeof header, NULL, NULL, NULL) ||
        !g_output_stream_write_all(output, command, length, NULL, NULL, NULL))
        return FALSE;
    GInputStream *input = g_io_stream_get_input_stream(stream);
    gsize received = 0;
    if (!g_input_stream_read_all(input, header, sizeof header, &received, NULL, NULL))
        return FALSE;
    if (allow_disconnect && received == 0)
        return TRUE;
    if (received != sizeof header || memcmp(header, "i3-ipc", 6) != 0)
        return FALSE;
    memcpy(&encoded_length, header + 6, sizeof encoded_length);
    const guint32 reply_length = GUINT32_FROM_LE(encoded_length);
    if (reply_length == 0 || reply_length > 4 * 1024 * 1024)
        return FALSE;
    g_autofree char *buffer = g_malloc(reply_length);
    if (!g_input_stream_read_all(input, buffer, reply_length, &received, NULL, NULL) ||
        received != reply_length)
        return FALSE;
    *reply = g_steal_pointer(&buffer);
    *reply_size = reply_length;
    return TRUE;
}

static gboolean sway_command(const char *command, gboolean allow_disconnect)
{
    g_autofree char *reply = NULL;
    gsize length;
    if (!sway_exchange(SWAY_COMMAND, command, allow_disconnect, &reply, &length))
        return FALSE;
    if (allow_disconnect && !reply)
        return TRUE;
    g_autoptr(JsonParser) parser = json_parser_new();
    if (!json_parser_load_from_data(parser, reply, length, NULL))
        return FALSE;
    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_ARRAY(root))
        return FALSE;
    JsonArray *results = json_node_get_array(root);
    return json_array_get_length(results) > 0 &&
           json_object_get_boolean_member_with_default(json_array_get_object_element(results, 0),
                                                       "success", FALSE);
}

static GPtrArray *read_layout(ZavoLayout *layout)
{
    g_autofree char *reply = NULL;
    gsize length;
    if (!sway_exchange(SWAY_GET_TREE, "", FALSE, &reply, &length))
        return NULL;
    return zavo_windows_parse(reply, length, layout);
}

gboolean zavo_tablet_layout(const char *mode, int width, int height)
{
    if (g_strcmp0(mode, "tablet") == 0)
        return TRUE;
    return g_strcmp0(mode, "phone") != 0 && MIN(width, height) >= 660;
}

gboolean zavo_apply_layout(const ZavoLayout *layout, gboolean wide)
{
    if (!layout->workspace_id || !layout->width || !layout->height)
        return FALSE;
    const int portrait_width = MAX(310, layout->height * 9 / 16);
    const int gap = wide ? 0 : MAX(0, (layout->width - portrait_width) / 2);
    g_autofree char *command = layout->first_window
        ? g_strdup_printf("[con_id=%" G_GINT64_FORMAT "] gaps horizontal current set %d",
                          layout->first_window, gap)
        : g_strdup_printf("gaps horizontal current set %d", gap);
    return (layout->first_window || layout->focused) && sway_command(command, FALSE);
}

gboolean zavo_set_cursor(const char *theme, int size)
{
    if (!theme || !*theme || size < 16 || size > 64 ||
        strspn(theme, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.- ") != strlen(theme))
        return FALSE;
    g_autofree char *command = g_strdup_printf("seat * xcursor_theme \"%s\" %d", theme, size);
    return sway_command(command, FALSE);
}

static JsonObject *built_in_output(JsonArray *outputs)
{
    JsonObject *first_active = NULL;
    for (guint i = 0; i < json_array_get_length(outputs); ++i) {
        JsonObject *output = json_array_get_object_element(outputs, i);
        if (!json_object_get_boolean_member_with_default(output, "active", FALSE))
            continue;
        const char *name = json_object_get_string_member_with_default(output, "name", "");
        if (g_str_has_prefix(name, "eDP") || g_str_has_prefix(name, "DSI") ||
            g_str_has_prefix(name, "LVDS"))
            return output;
        if (!first_active)
            first_active = output;
    }
    return first_active;
}

static JsonObject *read_output(JsonParser *parser)
{
    g_autofree char *reply = NULL;
    gsize length;
    if (!sway_exchange(SWAY_GET_OUTPUTS, "", FALSE, &reply, &length) ||
        !json_parser_load_from_data(parser, reply, length, NULL))
        return NULL;
    JsonNode *root = json_parser_get_root(parser);
    return JSON_NODE_HOLDS_ARRAY(root) ? built_in_output(json_node_get_array(root)) : NULL;
}

static void map_single_touchscreen(const char *output)
{
    g_autofree char *reply = NULL;
    gsize length;
    g_autoptr(JsonParser) parser = json_parser_new();
    if (!sway_exchange(SWAY_GET_INPUTS, "", FALSE, &reply, &length) ||
        !json_parser_load_from_data(parser, reply, length, NULL))
        return;
    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_ARRAY(root))
        return;
    JsonArray *inputs = json_node_get_array(root);
    const char *touchscreen = NULL;
    for (guint i = 0; i < json_array_get_length(inputs); ++i) {
        JsonObject *input = json_array_get_object_element(inputs, i);
        if (!g_str_equal(json_object_get_string_member_with_default(input, "type", ""), "touch"))
            continue;
        if (touchscreen)
            return;
        touchscreen = json_object_get_string_member_with_default(input, "identifier", NULL);
    }
    if (touchscreen) {
        g_autofree char *quoted = g_strescape(touchscreen, NULL);
        g_autofree char *command = g_strdup_printf("input \"%s\" map_to_output %s", quoted, output);
        sway_command(command, FALSE);
    }
}

int zavo_current_rotation(void)
{
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonObject *output = read_output(parser);
    if (!output)
        return -1;
    const char *transform = json_object_get_string_member_with_default(output, "transform", "normal");
    return g_str_equal(transform, "90") ? 90 : g_str_equal(transform, "180") ? 180 :
           g_str_equal(transform, "270") ? 270 : 0;
}

gboolean zavo_rotate_display(int rotation)
{
    if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270)
        return FALSE;
    g_autoptr(JsonParser) parser = json_parser_new();
    JsonObject *output = read_output(parser);
    if (!output)
        return FALSE;
    const char *name = json_object_get_string_member_with_default(output, "name", "");
    if (!*name || strspn(name, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-") != strlen(name))
        return FALSE;
    g_autofree char *value = rotation ? g_strdup_printf("%d", rotation) : g_strdup("normal");
    const gboolean changed = g_strcmp0(json_object_get_string_member_with_default(output, "transform", NULL),
                                        value) != 0;
    const gboolean focused = json_object_get_boolean_member_with_default(output, "focused", FALSE);
    if (changed) {
        g_autofree char *command = g_strdup_printf("output %s transform %s", name, value);
        if (!sway_command(command, FALSE))
            return FALSE;
    }
    map_single_touchscreen(name);
    if (changed && focused) {
        output = read_output(parser);
        JsonObject *rect = output ? json_object_get_object_member(output, "rect") : NULL;
        if (rect) {
            const gint64 x = json_object_get_int_member_with_default(rect, "x", 0) +
                            json_object_get_int_member_with_default(rect, "width", 0) / 2;
            const gint64 y = json_object_get_int_member_with_default(rect, "y", 0) +
                            json_object_get_int_member_with_default(rect, "height", 0) / 2;
            g_autofree char *command = g_strdup_printf("seat - cursor set %" G_GINT64_FORMAT
                                                      " %" G_GINT64_FORMAT, x, y);
            sway_command(command, FALSE);
        }
    }
    return TRUE;
}

gboolean zavo_show_home(void)
{
    return sway_command("workspace 1:Home", FALSE);
}

gint64 zavo_close_focused_window(const char *output)
{
    ZavoLayout layout;
    g_autoptr(GPtrArray) windows = read_layout(&layout);
    for (guint i = 0; windows && layout.focused_window && i < windows->len; ++i) {
        const ZavoWindow *window = windows->pdata[i];
        if (window->id == layout.focused_window &&
            (!*output || g_strcmp0(output, window->output) == 0) &&
            zavo_close_window(window->id))
            return window->id;
    }
    return 0;
}

static gboolean enter_apps_workspace(gboolean in_session, gboolean wide, const char *layout_mode)
{
    if (!in_session)
        return TRUE;
    if (!sway_command("workspace 2:Windows", FALSE))
        return FALSE;
    ZavoLayout layout;
    g_autoptr(GPtrArray) windows = read_layout(&layout);
    if (windows)
        zavo_apply_layout(&layout, wide || zavo_tablet_layout(layout_mode, layout.width, layout.height));
    return TRUE;
}

static gboolean finish_launch(gboolean started, gboolean in_session, GError *error)
{
    if (!started) {
        if (error)
            g_printerr("%s\n", error->message);
        if (in_session)
            zavo_show_home();
    }
    return started;
}

static char *snap_session_bus(void)
{
    g_autofree char *socket = g_build_filename(g_get_user_runtime_dir(), "bus", NULL);
    if (!g_file_test(socket, G_FILE_TEST_EXISTS))
        return NULL;
    g_autofree char *escaped = g_dbus_address_escape_value(socket);
    return g_strconcat("unix:path=", escaped, NULL);
}

static gboolean is_snap_app(GAppInfo *info)
{
    const char *executable = g_app_info_get_executable(info);
    return (G_IS_DESKTOP_APP_INFO(info) &&
            g_desktop_app_info_has_key(G_DESKTOP_APP_INFO(info), "X-SnapInstanceName")) ||
           (executable && g_str_has_prefix(executable, "/snap/bin/"));
}

static gboolean launch_firefox(GAppInfo *info)
{
    ZavoLayout layout;
    g_autoptr(GPtrArray) windows = read_layout(&layout);
    for (guint i = 0; windows && i < windows->len; ++i) {
        const ZavoWindow *window = windows->pdata[i];
        if (g_str_has_prefix(window->description, "Wayland · firefox") ||
            g_str_equal(window->description, "Wayland · org.mozilla.firefox"))
            return zavo_focus_window(window->id);
    }

    const char *desktop_id = info ? g_app_info_get_id(info) : NULL;
    const char *executable = info ? g_app_info_get_executable(info) : "firefox";
    const gboolean snap = (info && is_snap_app(info)) ||
        g_strcmp0(desktop_id, "firefox_firefox.desktop") == 0 ||
        (!info && g_file_test("/snap/bin/firefox", G_FILE_TEST_IS_EXECUTABLE));
    g_autofree char *basename = executable ? g_path_get_basename(executable) : NULL;
    const gboolean flatpak = g_strcmp0(basename, "flatpak") == 0;
    g_autofree char *profile = snap
        ? g_build_filename(g_get_home_dir(), "snap", "firefox", "common", "zavo-profile", NULL)
        : flatpak ? g_build_filename(g_get_home_dir(), ".var", "app", "org.mozilla.firefox",
                                     "data", "zavo-profile", NULL)
                  : g_build_filename(g_get_user_data_dir(), "zavo", "firefox-profile", NULL);

    g_autofree char *fallback = !info && !snap ? g_find_program_in_path("firefox") : NULL;
    if (!executable || (snap && !g_file_test("/snap/bin/firefox", G_FILE_TEST_IS_EXECUTABLE)) ||
        (!info && !snap && !fallback) ||
        g_mkdir_with_parents(profile, 0700) < 0 || !enter_apps_workspace(TRUE, TRUE, NULL))
        return FALSE;

    const char *native_args[] = { snap ? "/snap/bin/firefox" : executable,
                                  "--no-remote", "--profile", profile, NULL };
    const char *flatpak_args[] = { executable, "run", "org.mozilla.firefox",
                                   "--no-remote", "--profile", profile, NULL };
    g_auto(GStrv) env = g_environ_setenv(g_get_environ(), "MOZ_ENABLE_WAYLAND", "1", TRUE);
    g_autofree char *bus = snap ? snap_session_bus() : NULL;
    if (bus)
        env = g_environ_setenv(env, "DBUS_SESSION_BUS_ADDRESS", bus, TRUE);
    g_autoptr(GError) error = NULL;
    const gboolean started = g_spawn_async(NULL, (char **)(flatpak ? flatpak_args : native_args),
                                           env, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);
    return finish_launch(started, TRUE, error);
}

gboolean zavo_launch_app(const GPtrArray *apps, const char *key, gboolean shortcut,
                         gboolean in_session, const char *layout_mode)
{
    for (guint i = 0; i < apps->len; ++i) {
        ZavoApp *app = g_ptr_array_index(apps, i);
        if (app->shortcut != shortcut || g_strcmp0(shortcut ? app->name : app->id, key) != 0)
            continue;
        if (shortcut && in_session && g_str_equal(key, "Firefox"))
            return launch_firefox(app->info);
        if (!app->info || !enter_apps_workspace(in_session, prefers_wide(app->id), layout_mode))
            return FALSE;
        g_autoptr(GAppLaunchContext) context = g_app_launch_context_new();
        g_autofree char *bus = in_session && is_snap_app(app->info) ? snap_session_bus() : NULL;
        if (bus)
            g_app_launch_context_setenv(context, "DBUS_SESSION_BUS_ADDRESS", bus);
        g_autoptr(GError) error = NULL;
        const gboolean started = g_app_info_launch(app->info, NULL, context, &error);
        return finish_launch(started, in_session, error);
    }
    return FALSE;
}

gboolean zavo_open_builtin(const char *executable, const char *mode, gboolean in_session,
                           const char *layout_mode)
{
    if (!enter_apps_workspace(in_session, FALSE, layout_mode))
        return FALSE;
    const char *argv[] = { executable, mode, in_session ? "-platform" : NULL, "wayland", NULL };
    g_autoptr(GError) error = NULL;
    const gboolean started = g_spawn_async(NULL, (char **)argv, NULL, 0, NULL, NULL, NULL, &error);
    return finish_launch(started, in_session, error);
}

gboolean zavo_focus_window(gint64 id)
{
    g_autofree char *command = g_strdup_printf("[con_id=%" G_GINT64_FORMAT "] focus", id);
    return id > 0 && sway_command(command, FALSE);
}

gboolean zavo_close_window(gint64 id)
{
    g_autofree char *command = g_strdup_printf("[con_id=%" G_GINT64_FORMAT "] kill", id);
    return id > 0 && sway_command(command, FALSE);
}

gboolean zavo_end_session(void)
{
    return sway_command("exit", TRUE);
}
