#include <X11/extensions/Xrandr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libnotify/notify.h>
#include <sys/utsname.h>

#define VARS_FILE "/tmp/cinemavars"
#define MAX_LINE 256
#define MAX_LAYOUT 256

// Device-specific settings
typedef struct {
    char *primary_display;
    char *extra_display;
    char *primary_mode;
    char *extra_mode;
    char *primary_pos; // e.g., "--left-of HDMI-1-0"
    char *extra_pos;   // e.g., "--right-of eDP-1"
    double primary_rate;
    double extra_rate;
} Config;

Config config;
int touchscreen_is_connected = 0;

// Global variables
char active_state[16] = "off";
char mirror_state[16] = "nomirror";
char layout[MAX_LAYOUT] = "";

// Initialize device-specific settings
void init_config(const char *device) {
    if (strcmp(device, "laptop") == 0) {
        config.primary_display = "eDP-1";
        config.extra_display = "HDMI-1-0";
        config.primary_mode = "1920x1080";
        config.extra_mode = "1920x1080";
        config.primary_pos = "--left-of HDMI-1-0";
        config.extra_pos = "--right-of eDP-1";
        config.primary_rate = 144.0;
        config.extra_rate = 120.0;
    }
    else if (strcmp(device, "main") == 0)
    {
        config.primary_display = "eDP-1-1";
        config.extra_display = "HDMI-0";
        config.primary_mode = "1920x1080";
        config.extra_mode = "1920x1080";
        config.primary_pos = "";
        config.extra_pos = "";
        config.primary_rate = 165.0;
        config.extra_rate = 29.97;
    }
}

// Execute xrandr command
void run_xrandr(const char *cmd) {
    char full_cmd[512];
    snprintf(full_cmd, sizeof(full_cmd), "xrandr %s", cmd);
    system(full_cmd);
}

// Check if a monitor is connected
int is_connected(Display *dpy, XRRScreenResources *res, const char *output, int is_primary) {
    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo *info = XRRGetOutputInfo(dpy, res, res->outputs[i]);
        if (strcmp(info->name, output) == 0 && info->connection == RR_Connected) {
            XRRFreeOutputInfo(info);
            return 1;
        }
        XRRFreeOutputInfo(info);
    }
    return 0;
}

// Read vars file
void read_vars() {
    FILE *fp = fopen(VARS_FILE, "r");
    if (!fp) return;

    char line[MAX_LINE];
    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        strncpy(active_state, line, sizeof(active_state));
    }
    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        strncpy(mirror_state, line, sizeof(mirror_state));
    }
    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, "null") != 0 && line[0] != '\0') {
            strncpy(layout, line, sizeof(layout));
            char *token = strtok(line, " ");
            if (token) config.primary_display = strdup(token);
            token = strtok(NULL, " ");
            if (token) config.extra_display = strdup(token);
        }
    }
    fclose(fp);
}

// Write vars file
void write_vars(const char *active, const char *mirror, const char *layout_str) {
    FILE *fp = fopen(VARS_FILE, "w");
    if (!fp) return;
    fprintf(fp, "%s\n%s\n%s\n", active ? active : active_state,
            mirror ? mirror : mirror_state,
            layout_str ? layout_str : layout);
    fclose(fp);
}

// Notify via libnotify
void notify(const char *message) {
    notify_init("monitor");
    NotifyNotification *n = notify_notification_new("Monitor", message, NULL);
    notify_notification_show(n, NULL);
    g_object_unref(n);
    notify_uninit();
}

// Save window positions
void save_windows() {
    system("wmctrl -l > ~/.window_positions");
}

// Restore window positions
void restore_windows() {
    system("while read -r id desktop x y w h rest; do wmctrl -i -r \"$id\" -e 0,$x,$y,$w,$h; done < ~/.window_positions");
}

// Stub for touchscreen mapping
void map_touchscreen() {
    // Original script returns; no action needed
}

// Turn on monitors
void on() {
    char cmd[512];
    save_windows();
    write_vars("on", NULL, NULL);
    if (strcmp(mirror_state, "mirror") == 0) {
        snprintf(cmd, sizeof(cmd), "--output %s --primary --mode %s --rate %.2f "
                 "--output %s --mode %s --rate %.2f --same-as %s",
                 config.primary_display, config.primary_mode, config.primary_rate,
                 config.extra_display, config.primary_mode, config.extra_rate,
                 config.primary_display);
        run_xrandr(cmd);
        notify("Set to mirror");
    } else {
        snprintf(cmd, sizeof(cmd), "--output %s --primary --mode %s --rate %.2f "
                 "--output %s --mode %s --rate %.2f %s %s",
                 config.primary_display, config.primary_mode, config.primary_rate,
                 config.extra_display, config.extra_mode, config.extra_rate,
                 config.extra_pos, config.primary_display);
        run_xrandr(cmd);
    }
    map_touchscreen();
    restore_windows();
}

// Turn off extra monitor
void off() {
    char cmd[512];
    save_windows();
    write_vars("off", NULL, NULL);
    snprintf(cmd, sizeof(cmd), "--output %s --off", config.extra_display);
    run_xrandr(cmd);
    map_touchscreen();
    restore_windows();
}

// Toggle primary display only
void only(Display *dpy, XRRScreenResources *res) {
    char cmd[512];
    save_windows();
    if (is_connected(dpy, res, config.primary_display, 1)) {
        snprintf(cmd, sizeof(cmd), "--output %s --off", config.primary_display);
        run_xrandr(cmd);
    } else {
        if (strcmp(mirror_state, "mirror") == 0) {
            snprintf(cmd, sizeof(cmd), "--output %s --auto %s %s --output %s --same-as %s",
                     config.primary_display, config.primary_pos, config.extra_display,
                     config.extra_display, config.primary_display);
        } else {
            snprintf(cmd, sizeof(cmd), "--output %s --auto %s %s "
                     "--output %s --mode %s --rate %.2f %s %s",
                     config.primary_display, config.primary_pos, config.extra_display,
                     config.extra_display, config.extra_mode, config.extra_rate,
                     config.extra_pos, config.primary_display);
        }
        run_xrandr(cmd);
    }
    restore_windows();
}

// Turn off all monitors
void voidout() {
    char cmd[512];
    save_windows();
    snprintf(cmd, sizeof(cmd), "--output %s --off --output %s --off",
             config.primary_display, config.extra_display);
    run_xrandr(cmd);
    restore_windows();
}

// Toggle monitors
void toggle(Display *dpy, XRRScreenResources *res) {
    if (is_connected(dpy, res, config.extra_display, 0)) {
        off();
    } else {
        on();
    }
}

// Refresh based on last state
void refresh() {
    if (strcmp(active_state, "off") == 0) {
        off();
    } else {
        on();
    }
}

// Toggle mirror state
void toggle_mirror() {
    char new_mirror[16];
    strcpy(new_mirror, strcmp(mirror_state, "mirror") == 0 ? "nomirror" : "mirror");
    write_vars(NULL, new_mirror, NULL);
    char msg[64];
    snprintf(msg, sizeof(msg), "Set cinema to %s", new_mirror);
    notify(msg);
    strcpy(mirror_state, new_mirror);
}

// Set layout
void set_layout(const char *new_layout) {
    write_vars(NULL, NULL, new_layout);
    char msg[128];
    snprintf(msg, sizeof(msg), "Set layout to %s", new_layout);
    notify(msg);
    strcpy(layout, new_layout);
    char *token = strtok(strdup(new_layout), " ");
    if (token) config.primary_display = strdup(token);
    token = strtok(NULL, " ");
    if (token) config.extra_display = strdup(token);
}

int main(int argc, char *argv[]) {
    // Get device name
    struct utsname uname_data;
    uname(&uname_data);
    init_config(uname_data.nodename);

    // Read vars file
    read_vars();

    // Open X display
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    Window root = DefaultRootWindow(dpy);
    XRRScreenResources *res = XRRGetScreenResources(dpy, root);
    if (!res) {
        fprintf(stderr, "Cannot get screen resources\n");
        XCloseDisplay(dpy);
        return 1;
    }

    // Parse arguments
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "off") == 0) {
            off();
        } else if (strcmp(argv[i], "on") == 0) {
            on();
        } else if (strcmp(argv[i], "only") == 0) {
            only(dpy, res);
        } else if (strcmp(argv[i], "voidout") == 0) {
            voidout();
        } else if (strcmp(argv[i], "toggle") == 0) {
            toggle(dpy, res);
        } else if (strcmp(argv[i], "refresh") == 0) {
            refresh();
        } else if (strcmp(argv[i], "mirror") == 0 || strcmp(argv[i], "nomirror") == 0) {
            toggle_mirror();
        } else if (strcmp(argv[i], "layout") == 0) {
            i++;
            char layout_str[MAX_LAYOUT] = "";
            int count = 0;
            while (i < argc && strcmp(argv[i], "--") != 0) {
                if (count < 2) {
                    strcat(layout_str, argv[i]);
                    strcat(layout_str, " ");
                    if (count == 0) config.primary_display = strdup(argv[i]);
                    else config.extra_display = strdup(argv[i]);
                } else {
                    fprintf(stderr, "Error: More than two monitors not supported\n");
                    XRRFreeScreenResources(res);
                    XCloseDisplay(dpy);
                    return 1;
                }
                count++;
                i++;
            }
            if (count == 0) {
                fprintf(stderr, "Usage: %s layout primary_display [extra_display] [--]\n", argv[0]);
                XRRFreeScreenResources(res);
                XCloseDisplay(dpy);
                return 1;
            }
            set_layout(layout_str);
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            config.extra_mode = strdup(argv[++i]);
        } else {
            refresh();
        }
        i++;
    }

    // Run setbg (assuming it's an external command)
    system("setbg");

    XRRFreeScreenResources(res);
    XCloseDisplay(dpy);
    return 0;
}
