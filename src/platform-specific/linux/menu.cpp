#ifdef __linux__

#include <gtk/gtk.h>
#include <SDL3/SDL.h>

#include "platform-specific/menu.h"
#include "util/MenuInterface.h"

// ── Constants ─────────────────────────────────────────────────────────────────

// Hit region for the hamburger button (top-left corner of the SDL window)
static constexpr int HAMBURGER_X      = 4;
static constexpr int HAMBURGER_Y      = 4;
static constexpr int HAMBURGER_W      = 30;
static constexpr int HAMBURGER_H      = 30;

// ── Module state ──────────────────────────────────────────────────────────────

static SDL_Window   *g_window          = nullptr;
static GtkWidget    *g_menu            = nullptr;
static GtkWidget    *g_drives_menu     = nullptr;
static GtkWidget    *g_speed_menu      = nullptr;
static GtkWidget    *g_controller_menu = nullptr;
static GtkWidget    *g_monitor_menu    = nullptr;
static bool          g_gtk_inited      = false;

// Async popup state — avoids blocking inside SDL_AppEvent (X11 deadlock risk)
static bool          g_menu_pending    = false; // trigger received, show on next iterate
static bool          g_menu_active     = false; // menu is currently displayed
static bool          g_was_grab        = false; // SDL_SetWindowMouseGrab state before popup
static bool          g_was_relative    = false; // SDL relative-mouse-mode state before popup

// Storage keys parallel to the dynamically built Drives items
static std::vector<storage_key_t> g_drive_keys;

// ── Helpers ───────────────────────────────────────────────────────────────────

static GtkWidget *make_item(const char *label)
{
    return gtk_menu_item_new_with_label(label);
}

static GtkWidget *make_check(const char *label, bool active)
{
    GtkWidget *item = gtk_check_menu_item_new_with_label(label);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), active);
    return item;
}

static GtkWidget *make_separator()
{
    return gtk_separator_menu_item_new();
}

static void append(GtkWidget *menu, GtkWidget *item)
{
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
}

// ── Drives submenu (rebuilt on "show") ────────────────────────────────────────

static void rebuild_drives_menu(GtkWidget * /*menu*/, gpointer /*data*/)
{
    // Remove all existing items
    GList *children = gtk_container_get_children(GTK_CONTAINER(g_drives_menu));
    for (GList *l = children; l; l = l->next)
        gtk_container_remove(GTK_CONTAINER(g_drives_menu), GTK_WIDGET(l->data));
    g_list_free(children);
    g_drive_keys.clear();

    MenuInterface *mi = getMenuInterface();
    bool running = mi->isEmulationRunning();
    auto drives = mi->getDriveList();

    if (drives.empty()) {
        GtkWidget *empty = make_item("(no drives)");
        gtk_widget_set_sensitive(empty, FALSE);
        append(g_drives_menu, empty);
    } else {
        for (size_t i = 0; i < drives.size(); ++i) {
            const MenuDriveInfo &info = drives[i];
            g_drive_keys.push_back(info.key);

            std::string label = "S" + std::to_string(info.key.slot)
                              + "D" + std::to_string(info.key.drive + 1);
            if (info.is_mounted && !info.filename.empty()) {
                std::string fname = info.filename;
                size_t pos = fname.find_last_of("/\\");
                if (pos != std::string::npos) fname = fname.substr(pos + 1);
                label += ": " + fname;
                if (info.is_modified)        label += " *";
                if (info.is_write_protected) label += " [WP]";
            } else {
                label += ": (empty)";
            }

            GtkWidget *item = make_item(label.c_str());
            gtk_widget_set_sensitive(item, running);

            // Capture index by value via a heap-allocated copy
            size_t *idx_ptr = new size_t(i);
            g_signal_connect_data(item, "activate",
                G_CALLBACK(+[](GtkMenuItem * /*item*/, gpointer data) {
                    size_t idx = *static_cast<size_t *>(data);
                    if (idx < g_drive_keys.size())
                        getMenuInterface()->diskToggle(g_drive_keys[idx]);
                }),
                idx_ptr,
                [](gpointer data, GClosure * /*closure*/) { delete static_cast<size_t *>(data); },
                G_CONNECT_DEFAULT);

            append(g_drives_menu, item);
        }
    }

    gtk_widget_show_all(g_drives_menu);
}

// ── Dynamic state updates for Settings / Display submenus ────────────────────

static void update_speed_menu(GtkWidget * /*menu*/, gpointer /*data*/)
{
    MenuInterface *mi = getMenuInterface();
    bool running = mi->isEmulationRunning();
    int current = mi->getCurrentSpeed(); // SPEED_1_0=1 .. SPEED_14_3=4

    // Items are in order: 1.0, 2.8, 7.1, 14.3 → tags 1..4
    GList *children = gtk_container_get_children(GTK_CONTAINER(g_speed_menu));
    int tag = 1;
    for (GList *l = children; l; l = l->next, ++tag) {
        GtkWidget *item = GTK_WIDGET(l->data);
        gtk_widget_set_sensitive(item, running);
        if (GTK_IS_CHECK_MENU_ITEM(item))
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), tag == current);
    }
    g_list_free(children);
}

static void update_controller_menu(GtkWidget * /*menu*/, gpointer /*data*/)
{
    MenuInterface *mi = getMenuInterface();
    bool running = mi->isEmulationRunning();
    int current = mi->getCurrentControllerMode(); // 0..2

    GList *children = gtk_container_get_children(GTK_CONTAINER(g_controller_menu));
    int idx = 0;
    for (GList *l = children; l; l = l->next, ++idx) {
        GtkWidget *item = GTK_WIDGET(l->data);
        gtk_widget_set_sensitive(item, running);
        if (GTK_IS_CHECK_MENU_ITEM(item))
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), idx == current);
    }
    g_list_free(children);
}

static void update_monitor_menu(GtkWidget * /*menu*/, gpointer /*data*/)
{
    MenuInterface *mi = getMenuInterface();
    bool running = mi->isEmulationRunning();
    int current = mi->getCurrentMonitor(); // MONITOR_COMPOSITE(200)..

    // IDs map: pos 0→200, 1→201, ..., 4→204
    GList *children = gtk_container_get_children(GTK_CONTAINER(g_monitor_menu));
    int id = MONITOR_COMPOSITE;
    for (GList *l = children; l; l = l->next, ++id) {
        GtkWidget *item = GTK_WIDGET(l->data);
        gtk_widget_set_sensitive(item, running);
        if (GTK_IS_CHECK_MENU_ITEM(item))
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), id == current);
    }
    g_list_free(children);
}

// ── Menu construction ─────────────────────────────────────────────────────────

static void connect_activate(GtkWidget *item, void (*fn)())
{
    g_signal_connect(item, "activate", G_CALLBACK(+[](GtkMenuItem *, gpointer data) {
        reinterpret_cast<void(*)()>(data)();
    }), reinterpret_cast<gpointer>(fn));
}

static void build_menu()
{
    MenuInterface *mi = getMenuInterface();
    g_menu = gtk_menu_new();

    // ── File ──────────────────────────────────────────────────────────────────
    GtkWidget *file_item = make_item("File");
    GtkWidget *file_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);

    // Drives submenu
    GtkWidget *drives_item = make_item("Drives");
    g_drives_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(drives_item), g_drives_menu);
    g_signal_connect(g_drives_menu, "show", G_CALLBACK(rebuild_drives_menu), nullptr);
    append(file_menu, drives_item);

    append(file_menu, make_separator());

    GtkWidget *close_item = make_item("Close Emulation");
    connect_activate(close_item, []() {
        if (SDL_EventEnabled(SDL_EVENT_QUIT)) {
            SDL_Event ev = {};
            ev.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&ev);
        }
    });
    append(file_menu, close_item);

    append(file_menu, make_separator());

    GtkWidget *quit_item = make_item("Quit");
    connect_activate(quit_item, []() {
        SDL_Event ev = {};
        ev.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&ev);
    });
    append(file_menu, quit_item);

    append(g_menu, file_item);

    // ── Edit ──────────────────────────────────────────────────────────────────
    GtkWidget *edit_item = make_item("Edit");
    GtkWidget *edit_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_item), edit_menu);

    GtkWidget *copy_item = make_item("Copy Screen");
    connect_activate(copy_item, []() { getMenuInterface()->editCopyScreen(); });
    append(edit_menu, copy_item);

    GtkWidget *paste_item = make_item("Paste Text");
    connect_activate(paste_item, []() { getMenuInterface()->editPasteText(); });
    append(edit_menu, paste_item);

    append(g_menu, edit_item);

    // ── Machine ───────────────────────────────────────────────────────────────
    GtkWidget *machine_item = make_item("Machine");
    GtkWidget *machine_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(machine_item), machine_menu);

    GtkWidget *reset_item = make_item("Reset");
    connect_activate(reset_item, []() { getMenuInterface()->machineReset(); });
    append(machine_menu, reset_item);

    GtkWidget *restart_item = make_item("Restart");
    connect_activate(restart_item, []() { getMenuInterface()->machineRestart(); });
    append(machine_menu, restart_item);

    GtkWidget *pause_item = make_item("Pause / Resume");
    connect_activate(pause_item, []() { getMenuInterface()->machinePauseResume(); });
    append(machine_menu, pause_item);

    append(machine_menu, make_separator());

    GtkWidget *capture_item = make_item("Capture Mouse");
    connect_activate(capture_item, []() { getMenuInterface()->machineCaptureMouse(); });
    append(machine_menu, capture_item);

    append(g_menu, machine_item);

    // ── Settings ──────────────────────────────────────────────────────────────
    GtkWidget *settings_item = make_item("Settings");
    GtkWidget *settings_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(settings_item), settings_menu);

    // Speed submenu
    GtkWidget *speed_item = make_item("Speed");
    g_speed_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(speed_item), g_speed_menu);
    g_signal_connect(g_speed_menu, "show", G_CALLBACK(update_speed_menu), nullptr);

    struct { const char *label; int speed_id; } speeds[] = {
        { "1.0 MHz",  SPEED_1_0  },
        { "2.8 MHz",  SPEED_2_8  },
        { "7.1 MHz",  SPEED_7_1  },
        { "14.3 MHz", SPEED_14_3 },
    };
    for (auto &s : speeds) {
        GtkWidget *si = make_check(s.label, false);
        int *sid = new int(s.speed_id);
        g_signal_connect_data(si, "activate",
            G_CALLBACK(+[](GtkMenuItem *, gpointer data) {
                getMenuInterface()->setSpeed(*static_cast<int *>(data));
            }),
            sid,
            [](gpointer d, GClosure *) { delete static_cast<int *>(d); },
            G_CONNECT_DEFAULT);
        append(g_speed_menu, si);
    }
    append(settings_menu, speed_item);

    // Game Controller submenu
    GtkWidget *ctrl_item = make_item("Game Controller");
    g_controller_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(ctrl_item), g_controller_menu);
    g_signal_connect(g_controller_menu, "show", G_CALLBACK(update_controller_menu), nullptr);

    struct { const char *label; int mode; } controllers[] = {
        { "Joystick - Gamepad",       0 },
        { "Joystick - Mouse",         1 },
        { "Sirius / Atari Joyport",   2 },
    };
    for (auto &c : controllers) {
        GtkWidget *ci = make_check(c.label, false);
        int *cmode = new int(c.mode);
        g_signal_connect_data(ci, "activate",
            G_CALLBACK(+[](GtkMenuItem *, gpointer data) {
                getMenuInterface()->setControllerMode(*static_cast<int *>(data));
            }),
            cmode,
            [](gpointer d, GClosure *) { delete static_cast<int *>(d); },
            G_CONNECT_DEFAULT);
        append(g_controller_menu, ci);
    }
    append(settings_menu, ctrl_item);

    // Sleep / Busy Wait check item
    GtkWidget *sleep_item = make_check("Sleep / Busy Wait", mi->getSleepMode());
    connect_activate(sleep_item, []() { getMenuInterface()->toggleSleepMode(); });
    append(settings_menu, sleep_item);
    // Update checkmark before the settings menu opens
    g_signal_connect(settings_menu, "show", G_CALLBACK(+[](GtkWidget *menu, gpointer) {
        MenuInterface *mi2 = getMenuInterface();
        bool running = mi2->isEmulationRunning();
        GList *ch = gtk_container_get_children(GTK_CONTAINER(menu));
        for (GList *l = ch; l; l = l->next) {
            GtkWidget *w = GTK_WIDGET(l->data);
            // Sleep item is the last non-submenu item
            if (GTK_IS_CHECK_MENU_ITEM(w)) {
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w), mi2->getSleepMode());
                gtk_widget_set_sensitive(w, running);
            } else {
                gtk_widget_set_sensitive(w, running);
            }
        }
        g_list_free(ch);
    }), nullptr);

    append(g_menu, settings_item);

    // ── Display ───────────────────────────────────────────────────────────────
    GtkWidget *display_item = make_item("Display");
    GtkWidget *display_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(display_item), display_menu);

    // Monitor submenu
    GtkWidget *monitor_item = make_item("Monitor");
    g_monitor_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(monitor_item), g_monitor_menu);
    g_signal_connect(g_monitor_menu, "show", G_CALLBACK(update_monitor_menu), nullptr);

    struct { const char *label; int id; } monitors[] = {
        { "Composite",         MONITOR_COMPOSITE  },
        { "GS RGB",            MONITOR_GS_RGB     },
        { "Monochrome - Green",MONITOR_MONO_GREEN },
        { "Monochrome - Amber",MONITOR_MONO_AMBER },
        { "Monochrome - White",MONITOR_MONO_WHITE },
    };
    for (auto &m : monitors) {
        GtkWidget *mi2 = make_check(m.label, false);
        int *mid = new int(m.id);
        g_signal_connect_data(mi2, "activate",
            G_CALLBACK(+[](GtkMenuItem *, gpointer data) {
                getMenuInterface()->setMonitor(*static_cast<int *>(data));
            }),
            mid,
            [](gpointer d, GClosure *) { delete static_cast<int *>(d); },
            G_CONNECT_DEFAULT);
        append(g_monitor_menu, mi2);
    }
    append(display_menu, monitor_item);

    GtkWidget *fullscreen_item = make_item("Full Screen");
    connect_activate(fullscreen_item, []() { getMenuInterface()->displayFullScreen(); });
    append(display_menu, fullscreen_item);

    // Enable/disable display items based on running state
    g_signal_connect(display_menu, "show", G_CALLBACK(+[](GtkWidget *menu, gpointer) {
        bool running = getMenuInterface()->isEmulationRunning();
        GList *ch = gtk_container_get_children(GTK_CONTAINER(menu));
        for (GList *l = ch; l; l = l->next)
            gtk_widget_set_sensitive(GTK_WIDGET(l->data), running);
        g_list_free(ch);
    }), nullptr);

    append(g_menu, display_item);

    // ── Docs ──────────────────────────────────────────────────────────────────
    GtkWidget *docs_item = make_item("Docs");
    GtkWidget *docs_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(docs_item), docs_menu);

    GtkWidget *online_item = make_item("Online Documentation");
    connect_activate(online_item, []() {
        SDL_OpenURL("https://jawaidbazyar2.github.io/gssquared/");
    });
    append(docs_menu, online_item);

    append(g_menu, docs_item);

    gtk_widget_show_all(g_menu);
}

// ── Show the popup ────────────────────────────────────────────────────────────

// Called via the GtkMenu "hide" signal — cleans up after the popup closes.
static void on_menu_hide(GtkWidget * /*widget*/, gpointer /*data*/)
{
    g_menu_active = false;
    if (g_was_grab) {
        SDL_SetWindowMouseGrab(g_window, true);
        SDL_CaptureMouse(true);
        g_was_grab = false;
    }
    if (g_was_relative) {
        SDL_SetWindowRelativeMouseMode(g_window, true);
        g_was_relative = false;
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void initMenu(SDL_Window *window)
{
    g_window = window;

    if (!g_gtk_inited) {
        gtk_init(nullptr, nullptr);
        g_gtk_inited = true;
    }

    // Destroy existing menu if we're re-initialising (new window after close)
    if (g_menu) {
        gtk_widget_destroy(g_menu);
        g_menu          = nullptr;
        g_drives_menu   = nullptr;
        g_speed_menu    = nullptr;
        g_controller_menu = nullptr;
        g_monitor_menu  = nullptr;
        g_drive_keys.clear();
    }

    build_menu();
}

void setMenuTrackingCallback(MenuIterateCallback /*callback*/, void * /*appstate*/)
{
    // GTK runs its own GLib main loop during menu tracking; no SDL iterate
    // callback is needed on Linux.
}

bool handleMenuEvent(const SDL_Event *event)
{
    if (!g_gtk_inited) return false;

    // We handle both BUTTON_DOWN and BUTTON_UP events in the trigger zones.
    //
    // Trigger on BUTTON_UP (not DOWN) so that by the time pumpMenuEvents runs,
    // both SDL's and GDK's X11 connections have seen the button release.
    // If we triggered on DOWN, GDK would still see the button as pressed when
    // gtk_menu_popup is called, putting GTK into drag-to-select mode (hold button
    // to keep menu open, release to select) instead of click-click mode.
    // BUTTON_DOWN is consumed to prevent the emulator from acting on it.
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event->type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        float x = event->button.x;
        float y = event->button.y;

        bool left_in_hamburger =
            event->button.button == SDL_BUTTON_LEFT &&
            x >= HAMBURGER_X && x < HAMBURGER_X + HAMBURGER_W &&
            y >= HAMBURGER_Y && y < HAMBURGER_Y + HAMBURGER_H;

        bool right_click = (event->button.button == SDL_BUTTON_RIGHT);

        if (left_in_hamburger || right_click) {
            if (event->type == SDL_EVENT_MOUSE_BUTTON_UP)
                g_menu_pending = true;
            return true; // consume both down and up
        }
    }

    return false;
}

void pumpMenuEvents()
{
    if (!g_gtk_inited) return;

    // If a show was requested, build and popup the menu now.
    // We're called from SDL_AppIterate, so SDL's X11 connection is idle and
    // the X11 server won't stall when GDK sends its grab request.
    if (g_menu_pending) {
        g_menu_pending = false;

        // Rebuild from scratch each time — reusing a hidden GtkMenu causes
        // GDK/X11 to fail on second+ popups ("temporary window without parent").
        if (g_menu) gtk_widget_destroy(g_menu);
        g_menu = nullptr;
        g_drives_menu = g_speed_menu = g_controller_menu = g_monitor_menu = nullptr;
        g_drive_keys.clear();
        build_menu();

        g_signal_connect(g_menu, "hide", G_CALLBACK(on_menu_hide), nullptr);

        // Release all SDL pointer-grab modes so GDK can take an exclusive grab.
        // SDL_SetWindowMouseGrab and SDL_SetWindowRelativeMouseMode both call
        // XGrabPointer internally; either would prevent GDK's grab from succeeding.
        g_was_grab     = SDL_GetWindowMouseGrab(g_window);
        g_was_relative = SDL_GetWindowRelativeMouseMode(g_window);
        if (g_was_grab) {
            SDL_SetWindowMouseGrab(g_window, false);
            SDL_CaptureMouse(false);
        }
        if (g_was_relative)
            SDL_SetWindowRelativeMouseMode(g_window, false);

        // gtk_menu_popup_at_rect is the modern non-deprecated GTK3 API.
        // Passing the root GdkWindow as rect_window gives the popup a proper
        // X11 transient-for parent and lets GTK position the menu in screen
        // coordinates cleanly.
        float gx = 0.0f, gy = 0.0f;
        SDL_GetGlobalMouseState(&gx, &gy);

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        GdkWindow *root = gdk_screen_get_root_window(gdk_screen_get_default());
        G_GNUC_END_IGNORE_DEPRECATIONS

        GdkRectangle rect = { static_cast<gint>(gx), static_cast<gint>(gy), 1, 1 };
        gtk_menu_popup_at_rect(GTK_MENU(g_menu), root, &rect,
                               GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_NORTH_WEST,
                               nullptr);

        g_menu_active = true;
    }

    // While the menu is visible, drain all pending GLib/GDK events each frame
    // so GTK can process pointer motion, selections, and dismiss-on-click-outside.
    // g_main_context_iteration(FALSE) is non-blocking: it returns immediately
    // if there are no pending events.
    if (g_menu_active) {
        while (g_main_context_iteration(nullptr, FALSE)) {}
    }
}

void renderMenuOverlay(SDL_Renderer *renderer, int /*win_w*/, int /*win_h*/)
{
    // Save current draw colour
    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    SDL_BlendMode blend;
    SDL_GetRenderDrawBlendMode(renderer, &blend);

    // Semi-transparent dark background square
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
    SDL_FRect bg = { (float)HAMBURGER_X, (float)HAMBURGER_Y,
                     (float)HAMBURGER_W, (float)HAMBURGER_H };
    SDL_RenderFillRect(renderer, &bg);

    // Three white bars (the ☰ icon)
    SDL_SetRenderDrawColor(renderer, 220, 220, 220, 220);
    float bx = HAMBURGER_X + 6.0f;
    float bw = HAMBURGER_W - 12.0f;
    float bar_ys[3] = { HAMBURGER_Y + 8.0f, HAMBURGER_Y + 14.0f, HAMBURGER_Y + 20.0f };
    for (float by : bar_ys) {
        SDL_FRect bar = { bx, by, bw, 2.0f };
        SDL_RenderFillRect(renderer, &bar);
    }

    // Restore previous state
    SDL_SetRenderDrawBlendMode(renderer, blend);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

#endif // __linux__
