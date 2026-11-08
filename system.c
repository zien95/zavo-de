// SPDX-License-Identifier: GPL-2.0-or-later
#include "system.h"

#include <gio/gio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char *network_service = "org.freedesktop.NetworkManager";
static const char *network_path = "/org/freedesktop/NetworkManager";
static const char *bluez_service = "org.bluez";
static const char *adapter_interface = "org.bluez.Adapter1";
static const char *modem_service = "org.freedesktop.ModemManager1";
static const char *modem_path = "/org/freedesktop/ModemManager1";
static const char *location_interface = "org.freedesktop.ModemManager1.Modem.Location";
static const char *properties_interface = "org.freedesktop.DBus.Properties";

enum { UNAVAILABLE = -1, OFF = 0, ON = 1, BLOCKED = 2 };
enum {
    GPS_RAW = 1 << 1,
    GPS_NMEA = 1 << 2,
    GPS_UNMANAGED = 1 << 4,
    GPS_ASSISTED = (1 << 5) | (1 << 6),
    GPS_SOURCES = GPS_RAW | GPS_NMEA | GPS_UNMANAGED,
};

static GVariant *call(GDBusConnection *bus, const char *service, const char *path,
                      const char *interface, const char *method, GVariant *args,
                      const GVariantType *reply, GError **error)
{
    return g_dbus_connection_call_sync(bus, service, path, interface, method, args, reply,
                                       G_DBUS_CALL_FLAGS_NONE, error ? 30000 : 1200, NULL, error);
}

static GVariant *get_properties(GDBusConnection *bus, const char *service, const char *path,
                                const char *interface)
{
    g_autoptr(GVariant) reply = call(bus, service, path, properties_interface, "GetAll",
                                     g_variant_new("(s)", interface), G_VARIANT_TYPE("(a{sv})"), NULL);
    return reply ? g_variant_get_child_value(reply, 0) : NULL;
}

static GVariant *find_device(GDBusConnection *bus, const char *service, const char *root,
                             const char *interface, guint32 capability_mask, char **path)
{
    g_autoptr(GVariant) reply =
        call(bus, service, root, "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", NULL,
             G_VARIANT_TYPE("(a{oa{sa{sv}}})"), NULL);
    if (!reply)
        return NULL;

    g_autoptr(GVariant) objects = g_variant_get_child_value(reply, 0);
    GVariantIter iterator;
    g_variant_iter_init(&iterator, objects);
    const char *object_path;
    GVariant *interfaces;
    while (g_variant_iter_next(&iterator, "{&o@a{sa{sv}}}", &object_path, &interfaces)) {
        g_autoptr(GVariant) properties =
            g_variant_lookup_value(interfaces, interface, G_VARIANT_TYPE_VARDICT);
        g_variant_unref(interfaces);
        if (!properties)
            continue;
        guint32 capabilities = 0;
        g_variant_lookup(properties, "Capabilities", "u", &capabilities);
        if (capability_mask && !(capabilities & capability_mask))
            continue;
        if (path)
            *path = g_strdup(object_path);
        return g_steal_pointer(&properties);
    }
    return NULL;
}

static gboolean set_property(GDBusConnection *bus, const char *service, const char *path,
                             const char *interface, const char *property, GVariant *value,
                             GError **error)
{
    g_autoptr(GVariant) reply =
        call(bus, service, path, properties_interface, "Set",
             g_variant_new("(ssv)", interface, property, value),
             G_VARIANT_TYPE_UNIT, error);
    return reply != NULL;
}

static int network_state(GVariant *properties, gboolean wifi)
{
    guint32 flags = 0;
    gboolean enabled = FALSE;
    gboolean hardware_enabled = FALSE;
    if (!properties || !g_variant_lookup(properties, "RadioFlags", "u", &flags) ||
        !(flags & (wifi ? 1 : 2)))
        return UNAVAILABLE;
    g_variant_lookup(properties, wifi ? "WirelessHardwareEnabled" : "WwanHardwareEnabled",
                     "b", &hardware_enabled);
    g_variant_lookup(properties, wifi ? "WirelessEnabled" : "WwanEnabled", "b", &enabled);
    return hardware_enabled ? enabled : BLOCKED;
}

static const char *profile_service = "org.freedesktop.UPower.PowerProfiles";
static const char *profile_path = "/org/freedesktop/UPower/PowerProfiles";
static const char *audio_sink = "@DEFAULT_AUDIO_SINK@";

static int battery_level(GDBusConnection *bus)
{
    g_autoptr(GVariant) battery = get_properties(bus, "org.freedesktop.UPower",
        "/org/freedesktop/UPower/devices/DisplayDevice", "org.freedesktop.UPower.Device");
    gboolean present = FALSE;
    double percentage = 0;
    if (!battery || !g_variant_lookup(battery, "IsPresent", "b", &present) || !present ||
        !g_variant_lookup(battery, "Percentage", "d", &percentage))
        return UNAVAILABLE;
    return (int)(CLAMP(percentage, 0, 100) + 0.5);
}

static int profile_state(GDBusConnection *bus)
{
    g_autoptr(GVariant) properties = get_properties(bus, profile_service, profile_path, profile_service);
    const char *active = NULL;
    if (!properties || !g_variant_lookup(properties, "ActiveProfile", "&s", &active))
        return UNAVAILABLE;
    return g_str_equal(active, "power-saver");
}

static guint32 backlight_value(const char *device, const char *attribute)
{
    g_autofree char *path = g_build_filename("/sys/class/backlight", device, attribute, NULL);
    g_autofree char *value = NULL;
    if (!g_file_get_contents(path, &value, NULL, NULL))
        return 0;
    return (guint32)g_ascii_strtoull(value, NULL, 10);
}

static char *backlight_device(void)
{
    g_autoptr(GDir) devices = g_dir_open("/sys/class/backlight", 0, NULL);
    const char *name;
    while (devices && (name = g_dir_read_name(devices))) {
        if (backlight_value(name, "max_brightness"))
            return g_strdup(name);
    }
    return NULL;
}

static int brightness_level(void)
{
    g_autofree char *device = backlight_device();
    if (!device)
        return UNAVAILABLE;
    const guint32 max = backlight_value(device, "max_brightness");
    const guint32 current = backlight_value(device, "brightness");
    return max ? (int)MIN(100, ((guint64)current * 100 + max / 2) / max) : UNAVAILABLE;
}

static gboolean run_audio(const char *operation, const char *value, char **output, GError **error)
{
    const char *argv[] = { "wpctl", operation, audio_sink, value, NULL };
    int status = 0;
    return g_spawn_sync(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
                        NULL, NULL, output, NULL, &status, error) &&
           g_spawn_check_wait_status(status, error);
}

static int volume_level(void)
{
    g_autofree char *output = NULL;
    if (!run_audio("get-volume", NULL, &output, NULL) || !g_str_has_prefix(output, "Volume: "))
        return UNAVAILABLE;
    if (strstr(output, "[MUTED]"))
        return 0;
    const double volume = g_ascii_strtod(output + 8, NULL);
    return (int)(CLAMP(volume, 0, 1) * 100 + 0.5);
}

static gboolean set_device_level(GDBusConnection *bus, const char *device, const char *value, GError **error)
{
    char *end;
    const long percent = strtol(value, &end, 10);
    if (end == value || *end || percent < 0 || percent > 100)
        return FALSE;
    if (g_str_equal(device, "volume")) {
        g_autofree char *argument = g_strdup_printf("%ld%%", percent);
        return run_audio("set-volume", argument, NULL, error) &&
               (percent == 0 || run_audio("set-mute", "0", NULL, error));
    }
    if (!bus || !g_str_equal(device, "brightness"))
        return FALSE;
    g_autofree char *backlight = backlight_device();
    if (!backlight)
        return FALSE;
    const guint32 max = backlight_value(backlight, "max_brightness");
    const guint32 level = MAX(1, ((guint64)max * percent + 50) / 100);
    g_autoptr(GVariant) reply =
        call(bus, "org.freedesktop.login1", "/org/freedesktop/login1/session/auto",
             "org.freedesktop.login1.Session", "SetBrightness", g_variant_new("(ssu)", "backlight", backlight, level),
             G_VARIANT_TYPE_UNIT, error);
    return reply != NULL;
}

static void print_radio_state(GDBusConnection *bus)
{
    g_autoptr(GVariant) network = get_properties(bus, network_service, network_path, network_service);
    g_autoptr(GVariant) bluetooth = find_device(bus, bluez_service, "/", adapter_interface, 0, NULL);
    g_autoptr(GVariant) gps = find_device(bus, modem_service, modem_path, location_interface,
                                        GPS_SOURCES, NULL);
    gboolean powered = FALSE;
    guint32 enabled = 0;
    const int bluetooth_state = bluetooth && g_variant_lookup(bluetooth, "Powered", "b", &powered)
        ? powered : UNAVAILABLE;
    const int gps_state = gps && g_variant_lookup(gps, "Enabled", "u", &enabled)
        ? !!(enabled & GPS_SOURCES) : UNAVAILABLE;
    g_print("%d %d %d %d %d %d %d %d\n", network_state(network, TRUE), bluetooth_state,
            network_state(network, FALSE), gps_state, battery_level(bus), profile_state(bus),
            brightness_level(), volume_level());
}

static gboolean set_radio(GDBusConnection *bus, const char *name, gboolean on, GError **error)
{
    if (g_str_equal(name, "wifi") || g_str_equal(name, "mobile")) {
        const gboolean wifi = g_str_equal(name, "wifi");
        g_autoptr(GVariant) network = get_properties(bus, network_service, network_path, network_service);
        const int state = network_state(network, wifi);
        return (state == ON || state == OFF) &&
            set_property(bus, network_service, network_path, network_service,
                         wifi ? "WirelessEnabled" : "WwanEnabled", g_variant_new_boolean(on), error);
    }

    g_autofree char *path = NULL;
    if (g_str_equal(name, "bluetooth")) {
        g_autoptr(GVariant) device = find_device(bus, bluez_service, "/", adapter_interface, 0, &path);
        return device && set_property(bus, bluez_service, path, adapter_interface, "Powered", g_variant_new_boolean(on), error);
    }
    if (!g_str_equal(name, "gps"))
        return FALSE;

    g_autoptr(GVariant) device = find_device(bus, modem_service, modem_path, location_interface,
                                           GPS_SOURCES, &path);
    if (!device)
        return FALSE;
    guint32 capabilities = 0;
    guint32 enabled = 0;
    gboolean signals = FALSE;
    g_variant_lookup(device, "Capabilities", "u", &capabilities);
    g_variant_lookup(device, "Enabled", "u", &enabled);
    g_variant_lookup(device, "SignalsLocation", "b", &signals);
    if (on) {
        if (!(enabled & GPS_SOURCES))
            enabled |= (capabilities & GPS_RAW) ? GPS_RAW :
                       (capabilities & GPS_NMEA) ? GPS_NMEA : GPS_UNMANAGED;
    } else {
        enabled &= ~(GPS_SOURCES | GPS_ASSISTED);
    }
    g_autoptr(GVariant) reply = call(bus, modem_service, path, location_interface, "Setup",
                                     g_variant_new("(ub)", enabled, signals), G_VARIANT_TYPE_UNIT, error);
    return reply != NULL;
}

static void print_orientation(GDBusProxy *sensor)
{
    g_autoptr(GVariant) value = g_dbus_proxy_get_cached_property(sensor, "AccelerometerOrientation");
    if (!value)
        return;
    const char *orientation = g_variant_get_string(value, NULL);
    const int rotation = g_str_equal(orientation, "normal") ? 0 :
                         g_str_equal(orientation, "left-up") ? 90 :
                         g_str_equal(orientation, "bottom-up") ? 180 :
                         g_str_equal(orientation, "right-up") ? 270 : -1;
    if (rotation >= 0) {
        g_print("%d\n", rotation);
        fflush(stdout);
    }
}

static void orientation_changed(GDBusProxy *sensor, GVariant *changed,
                                const char *const *invalidated, gpointer data)
{
    (void)changed;
    (void)invalidated;
    (void)data;
    print_orientation(sensor);
}

static int monitor_orientation(GDBusConnection *bus)
{
    g_autoptr(GDBusProxy) sensor = g_dbus_proxy_new_sync(bus, G_DBUS_PROXY_FLAGS_NONE, NULL,
        "net.hadess.SensorProxy", "/net/hadess/SensorProxy", "net.hadess.SensorProxy", NULL, NULL);
    if (!sensor)
        return 1;
    g_autoptr(GVariant) present = g_dbus_proxy_get_cached_property(sensor, "HasAccelerometer");
    if (!present || !g_variant_get_boolean(present))
        return 1;
    g_autoptr(GVariant) claimed = g_dbus_proxy_call_sync(sensor, "ClaimAccelerometer", NULL,
        G_DBUS_CALL_FLAGS_NONE, 1200, NULL, NULL);
    if (!claimed)
        return 1;
    print_orientation(sensor);
    g_signal_connect(sensor, "g-properties-changed", G_CALLBACK(orientation_changed), NULL);
    g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    return 0;
}

int zavo_system_command(int argc, char **argv)
{
    const gboolean query = argc == 2 && g_str_equal(argv[1], "--radio-state");
    const gboolean radio = argc == 4 && g_str_equal(argv[1], "--set-radio");
    const gboolean power = argc == 3 && g_str_equal(argv[1], "--power");
    const gboolean level = argc == 4 && g_str_equal(argv[1], "--set-level");
    const gboolean saver = argc == 3 && g_str_equal(argv[1], "--power-saver");
    const gboolean orientation = argc == 2 && g_str_equal(argv[1], "--monitor-orientation");
    if (!query && !radio && !power && !level && !saver && !orientation)
        return -1;

    g_autoptr(GError) error = NULL;
    g_autoptr(GDBusConnection) bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (orientation)
        return bus ? monitor_orientation(bus) : 1;
    if (query) {
        if (bus)
            print_radio_state(bus);
        else
            g_print("-1 -1 -1 -1 -1 -1 %d %d\n", brightness_level(), volume_level());
        return 0;
    }

    gboolean success = FALSE;
    if (level) {
        if (g_str_equal(argv[2], "volume"))
            g_clear_error(&error);
        success = set_device_level(bus, argv[2], argv[3], &error);
    }
    if (bus && saver && (g_str_equal(argv[2], "on") || g_str_equal(argv[2], "off"))) {
        const char *profile = g_str_equal(argv[2], "on") ? "power-saver" : "balanced";
        success = set_property(bus, profile_service, profile_path, profile_service,
                               "ActiveProfile", g_variant_new_string(profile), &error);
    }
    if (bus && radio && (g_str_equal(argv[3], "on") || g_str_equal(argv[3], "off")))
        success = set_radio(bus, argv[2], g_str_equal(argv[3], "on"), &error);
    if (bus && power) {
        const char *method = g_str_equal(argv[2], "suspend") ? "Suspend" :
                             g_str_equal(argv[2], "reboot") ? "Reboot" :
                             g_str_equal(argv[2], "poweroff") ? "PowerOff" : NULL;
        if (method) {
            g_autoptr(GVariant) reply =
                call(bus, "org.freedesktop.login1", "/org/freedesktop/login1",
                     "org.freedesktop.login1.Manager", method, g_variant_new("(b)", TRUE),
                     G_VARIANT_TYPE_UNIT, &error);
            success = reply != NULL;
        }
    }
    if (!success)
        g_printerr("%s\n", error ? error->message : "This action is unavailable");
    return success ? 0 : 1;
}
