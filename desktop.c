// SPDX-License-Identifier: GPL-2.0-or-later
#include "desktop.h"

#include <gio/gdesktopappinfo.h>
#include <json-glib/json-glib.h>
#include <string.h>

static const struct {
    const char *name;
    const char *icon;
    const char *ids[4];
} shortcuts[] = {
    { "Terminal", "utilities-terminal", { "org.kde.qmlkonsole" } },
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
    g_free(window);
}

static void collect_windows(JsonObject *node, GPtrArray *windows)
{
    const char *app_id = json_object_get_string_member_with_default(node, "app_id", NULL);
    const char *shell = json_object_get_string_member_with_default(node, "shell", NULL);
    const gboolean wayland = g_strcmp0(shell, "xdg_shell") == 0;
    const gboolean xwayland = g_strcmp0(shell, "xwayland") == 0;
    if ((wayland || xwayland) && g_strcmp0(app_id, "org.zavo.Shell") != 0) {
        const char *title = json_object_get_string_member_with_default(node, "name", NULL);
        const char *protocol = wayland ? "Wayland" : "XWayland";
        ZavoWindow *window = g_new0(ZavoWindow, 1);
        window->id = json_object_get_int_member_with_default(node, "id", 0);
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
            collect_windows(json_array_get_object_element(children, j), windows);
    }
}

GPtrArray *zavo_windows_parse(const char *data, gssize length)
{
    g_autoptr(JsonParser) parser = json_parser_new();
    if (!json_parser_load_from_data(parser, data, length, NULL))
        return NULL;
    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_OBJECT(root))
        return NULL;
    GPtrArray *windows = g_ptr_array_new_with_free_func(free_window);
    collect_windows(json_node_get_object(root), windows);
    return windows;
}

static gboolean sway_command(const char *command, gboolean allow_disconnect)
{
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

    /* Sway IPC command header: magic, little-endian length, then command type 0. */
    guint8 header[14] = { 'i', '3', '-', 'i', 'p', 'c' };
    const gsize length = strlen(command);
    guint32 encoded_length = GUINT32_TO_LE(length);
    memcpy(header + 6, &encoded_length, sizeof encoded_length);
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
    if (reply_length == 0 || reply_length > 65536)
        return FALSE;
    g_autofree char *reply = g_malloc(reply_length);
    if (!g_input_stream_read_all(input, reply, reply_length, &received, NULL, NULL) ||
        received != reply_length)
        return FALSE;

    g_autoptr(JsonParser) parser = json_parser_new();
    if (!json_parser_load_from_data(parser, reply, reply_length, NULL))
        return FALSE;
    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_ARRAY(root))
        return FALSE;
    JsonArray *results = json_node_get_array(root);
    return json_array_get_length(results) > 0 &&
           json_object_get_boolean_member_with_default(json_array_get_object_element(results, 0),
                                                       "success", FALSE);
}

static gboolean enter_apps_workspace(gboolean in_session)
{
    return !in_session || sway_command("workspace 2:Windows", FALSE);
}

static gboolean finish_launch(gboolean started, gboolean in_session, GError *error)
{
    if (!started) {
        if (error)
            g_printerr("%s\n", error->message);
        if (in_session)
            sway_command("workspace 1:Home", FALSE);
    }
    return started;
}

gboolean zavo_launch_app(const GPtrArray *apps, const char *key, gboolean shortcut,
                         gboolean in_session)
{
    for (guint i = 0; i < apps->len; ++i) {
        ZavoApp *app = g_ptr_array_index(apps, i);
        if (app->shortcut != shortcut || g_strcmp0(shortcut ? app->name : app->id, key) != 0)
            continue;
        if (!app->info || !enter_apps_workspace(in_session))
            return FALSE;
        g_autoptr(GError) error = NULL;
        const gboolean started = g_app_info_launch(app->info, NULL, NULL, &error);
        return finish_launch(started, in_session, error);
    }
    return FALSE;
}

gboolean zavo_open_maps(const char *executable, gboolean in_session)
{
    if (!enter_apps_workspace(in_session))
        return FALSE;
    const char *argv[] = { executable, "--maps", in_session ? "-platform" : NULL, "wayland", NULL };
    g_autoptr(GError) error = NULL;
    const gboolean started = g_spawn_async(NULL, (char **)argv, NULL, 0, NULL, NULL, NULL, &error);
    return finish_launch(started, in_session, error);
}

gboolean zavo_open_setting(const char *name)
{
    const char *settings[][2] = {
        { "Network", "kcm_networkmanagement" }, { "Bluetooth", "kcm_bluetooth" },
        { "Sound", "kcm_pulseaudio" }, { "Display", "kcm_kscreen" },
        { "Power", "kcm_powerdevilprofilesconfig" }, { "Notifications", "kcm_notifications" },
    };
    for (gsize i = 0; i < G_N_ELEMENTS(settings); ++i) {
        if (g_strcmp0(name, settings[i][0]) == 0) {
            const char *argv[] = { "kcmshell6", settings[i][1], NULL };
            return g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH,
                                 NULL, NULL, NULL, NULL);
        }
    }
    return FALSE;
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
