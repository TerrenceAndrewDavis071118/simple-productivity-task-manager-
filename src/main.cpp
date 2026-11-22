#include <gtk/gtk.h>
#ifndef _WIN32
#include <libnotify/notify.h>
#endif

#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "platform.hpp"
#include "audio.hpp"
#include "store.hpp"
#include "style.hpp"

namespace {

constexpr double PI = 3.14159265358979323846;

struct AppState {
    GtkApplication* app = nullptr;
    GtkWidget* window = nullptr;
    GtkWidget* list_box = nullptr;
    GtkWidget* view_title = nullptr;
    GtkWidget* view_count_lbl = nullptr;
    GtkWidget* add_card = nullptr;
    GtkWidget* add_hint_lbl = nullptr;
    GtkWidget* add_revealer = nullptr;
    bool add_expanded = false;
    GtkWidget* title_entry = nullptr;
    GtkWidget* detail_view = nullptr;
    GtkWidget* seg_btn[3]{};
    GtkWidget* seg_val[3]{};
    GtkWidget* calendar = nullptr;
    GtkWidget* clock_area = nullptr;
    GtkWidget* hour_btn = nullptr;
    GtkWidget* min_btn = nullptr;
    GtkWidget* ampm_btn = nullptr;
    GtkWidget* form_err = nullptr;

    // Hamburger menu (toggle 3 garis di kiri atas)
    GtkWidget* menu_btn = nullptr;
    GtkWidget* menu_popover = nullptr;
    GtkWidget* menu_achiev_btn = nullptr;
    GtkWidget* menu_stats_btn = nullptr;

    // Collapsible side drawer on the left for Achievements & Statistics
    GtkWidget* side_panel = nullptr;
    GtkWidget* side_stack = nullptr;
    GtkWidget* side_title = nullptr;
    GtkWidget* side_close_btn = nullptr;

    // Achievements in side drawer
    GtkWidget* achiev_list_box = nullptr;
    GtkWidget* achiev_empty_label = nullptr;

    // Statistics widgets
    GtkWidget* scope_btn[4]{}; // Range, Day, Month, Year
    GtkWidget* period_nav_box = nullptr;
    GtkWidget* period_menu_btn = nullptr;
    GtkWidget* period_popover = nullptr;
    GtkWidget* stat_calendar = nullptr;
    GtkWidget* range_bar = nullptr;
    GtkWidget* from_menu_btn = nullptr;
    GtkWidget* from_popover = nullptr;
    GtkWidget* from_calendar = nullptr;
    GtkWidget* to_menu_btn = nullptr;
    GtkWidget* to_popover = nullptr;
    GtkWidget* to_calendar = nullptr;
    GtkWidget* pie_area = nullptr;
    GtkWidget* stat_total_lbl = nullptr;
    GtkWidget* victims_list = nullptr;
    GtkWidget* victims_empty = nullptr;

    tk::StatScope stat_scope = tk::StatScope::Day;
    std::tm stat_date{};
    std::tm range_from{};
    std::tm range_to{};
    bool syncing_stat_cal = false;
    tk::StatSummary current_stats;

    time_t draft_times[3]{};
    bool slot_touched[3]{};
    int active_slot = 0;
    bool editing_minute = false;
    bool syncing_picker = false;
    double drag_x0 = 0.0;
    double drag_y0 = 0.0;

    std::string active_side_page; // empty when closed, "achiev" or "stats"
    std::string draft_title;
    std::string draft_detail;
    bool has_draft_times = false;
    tk::Store store;
};

#ifndef _WIN32
struct NotifCtx {
    AppState* state;
    long task_id;
};
#endif

static void refresh_ui(AppState* s);
static void refresh_stats_view(AppState* s);
static void sync_picker_ui(AppState* s);

static std::string trim(const std::string& str) {
    auto b = str.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    auto e = str.find_last_not_of(" \t\r\n");
    return str.substr(b, e - b + 1);
}

static long widget_task_id(GtkWidget* w) {
    return static_cast<long>(
        reinterpret_cast<intptr_t>(g_object_get_data(G_OBJECT(w), "task-id")));
}

static void set_widget_task_id(GtkWidget* w, long id) {
    g_object_set_data(G_OBJECT(w), "task-id",
                      reinterpret_cast<gpointer>(static_cast<intptr_t>(id)));
}

#ifndef _WIN32
static void on_notif_done(NotifyNotification*, char*, gpointer user_data) {
    auto* ctx = static_cast<NotifCtx*>(user_data);
    if (ctx && ctx->state) {
        const tk::Task* t = ctx->state->store.find(ctx->task_id);
        if (t && t->status == tk::Status::Active) {
            ctx->state->store.set_status(ctx->task_id, tk::Status::Achieved);
            refresh_ui(ctx->state);
        }
    }
}

static void on_notif_closed(NotifyNotification* n, gpointer) {
    g_object_unref(n);
}
#endif

static void send_task_notification(AppState* s, const tk::Task& t, tk::NotifKind kind) {
    std::string body;
    if (kind == tk::NotifKind::Deadline5m) {
        body = "5 minutes before deadline · " + tk::format_hm(t.deadline_at);
    } else {
        body = "Task ready · Deadline " + tk::format_hm(t.deadline_at);
    }

    std::error_code ec;
    std::string icon_path;
    if (std::filesystem::exists("/usr/share/icons/hicolor/scalable/apps/com.taskkiller.app.svg", ec)) {
        icon_path = "com.taskkiller.app";
    } else if (std::filesystem::exists("assets/com.taskkiller.app.svg", ec)) {
        icon_path = tk::normalize_path_string("assets/com.taskkiller.app.svg");
    } else {
        auto exe = tk::get_executable_path();
        if (!exe.empty()) {
            auto ep = exe.parent_path() / "assets" / "com.taskkiller.app.svg";
            if (std::filesystem::exists(ep, ec)) {
                icon_path = tk::normalize_path_string(ep);
            } else {
                ep = exe.parent_path() / ".." / "assets" / "com.taskkiller.app.svg";
                if (std::filesystem::exists(ep, ec)) {
                    icon_path = tk::normalize_path_string(ep);
                }
            }
        }
        if (icon_path.empty()) {
            icon_path = "appointment-soon-symbolic";
        }
    }

#ifndef _WIN32
    NotifyNotification* n = notify_notification_new(t.title.c_str(), body.c_str(), icon_path.c_str());
    notify_notification_set_urgency(n, (kind == tk::NotifKind::Deadline5m) ? NOTIFY_URGENCY_CRITICAL : NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout(n, NOTIFY_EXPIRES_DEFAULT);

    auto* ctx = g_new(NotifCtx, 1);
    ctx->state = s;
    ctx->task_id = t.id;
    notify_notification_add_action(n, "done", "Done", on_notif_done, ctx, g_free);
    g_signal_connect(n, "closed", G_CALLBACK(on_notif_closed), nullptr);

    if (!notify_notification_show(n, nullptr)) {
        g_object_unref(n);
    }
#else
    (void)s;
    std::string win_body;
    if (kind == tk::NotifKind::Deadline5m) {
        win_body = "5 minutes before deadline - " + tk::format_hm(t.deadline_at);
    } else {
        win_body = "Task ready - Deadline " + tk::format_hm(t.deadline_at);
    }

    auto escape_ps = [](const std::string& str) {
        std::string res;
        for (char c : str) {
            if (c == '\'') res += "''";
            else if (c == '`') res += "``";
            else if (c == '\n' || c == '\r' || c == '\t') res += ' ';
            else res += c;
        }
        return res;
    };

    std::string esc_title = escape_ps(t.title);
    std::string esc_body = escape_ps(win_body);

    // Windows 10/11 requires a registered AUMID to display WinRT toast notifications.
    // Using PowerShell's standard AUMID ({1AC14E77-02E7-4E5D-B744-2EB1AE5198B7}\WindowsPowerShell\v1.0\powershell.exe)
    // guarantees delivery without needing a Start Menu installer shortcut.
    // A try-catch block provides fallback to System.Windows.Forms balloon tips if WinRT is unavailable.
    std::string ps_cmd =
        "try { "
        "  [Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType = WindowsRuntime] > $null; "
        "  $template = [Windows.UI.Notifications.ToastNotificationManager]::GetTemplateContent([Windows.UI.Notifications.ToastTemplateType]::ToastText02); "
        "  $textNodes = $template.GetElementsByTagName('text'); "
        "  $textNodes.Item(0).AppendChild($template.CreateTextNode('" + esc_title + "')) > $null; "
        "  $textNodes.Item(1).AppendChild($template.CreateTextNode('" + esc_body + "')) > $null; "
        "  $appId = '{1AC14E77-02E7-4E5D-B744-2EB1AE5198B7}\\WindowsPowerShell\\v1.0\\powershell.exe'; "
        "  $notifier = [Windows.UI.Notifications.ToastNotificationManager]::CreateToastNotifier($appId); "
        "  $notifier.Show([Windows.UI.Notifications.ToastNotification]::new($template)); "
        "} catch { "
        "  try { "
        "    Add-Type -AssemblyName System.Windows.Forms; "
        "    $notify = New-Object System.Windows.Forms.NotifyIcon; "
        "    $notify.Icon = [System.Drawing.SystemIcons]::Information; "
        "    $notify.BalloonTipTitle = '" + esc_title + "'; "
        "    $notify.BalloonTipText = '" + esc_body + "'; "
        "    $notify.Visible = $true; "
        "    $notify.ShowBalloonTip(5000); "
        "  } catch {} "
        "}";

    std::vector<const char*> argv = {
        "powershell.exe",
        "-NoProfile",
        "-NonInteractive",
        "-ExecutionPolicy", "Bypass",
        "-WindowStyle", "Hidden",
        "-Command",
        ps_cmd.c_str(),
        nullptr
    };

    GError* err = nullptr;
    g_spawn_async(
        nullptr,
        const_cast<char**>(argv.data()),
        nullptr,
        static_cast<GSpawnFlags>(G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL),
        nullptr,
        nullptr,
        nullptr,
        &err
    );
    if (err) {
        g_error_free(err);
    }
#endif

    tk::play_alert_sound();
}

static gboolean on_timer_tick(gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    bool changed = false;
    auto notify_events = s->store.tick(std::time(nullptr), changed);
    for (const auto& ev : notify_events) {
        if (const tk::Task* t = s->store.find(ev.task_id)) {
            send_task_notification(s, *t, ev.kind);
        }
    }
    if (changed) {
        refresh_ui(s);
    }
    return G_SOURCE_CONTINUE;
}

static void on_check_toggled(GtkCheckButton* btn, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    if (!gtk_check_button_get_active(btn)) return;
    long id = widget_task_id(GTK_WIDGET(btn));
    const tk::Task* t = s->store.find(id);
    if (t) {
        time_t now = std::time(nullptr);
        // Play complete sound only if completed on time (before or at deadline)
        if (now <= t->deadline_at) {
            tk::play_complete_sound();
        }
    }
    s->store.set_status(id, tk::Status::Achieved);
    refresh_ui(s);
}

static void on_delete_clicked(GtkButton* btn, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    long id = widget_task_id(GTK_WIDGET(btn));
    const tk::Task* t = s->store.find(id);
    if (t && t->status == tk::Status::Active) {
        s->store.set_status(id, tk::Status::Trash);
    } else {
        s->store.remove_permanently(id);
    }
    refresh_ui(s);
}

static void on_subtask_toggled(GtkCheckButton* btn, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    long task_id = reinterpret_cast<intptr_t>(g_object_get_data(G_OBJECT(btn), "sub-task-id"));
    size_t sub_idx = static_cast<size_t>(reinterpret_cast<intptr_t>(g_object_get_data(G_OBJECT(btn), "sub-idx")));
    bool is_active = gtk_check_button_get_active(btn);

    const tk::Task* t = s->store.find(task_id);
    bool main_completed = s->store.set_subtask_done(task_id, sub_idx, is_active);
    if (main_completed && t) {
        time_t now = std::time(nullptr);
        if (now <= t->deadline_at) {
            tk::play_complete_sound();
        }
    }
    refresh_ui(s);
}

static GtkWidget* create_thumbnail_widget(const std::string& path, int max_w = 260, int max_h = 160);
static gboolean on_editor_key_pressed(GtkEventControllerKey*, guint keyval, guint, GdkModifierType, gpointer);

static GtkWidget* build_task_row(AppState* s, const tk::Task& t) {
    GtkWidget* row_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(row_box, "tk-row");

    // Header bar within card: [Checkbox] [Title] --- [Time] [Delete]
    GtkWidget* header_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_valign(header_row, GTK_ALIGN_CENTER);

    GtkWidget* check = gtk_check_button_new();
    gtk_widget_add_css_class(check, "tk-check");
    gtk_widget_set_valign(check, GTK_ALIGN_CENTER);
    if (t.status == tk::Status::Achieved) {
        gtk_check_button_set_active(GTK_CHECK_BUTTON(check), TRUE);
        gtk_widget_set_can_target(check, FALSE);
    } else {
        set_widget_task_id(check, t.id);
        g_signal_connect(check, "toggled", G_CALLBACK(on_check_toggled), s);
    }
    gtk_box_append(GTK_BOX(header_row), check);

    GtkWidget* title = gtk_label_new(t.title.c_str());
    gtk_widget_add_css_class(title, "tk-task-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_box_append(GTK_BOX(header_row), title);

    std::string hm = tk::format_hm(t.deadline_at);
    GtkWidget* deadline_lbl = gtk_label_new(hm.c_str());
    gtk_widget_add_css_class(deadline_lbl, "tk-task-time");
    gtk_widget_set_valign(deadline_lbl, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(header_row), deadline_lbl);

    GtkWidget* del_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(del_btn, "tk-del-btn");
    gtk_widget_set_valign(del_btn, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(del_btn,
        t.status == tk::Status::Active ? "Remove task" : "Delete permanently");
    set_widget_task_id(del_btn, t.id);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_clicked), s);
    gtk_box_append(GTK_BOX(header_row), del_btn);

    gtk_box_append(GTK_BOX(row_box), header_row);

    // Subtasks / Details section indented directly beneath title
    if (!t.subtasks.empty()) {
        GtkWidget* subs_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_set_margin_start(subs_box, 32);
        gtk_widget_set_margin_top(subs_box, 2);
        for (size_t i = 0; i < t.subtasks.size(); ++i) {
            const auto& sub = t.subtasks[i];
            GtkWidget* sub_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_add_css_class(sub_row, "tk-subtask-row");

            GtkWidget* sub_chk = gtk_check_button_new();
            gtk_widget_add_css_class(sub_chk, "tk-sub-check");
            gtk_check_button_set_active(GTK_CHECK_BUTTON(sub_chk), sub.done);
            if (t.status == tk::Status::Achieved) {
                gtk_widget_set_can_target(sub_chk, FALSE);
            } else {
                g_object_set_data(G_OBJECT(sub_chk), "sub-task-id", reinterpret_cast<gpointer>(static_cast<intptr_t>(t.id)));
                g_object_set_data(G_OBJECT(sub_chk), "sub-idx", reinterpret_cast<gpointer>(static_cast<intptr_t>(i)));
                g_signal_connect(sub_chk, "toggled", G_CALLBACK(on_subtask_toggled), s);
            }
            gtk_box_append(GTK_BOX(sub_row), sub_chk);

            if (!sub.title.empty()) {
                GtkWidget* sub_lbl = gtk_label_new(sub.title.c_str());
                gtk_widget_add_css_class(sub_lbl, "tk-sub-title");
                if (sub.done) {
                    gtk_widget_add_css_class(sub_lbl, "done");
                }
                gtk_label_set_xalign(GTK_LABEL(sub_lbl), 0.0f);
                gtk_label_set_ellipsize(GTK_LABEL(sub_lbl), PANGO_ELLIPSIZE_END);
                gtk_box_append(GTK_BOX(sub_row), sub_lbl);
            }

            if (!sub.image_path.empty()) {
                GtkWidget* thumb = create_thumbnail_widget(sub.image_path, 160, 100);
                if (thumb) {
                    if (sub.done) {
                        gtk_widget_set_opacity(thumb, 0.45);
                    }
                    gtk_box_append(GTK_BOX(sub_row), thumb);
                }
            }

            gtk_box_append(GTK_BOX(subs_box), sub_row);
        }
        gtk_box_append(GTK_BOX(row_box), subs_box);
    } else if (!t.detail.empty()) {
        std::string unesc = tk::strip_image_tags(t.detail);
        if (!unesc.empty()) {
            GtkWidget* detail_lbl = gtk_label_new(unesc.c_str());
            gtk_widget_add_css_class(detail_lbl, "tk-task-detail");
            gtk_widget_set_margin_start(detail_lbl, 32);
            gtk_label_set_xalign(GTK_LABEL(detail_lbl), 0.0f);
            gtk_label_set_wrap(GTK_LABEL(detail_lbl), TRUE);
            gtk_label_set_wrap_mode(GTK_LABEL(detail_lbl), PANGO_WRAP_WORD_CHAR);
            gtk_box_append(GTK_BOX(row_box), detail_lbl);
        }
    }

    GtkWidget* row = gtk_list_box_row_new();
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), row_box);
    return row;
}

static GtkWidget* build_victim_row(const tk::VictimItem& v) {
    GtkWidget* row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(row_box, "tk-victim-row");
    gtk_widget_set_valign(row_box, GTK_ALIGN_CENTER);

    const char* color_cls = (v.outcome == tk::Outcome::OnTime) ? "green"
                          : (v.outcome == tk::Outcome::Late ? "orange" : "red");

    GtkWidget* dot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(dot, "tk-dot");
    gtk_widget_add_css_class(dot, color_cls);
    gtk_widget_set_valign(dot, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(row_box), dot);

    GtkWidget* title = gtk_label_new(v.title.c_str());
    gtk_widget_add_css_class(title, "tk-task-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_box_append(GTK_BOX(row_box), title);

    GtkWidget* delta = gtk_label_new(v.detail.c_str());
    gtk_widget_add_css_class(delta, "tk-delta");
    gtk_widget_add_css_class(delta, color_cls);
    gtk_box_append(GTK_BOX(row_box), delta);

    GtkWidget* row = gtk_list_box_row_new();
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), row_box);
    return row;
}

static void measure_text(GtkWidget* w, const char* text, const char* font_str,
                         int& tw, int& th) {
    PangoLayout* layout = gtk_widget_create_pango_layout(w, text);
    PangoFontDescription* desc = pango_font_description_from_string(font_str);
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    pango_layout_get_pixel_size(layout, &tw, &th);
    pango_font_description_free(desc);
    g_object_unref(layout);
}

static void draw_centered_text(GtkWidget* w, cairo_t* cr, const char* text,
                               double x, double y, double r, double g, double b,
                               const char* font_str) {
    PangoLayout* layout = gtk_widget_create_pango_layout(w, text);
    PangoFontDescription* desc = pango_font_description_from_string(font_str);
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    int tw = 0, th = 0;
    pango_layout_get_pixel_size(layout, &tw, &th);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_move_to(cr, x - tw / 2.0, y - th / 2.0);
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(desc);
    g_object_unref(layout);
}

static double norm_angle_diff(double a, double start) {
    double d = std::fmod(a - start, 2.0 * PI);
    if (d < 0) d += 2.0 * PI;
    return d;
}

static bool box_fits_in_wedge(double cx, double cy, double r,
                              double start, double sweep,
                              double lx, double ly, int w, int h) {
    double hw = w / 2.0 + 3.0;
    double hh = h / 2.0 + 2.5;
    double xs[2] = {lx - hw, lx + hw};
    double ys[2] = {ly - hh, ly + hh};
    double eps = 0.035;
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            double dx = xs[i] - cx;
            double dy = ys[j] - cy;
            if (std::hypot(dx, dy) > r - 4.0) return false;
            double rel = norm_angle_diff(std::atan2(dy, dx), start);
            if (rel <= eps || rel >= sweep - eps) return false;
        }
    }
    return true;
}

static void draw_pie_chart(GtkDrawingArea* area, cairo_t* cr, int width, int height,
                           gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    double cx = width / 2.0;
    double cy = height / 2.0;
    double r = std::min(cx - 36.0, cy - 16.0);
    if (r < 38.0) r = std::min(cx, cy) - 10.0;

    int total = s->current_stats.total();
    if (total == 0) {
        cairo_arc(cr, cx, cy, r, 0, 2 * PI);
        cairo_set_source_rgb(cr, 0.149, 0.149, 0.161);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.196, 0.196, 0.212);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
        draw_centered_text(GTK_WIDGET(area), cr, "No data", cx, cy,
                           0.388, 0.388, 0.400, "SF Pro Text, Inter 10");
        return;
    }

    struct Slice {
        int count;
        double r, g, b;
        double tr, tg, tb;
    };
    const Slice slices[3] = {
        {s->current_stats.on_time, 0.188, 0.820, 0.345, 0.078, 0.078, 0.086},
        {s->current_stats.late,    1.000, 0.624, 0.039, 0.078, 0.078, 0.086},
        {s->current_stats.failed,  1.000, 0.271, 0.227, 0.961, 0.961, 0.969},
    };

    int non_zero = (s->current_stats.on_time > 0) + (s->current_stats.late > 0) +
                   (s->current_stats.failed > 0);
    double start = -PI / 2.0;

    // Pass 1: Draw all pie wedges cleanly
    double starts[3]{}, sweeps[3]{};
    for (int idx = 0; idx < 3; ++idx) {
        const auto& sl = slices[idx];
        if (sl.count <= 0) continue;
        double sweep = (static_cast<double>(sl.count) / total) * 2.0 * PI;
        starts[idx] = start;
        sweeps[idx] = sweep;

        cairo_move_to(cr, cx, cy);
        cairo_arc(cr, cx, cy, r, start, start + sweep);
        cairo_close_path(cr);
        cairo_set_source_rgb(cr, sl.r, sl.g, sl.b);
        if (non_zero > 1) {
            cairo_fill_preserve(cr);
            cairo_set_source_rgb(cr, 0.110, 0.110, 0.118);
            cairo_set_line_width(cr, 1.5);
            cairo_stroke(cr);
        } else {
            cairo_fill(cr);
        }
        start += sweep;
    }

    // Pass 2: Flexible text placement (fits inside wedge, or clean outside callout)
    std::vector<double> placed_y[2];
    for (int idx = 0; idx < 3; ++idx) {
        const auto& sl = slices[idx];
        if (sl.count <= 0) continue;

        long pct = std::llround((sl.count * 100.0) / total);
        std::string two_line = std::to_string(sl.count) + "\n" + std::to_string(pct) + "%";
        std::string one_line = std::to_string(sl.count) + " (" + std::to_string(pct) + "%)";

        if (non_zero == 1) {
            draw_centered_text(GTK_WIDGET(area), cr, two_line.c_str(), cx, cy,
                               sl.tr, sl.tg, sl.tb, "SF Pro Text, Inter 11 Bold");
            continue;
        }

        double mid = starts[idx] + sweeps[idx] / 2.0;
        bool placed = false;

        struct Candidate { const char* txt; const char* font; };
        const Candidate cands[5] = {
            {two_line.c_str(), "SF Pro Text, Inter 10 Bold"},
            {two_line.c_str(), "SF Pro Text, Inter 8.5 Bold"},
            {one_line.c_str(), "SF Pro Text, Inter 8 Bold"},
            {two_line.c_str(), "SF Pro Text, Inter 7.5 Bold"},
            {one_line.c_str(), "SF Pro Text, Inter 7.5 Bold"},
        };
        const double radii[4] = {0.56, 0.64, 0.72, 0.78};

        for (const auto& c : cands) {
            int tw = 0, th = 0;
            measure_text(GTK_WIDGET(area), c.txt, c.font, tw, th);
            for (double rf : radii) {
                double lx = cx + (r * rf) * std::cos(mid);
                double ly = cy + (r * rf) * std::sin(mid);
                if (box_fits_in_wedge(cx, cy, r, starts[idx], sweeps[idx],
                                      lx, ly, tw, th)) {
                    draw_centered_text(GTK_WIDGET(area), cr, c.txt, lx, ly,
                                       sl.tr, sl.tg, sl.tb, c.font);
                    placed = true;
                    break;
                }
            }
            if (placed) break;
        }

        if (!placed) {
            int tw = 0, th = 0;
            const char* fstr = "SF Pro Text, Inter 8.5 Bold";
            measure_text(GTK_WIDGET(area), one_line.c_str(), fstr, tw, th);

            double cos_m = std::cos(mid);
            double sin_m = std::sin(mid);
            int side = (cos_m >= 0.0) ? 1 : 0;
            double dir = (side == 1) ? 1.0 : -1.0;

            double x0 = cx + (r - 1.0) * cos_m;
            double y0 = cy + (r - 1.0) * sin_m;
            double x1 = cx + (r + 12.0) * cos_m;
            double y1 = cy + (r + 12.0) * sin_m;

            double min_y = th / 2.0 + 3.0;
            double max_y = height - th / 2.0 - 3.0;
            if (y1 < min_y) y1 = min_y;
            if (y1 > max_y) y1 = max_y;

            const double min_gap = static_cast<double>(th) + 5.0;
            for (int pass = 0; pass < 3; ++pass) {
                for (double py : placed_y[side]) {
                    if (std::abs(y1 - py) < min_gap) {
                        double down = py + min_gap;
                        double up = py - min_gap;
                        if (sin_m <= 0.0) {
                            y1 = (down <= max_y) ? down : up;
                        } else {
                            y1 = (up >= min_y) ? up : down;
                        }
                    }
                }
            }
            if (y1 < min_y) y1 = min_y;
            if (y1 > max_y) y1 = max_y;
            placed_y[side].push_back(y1);

            double dy = y1 - cy;
            double min_dx = 0.0;
            if (std::abs(dy) < r + 8.0) {
                min_dx = std::sqrt((r + 8.0) * (r + 8.0) - dy * dy) + 6.0;
            }
            double x2 = x1 + dir * 10.0;
            if (dir * (x2 - cx) < min_dx) {
                x2 = cx + dir * min_dx;
            }
            double max_x2 = (side == 1) ? (width - tw - 8.0) : (tw + 8.0);
            if ((side == 1 && x2 > max_x2) || (side == 0 && x2 < max_x2)) {
                x2 = max_x2;
            }

            cairo_set_source_rgb(cr, sl.r, sl.g, sl.b);
            cairo_set_line_width(cr, 1.25);
            cairo_move_to(cr, x0, y0);
            cairo_line_to(cr, x1, y1);
            cairo_line_to(cr, x2, y1);
            cairo_stroke(cr);

            double tx = x2 + dir * (tw / 2.0 + 4.0);
            if (tx < tw / 2.0 + 2.0) tx = tw / 2.0 + 2.0;
            if (tx > width - tw / 2.0 - 2.0) tx = width - tw / 2.0 - 2.0;
            draw_centered_text(GTK_WIDGET(area), cr, one_line.c_str(), tx, y1,
                               sl.r, sl.g, sl.b, fstr);
        }
    }
}

static void sync_calendar_tm(GtkCalendar* cal, const std::tm& d) {
    std::tm tmp = d;
    time_t t = std::mktime(&tmp);
    GDateTime* dt = g_date_time_new_from_unix_local(t);
    gtk_calendar_select_day(cal, dt);
    g_date_time_unref(dt);
}

static void refresh_stats_view(AppState* s) {
    int sc = static_cast<int>(s->stat_scope);
    bool is_range = (s->stat_scope == tk::StatScope::Range);

    for (int i = 0; i < 4; ++i) {
        if (i == sc) gtk_widget_add_css_class(s->scope_btn[i], "active");
        else gtk_widget_remove_css_class(s->scope_btn[i], "active");

        // When in Range mode, hide Day, Month, Year buttons and keep Range in the center
        if (is_range) {
            gtk_widget_set_visible(s->scope_btn[i], (i == 0));
        } else {
            gtk_widget_set_visible(s->scope_btn[i], TRUE);
        }
    }

    gtk_widget_set_visible(s->period_nav_box, !is_range);
    gtk_widget_set_visible(s->range_bar, is_range);

    s->syncing_stat_cal = true;
    if (is_range) {
        gtk_menu_button_set_label(GTK_MENU_BUTTON(s->from_menu_btn),
            tk::format_period_label(s->range_from, tk::StatScope::Range).c_str());
        gtk_menu_button_set_label(GTK_MENU_BUTTON(s->to_menu_btn),
            tk::format_period_label(s->range_to, tk::StatScope::Range).c_str());
        sync_calendar_tm(GTK_CALENDAR(s->from_calendar), s->range_from);
        sync_calendar_tm(GTK_CALENDAR(s->to_calendar), s->range_to);

        s->current_stats = s->store.compute_stats_range(
            tk::day_start(s->range_from), tk::day_end(s->range_to));
    } else {
        std::string period_txt = tk::format_period_label(s->stat_date, s->stat_scope);
        gtk_menu_button_set_label(GTK_MENU_BUTTON(s->period_menu_btn), period_txt.c_str());
        sync_calendar_tm(GTK_CALENDAR(s->stat_calendar), s->stat_date);

        s->current_stats = s->store.compute_stats(s->stat_date, s->stat_scope);
    }
    s->syncing_stat_cal = false;

    int killed = s->current_stats.total_completed();
    std::string total_txt = (killed > 0)
        ? ("Congratulations! You have killed " + std::to_string(killed) +
           (killed == 1 ? " task." : " tasks."))
        : "You have killed 0 tasks.";
    gtk_label_set_text(GTK_LABEL(s->stat_total_lbl), total_txt.c_str());
    gtk_widget_queue_draw(s->pie_area);

    gtk_list_box_remove_all(GTK_LIST_BOX(s->victims_list));
    for (const auto& v : s->current_stats.victims) {
        gtk_list_box_append(GTK_LIST_BOX(s->victims_list), build_victim_row(v));
    }
    gtk_widget_set_visible(s->victims_empty, s->current_stats.victims.empty());
}

static void refresh_ui(AppState* s) {
    // Update active task count
    int active_count = s->store.count(tk::Status::Active);
    std::string count_str = "(" + std::to_string(active_count) + ")";
    if (s->view_count_lbl) {
        gtk_label_set_text(GTK_LABEL(s->view_count_lbl), count_str.c_str());
    }

    // Refresh active tasks list
    gtk_list_box_remove_all(GTK_LIST_BOX(s->list_box));
    for (const auto& t : s->store.tasks) {
        if (t.status == tk::Status::Active) {
            gtk_list_box_append(GTK_LIST_BOX(s->list_box), build_task_row(s, t));
        }
    }

    // Update menu achievements item label with count
    int achiev_count = s->store.count(tk::Status::Achieved);
    std::string achiev_menu_txt = "Achievements (" + std::to_string(achiev_count) + ")";
    if (s->menu_achiev_btn) {
        gtk_button_set_label(GTK_BUTTON(s->menu_achiev_btn), achiev_menu_txt.c_str());
    }

    // Update side drawer panel content if open
    bool panel_open = !s->active_side_page.empty();
    gtk_widget_set_visible(s->side_panel, panel_open);
    if (s->menu_btn) {
        if (panel_open) gtk_widget_add_css_class(s->menu_btn, "active");
        else gtk_widget_remove_css_class(s->menu_btn, "active");
    }

    if (s->active_side_page == "achiev") {
        gtk_label_set_text(GTK_LABEL(s->side_title), ("Achievements (" + std::to_string(achiev_count) + ")").c_str());
        gtk_stack_set_visible_child_name(GTK_STACK(s->side_stack), "achiev");
        gtk_list_box_remove_all(GTK_LIST_BOX(s->achiev_list_box));
        int achiev_rows = 0;
        for (const auto& t : s->store.tasks) {
            if (t.status == tk::Status::Achieved) {
                gtk_list_box_append(GTK_LIST_BOX(s->achiev_list_box), build_task_row(s, t));
                ++achiev_rows;
            }
        }
        gtk_widget_set_visible(s->achiev_empty_label, achiev_rows == 0);
    } else if (s->active_side_page == "stats") {
        gtk_label_set_text(GTK_LABEL(s->side_title), "Statistics");
        gtk_stack_set_visible_child_name(GTK_STACK(s->side_stack), "stats");
        refresh_stats_view(s);
    }
}

static void on_open_side_page(AppState* s, const std::string& page) {
    if (s->active_side_page == page) {
        s->active_side_page.clear();
    } else {
        s->active_side_page = page;
    }
    if (s->menu_popover) {
        gtk_popover_popdown(GTK_POPOVER(s->menu_popover));
    }
    refresh_ui(s);
}

static void on_menu_achiev_clicked(GtkButton*, gpointer user_data) {
    on_open_side_page(static_cast<AppState*>(user_data), "achiev");
}

static void on_menu_stats_clicked(GtkButton*, gpointer user_data) {
    on_open_side_page(static_cast<AppState*>(user_data), "stats");
}

static void on_side_close_clicked(GtkButton*, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    s->active_side_page.clear();
    refresh_ui(s);
}

static void on_scope_clicked(GtkButton* btn, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    int idx = static_cast<int>(
        reinterpret_cast<intptr_t>(g_object_get_data(G_OBJECT(btn), "scope-idx")));
    if (idx == 0 && s->stat_scope == tk::StatScope::Range) {
        // Clicking Range again toggles back to Day view
        s->stat_scope = tk::StatScope::Day;
    } else {
        s->stat_scope = static_cast<tk::StatScope>(idx);
    }
    refresh_stats_view(s);
}

static std::tm tm_from_calendar(GtkCalendar* cal) {
    GDateTime* dt = gtk_calendar_get_date(cal);
    std::tm out{};
    out.tm_year = g_date_time_get_year(dt) - 1900;
    out.tm_mon = g_date_time_get_month(dt) - 1;
    out.tm_mday = g_date_time_get_day_of_month(dt);
    out.tm_isdst = -1;
    g_date_time_unref(dt);
    std::mktime(&out);
    return out;
}

static void on_from_cal_selected(GtkCalendar* cal, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    if (s->syncing_stat_cal) return;
    s->range_from = tm_from_calendar(cal);
    if (tk::day_start(s->range_from) > tk::day_start(s->range_to)) {
        s->range_to = s->range_from;
    }
    gtk_popover_popdown(GTK_POPOVER(s->from_popover));
    refresh_stats_view(s);
}

static void on_to_cal_selected(GtkCalendar* cal, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    if (s->syncing_stat_cal) return;
    s->range_to = tm_from_calendar(cal);
    if (tk::day_start(s->range_to) < tk::day_start(s->range_from)) {
        s->range_from = s->range_to;
    }
    gtk_popover_popdown(GTK_POPOVER(s->to_popover));
    refresh_stats_view(s);
}

static void step_stat_period(AppState* s, int dir) {
    if (s->stat_scope == tk::StatScope::Day) {
        s->stat_date.tm_mday += dir;
    } else if (s->stat_scope == tk::StatScope::Month) {
        s->stat_date.tm_mday = 1;
        s->stat_date.tm_mon += dir;
    } else {
        s->stat_date.tm_mday = 1;
        s->stat_date.tm_mon = 0;
        s->stat_date.tm_year += dir;
    }
    std::mktime(&s->stat_date);
    refresh_stats_view(s);
}

static void on_period_prev(GtkButton*, gpointer user_data) {
    step_stat_period(static_cast<AppState*>(user_data), -1);
}

static void on_period_next(GtkButton*, gpointer user_data) {
    step_stat_period(static_cast<AppState*>(user_data), 1);
}

static void on_stat_cal_selected(GtkCalendar* cal, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    if (s->syncing_stat_cal) return;
    s->stat_date = tm_from_calendar(cal);
    gtk_popover_popdown(GTK_POPOVER(s->period_popover));
    refresh_stats_view(s);
}

static void cascade_draft_times(AppState* s) {
    if (s->active_slot == 0) {
        if (!s->slot_touched[1]) s->draft_times[1] = s->draft_times[0] + 3600;
        if (!s->slot_touched[2]) s->draft_times[2] = s->draft_times[1];
    } else if (s->active_slot == 1 && !s->slot_touched[2]) {
        s->draft_times[2] = s->draft_times[1];
    }
}

static void sync_picker_ui(AppState* s) {
    s->syncing_picker = true;
    time_t now = std::time(nullptr);

    for (int i = 0; i < 3; ++i) {
        if (i == s->active_slot) gtk_widget_add_css_class(s->seg_btn[i], "active");
        else gtk_widget_remove_css_class(s->seg_btn[i], "active");
        std::string txt = tk::format_slot(s->draft_times[i], now);
        gtk_label_set_text(GTK_LABEL(s->seg_val[i]), txt.c_str());
    }

    time_t cur = s->draft_times[s->active_slot];
    GDateTime* dt = g_date_time_new_from_unix_local(cur);
    gtk_calendar_select_day(GTK_CALENDAR(s->calendar), dt);
    g_date_time_unref(dt);

    std::tm tm_cur{};
    tk::localtime_safe(&cur, &tm_cur);
    char hbuf[4], mbuf[4];
    std::snprintf(hbuf, sizeof(hbuf), "%02d", tm_cur.tm_hour);
    std::snprintf(mbuf, sizeof(mbuf), "%02d", tm_cur.tm_min);
    gtk_button_set_label(GTK_BUTTON(s->hour_btn), hbuf);
    gtk_button_set_label(GTK_BUTTON(s->min_btn), mbuf);
    gtk_button_set_label(GTK_BUTTON(s->ampm_btn), tm_cur.tm_hour >= 12 ? "PM" : "AM");

    if (s->editing_minute) {
        gtk_widget_remove_css_class(s->hour_btn, "active");
        gtk_widget_add_css_class(s->min_btn, "active");
    } else {
        gtk_widget_add_css_class(s->hour_btn, "active");
        gtk_widget_remove_css_class(s->min_btn, "active");
    }

    gtk_widget_queue_draw(s->clock_area);
    s->syncing_picker = false;
}

static void draw_clock_face(GtkDrawingArea* area, cairo_t* cr, int width, int height,
                            gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    double cx = width / 2.0;
    double cy = height / 2.0;
    double dial_r = std::min(cx, cy) - 4.0;
    double num_r = dial_r - 17.0;

    std::tm tm_cur{};
    tk::localtime_safe(&s->draft_times[s->active_slot], &tm_cur);
    bool is_pm = tm_cur.tm_hour >= 12;

    cairo_arc(cr, cx, cy, dial_r, 0, 2 * PI);
    cairo_set_source_rgb(cr, 0.11, 0.11, 0.125);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.21, 0.21, 0.23);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    int steps = s->editing_minute ? 60 : 12;
    int val = s->editing_minute ? tm_cur.tm_min : (tm_cur.tm_hour % 12);
    double angle = (static_cast<double>(val) / steps) * 2.0 * PI;
    double hx = cx + num_r * std::sin(angle);
    double hy = cy - num_r * std::cos(angle);

    cairo_set_source_rgb(cr, 0.898, 0.898, 0.918);
    cairo_set_line_width(cr, 1.75);
    cairo_move_to(cr, cx, cy);
    cairo_line_to(cr, hx, hy);
    cairo_stroke(cr);

    cairo_arc(cr, cx, cy, 3.0, 0, 2 * PI);
    cairo_fill(cr);

    cairo_arc(cr, hx, hy, 12.0, 0, 2 * PI);
    cairo_set_source_rgb(cr, 0.898, 0.898, 0.918);
    cairo_fill(cr);

    for (int i = 0; i < 12; ++i) {
        double a = (static_cast<double>(i) / 12.0) * 2.0 * PI;
        double px = cx + num_r * std::sin(a);
        double py = cy - num_r * std::cos(a);

        char label[4];
        bool selected = false;
        if (s->editing_minute) {
            int m_mark = i * 5;
            std::snprintf(label, sizeof(label), "%02d", m_mark);
            selected = (tm_cur.tm_min == m_mark);
        } else {
            int h_mark = is_pm ? (i + 12) : i;
            std::snprintf(label, sizeof(label), "%02d", h_mark);
            selected = (tm_cur.tm_hour == h_mark);
        }

        if (selected) {
            draw_centered_text(GTK_WIDGET(area), cr, label, px, py,
                               0.08, 0.08, 0.09, "SF Pro Text, Inter 9.5 Bold");
        } else {
            draw_centered_text(GTK_WIDGET(area), cr, label, px, py,
                               0.56, 0.56, 0.58, "SF Pro Text, Inter 9");
        }
    }
}

static void apply_clock_point(AppState* s, double x, double y) {
    int w = gtk_widget_get_width(s->clock_area);
    int h = gtk_widget_get_height(s->clock_area);
    double cx = w / 2.0;
    double cy = h / 2.0;
    double dx = x - cx;
    double dy = y - cy;
    if (dx * dx + dy * dy < 36.0) return;

    double theta = std::atan2(dx, -dy);
    if (theta < 0) theta += 2.0 * PI;

    std::tm tm_cur{};
    tk::localtime_safe(&s->draft_times[s->active_slot], &tm_cur);

    if (s->editing_minute) {
        int m = static_cast<int>(std::llround((theta / (2.0 * PI)) * 60.0)) % 60;
        tm_cur.tm_min = m;
    } else {
        int pos = static_cast<int>(std::llround((theta / (2.0 * PI)) * 12.0)) % 12;
        bool is_pm = tm_cur.tm_hour >= 12;
        tm_cur.tm_hour = is_pm ? (pos + 12) : pos;
    }

    s->draft_times[s->active_slot] = tk::with_hm(
        s->draft_times[s->active_slot], tm_cur.tm_hour, tm_cur.tm_min);
    s->slot_touched[s->active_slot] = true;
    cascade_draft_times(s);
    sync_picker_ui(s);
}

static void on_clock_drag_begin(GtkGestureDrag*, double x, double y, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    s->drag_x0 = x;
    s->drag_y0 = y;
    apply_clock_point(s, x, y);
}

static void on_clock_drag_update(GtkGestureDrag*, double dx, double dy, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    apply_clock_point(s, s->drag_x0 + dx, s->drag_y0 + dy);
}

static void on_clock_drag_end(GtkGestureDrag*, double, double, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    if (!s->editing_minute) {
        s->editing_minute = true;
        sync_picker_ui(s);
    }
}

static void on_calendar_day_selected(GtkCalendar* cal, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    if (s->syncing_picker) return;

    GDateTime* dt = gtk_calendar_get_date(cal);
    s->draft_times[s->active_slot] = tk::with_date(
        s->draft_times[s->active_slot],
        g_date_time_get_year(dt),
        g_date_time_get_month(dt),
        g_date_time_get_day_of_month(dt));
    g_date_time_unref(dt);

    s->slot_touched[s->active_slot] = true;
    cascade_draft_times(s);
    sync_picker_ui(s);
}

static void on_seg_clicked(GtkButton* btn, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    int idx = static_cast<int>(
        reinterpret_cast<intptr_t>(g_object_get_data(G_OBJECT(btn), "seg-idx")));
    s->active_slot = idx;
    s->editing_minute = false;
    sync_picker_ui(s);
}

static void on_hour_btn_clicked(GtkButton*, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    s->editing_minute = false;
    sync_picker_ui(s);
}

static void on_min_btn_clicked(GtkButton*, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    s->editing_minute = true;
    sync_picker_ui(s);
}

static void on_ampm_clicked(GtkButton*, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    std::tm tm_cur{};
    tk::localtime_safe(&s->draft_times[s->active_slot], &tm_cur);
    int new_hour = (tm_cur.tm_hour >= 12) ? (tm_cur.tm_hour - 12) : (tm_cur.tm_hour + 12);
    s->draft_times[s->active_slot] = tk::with_hm(
        s->draft_times[s->active_slot], new_hour, tm_cur.tm_min);
    s->slot_touched[s->active_slot] = true;
    cascade_draft_times(s);
    sync_picker_ui(s);
}

static gboolean on_time_scroll(GtkEventControllerScroll* ctrl, double, double dy,
                               gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    GtkWidget* w = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(ctrl));
    bool is_min = (w == s->min_btn);
    int step = (dy < 0) ? 1 : -1;

    std::tm tm_cur{};
    tk::localtime_safe(&s->draft_times[s->active_slot], &tm_cur);
    if (is_min) {
        tm_cur.tm_min = (tm_cur.tm_min + step + 60) % 60;
        s->editing_minute = true;
    } else {
        tm_cur.tm_hour = (tm_cur.tm_hour + step + 24) % 24;
        s->editing_minute = false;
    }

    s->draft_times[s->active_slot] = tk::with_hm(
        s->draft_times[s->active_slot], tm_cur.tm_hour, tm_cur.tm_min);
    s->slot_touched[s->active_slot] = true;
    cascade_draft_times(s);
    sync_picker_ui(s);
    return TRUE;
}

static std::filesystem::path get_images_storage_dir() {
    auto p = tk::get_default_data_dir() / "images";
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    return p;
}

static bool is_image_file(const std::string& path) {
    std::error_code ec;
    if (path.empty() || !std::filesystem::exists(path, ec) || std::filesystem::is_directory(path, ec)) {
        return false;
    }
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" ||
            ext == ".bmp" || ext == ".gif" || ext == ".svg" || ext == ".tiff" ||
            ext == ".tif" || ext == ".ico" || ext == ".avif");
}

static std::vector<std::string> parse_paths_or_uris(const std::string& input) {
    std::vector<std::string> results;
    std::istringstream ss(input);
    std::string line;
    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        size_t start = 0;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) {
            ++start;
        }
        if (start >= line.size()) continue;
        std::string item = line.substr(start);

        if (item.rfind("file://", 0) == 0) {
            char* fpath = g_filename_from_uri(item.c_str(), nullptr, nullptr);
            if (fpath) {
                results.push_back(std::string(fpath));
                g_free(fpath);
            }
        } else if (item[0] == '/' || (item.size() >= 3 && std::isalpha(static_cast<unsigned char>(item[0])) && item[1] == ':' && (item[2] == '\\' || item[2] == '/'))) {
            results.push_back(item);
        }
    }
    return results;
}

static std::string save_image_file_to_storage(const std::string& src_path) {
    std::error_code ec;
    if (!std::filesystem::exists(src_path, ec) || std::filesystem::is_directory(src_path, ec)) return "";
    auto dir = get_images_storage_dir();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string ext = std::filesystem::path(src_path).extension().string();
    if (ext.empty()) ext = ".png";
    std::string filename = "img_" + std::to_string(now_ms) + ext;
    auto dst = dir / filename;
    int counter = 1;
    while (std::filesystem::exists(dst, ec)) {
        filename = "img_" + std::to_string(now_ms) + "_" + std::to_string(counter++) + ext;
        dst = dir / filename;
    }
    std::filesystem::copy_file(src_path, dst, std::filesystem::copy_options::overwrite_existing, ec);
    if (!ec) return dst.string();
    return "";
}

static std::string save_texture_to_storage(GdkTexture* tex) {
    if (!tex) return "";
    int w = gdk_texture_get_width(tex);
    int h = gdk_texture_get_height(tex);
    if (w <= 0 || h <= 0) return "";
    auto dir = get_images_storage_dir();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string filename = "img_" + std::to_string(now_ms) + ".png";
    auto dst = dir / filename;
    std::error_code ec;
    int counter = 1;
    while (std::filesystem::exists(dst, ec)) {
        filename = "img_" + std::to_string(now_ms) + "_" + std::to_string(counter++) + ".png";
        dst = dir / filename;
    }
    if (gdk_texture_save_to_png(tex, dst.string().c_str())) {
        return dst.string();
    }
    return "";
}

static GtkWidget* create_thumbnail_widget(const std::string& path, int max_w, int max_h) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return nullptr;
    GError* err = nullptr;
    GdkTexture* tex = gdk_texture_new_from_filename(path.c_str(), &err);
    if (!tex) {
        if (err) g_error_free(err);
        return nullptr;
    }
    int orig_w = gdk_texture_get_width(tex);
    int orig_h = gdk_texture_get_height(tex);
    if (orig_w <= 8 || orig_h <= 8) {
        g_object_unref(tex);
        return nullptr;
    }
    double sw = static_cast<double>(max_w) / orig_w;
    double sh = static_cast<double>(max_h) / orig_h;
    double scale = std::min(1.0, std::min(sw, sh));
    int target_w = std::max(24, static_cast<int>(orig_w * scale));
    int target_h = std::max(24, static_cast<int>(orig_h * scale));

    GtkWidget* pic = gtk_picture_new_for_paintable(GDK_PAINTABLE(tex));
    gtk_picture_set_can_shrink(GTK_PICTURE(pic), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_COVER);
    gtk_widget_set_size_request(pic, target_w, target_h);
    gtk_widget_add_css_class(pic, "tk-detail-img");

    // Click to open in default viewer
    GtkGesture* click = gtk_gesture_click_new();
    char* copy_path = g_strdup(path.c_str());
    g_object_set_data_full(G_OBJECT(pic), "img-path-data", copy_path, g_free);
    g_signal_connect(click, "pressed", G_CALLBACK(+[](GtkGestureClick*, int, double, double, gpointer ud) {
        const char* p = static_cast<const char*>(ud);
        if (p) {
            GFile* f = g_file_new_for_path(p);
            char* uri = g_file_get_uri(f);
            g_app_info_launch_default_for_uri(uri, nullptr, nullptr);
            g_free(uri);
            g_object_unref(f);
        }
    }), copy_path);
    gtk_widget_add_controller(pic, GTK_EVENT_CONTROLLER(click));
    gtk_widget_set_cursor_from_name(pic, "pointer");
    gtk_widget_set_tooltip_text(pic, "Click to view image");

    g_object_unref(tex);
    return pic;
}

static std::string serialize_detail_buffer(GtkTextBuffer* buf) {
    if (!buf) return "";
    GtkTextIter iter;
    gtk_text_buffer_get_start_iter(buf, &iter);
    std::string result;
    while (!gtk_text_iter_is_end(&iter)) {
        GdkPaintable* p = gtk_text_iter_get_paintable(&iter);
        if (p) {
            const char* path = static_cast<const char*>(g_object_get_data(G_OBJECT(p), "img-path"));
            if (path && *path) {
                result += "[img:";
                result += path;
                result += "]";
            }
            gtk_text_iter_forward_char(&iter);
            continue;
        }
        gunichar ch = gtk_text_iter_get_char(&iter);
        if (ch == 0xFFFC) {
            gtk_text_iter_forward_char(&iter);
            continue;
        }
        if (ch != 0) {
            char utf8[8] = {0};
            int len = g_unichar_to_utf8(ch, utf8);
            result.append(utf8, len);
        }
        gtk_text_iter_forward_char(&iter);
    }
    return result;
}

static void populate_detail_buffer(GtkTextBuffer* buf, const std::string& raw) {
    if (!buf) return;
    gtk_text_buffer_set_text(buf, "", -1);
    std::string text = tk::unescape_newlines(raw);
    GtkTextIter iter;
    gtk_text_buffer_get_start_iter(buf, &iter);
    size_t pos = 0;
    while (pos < text.size()) {
        size_t img_start = text.find("[img:", pos);
        if (img_start != std::string::npos) {
            if (img_start > pos) {
                std::string chunk = text.substr(pos, img_start - pos);
                gtk_text_buffer_insert(buf, &iter, chunk.c_str(), chunk.size());
            }
            size_t img_end = text.find("]", img_start + 5);
            if (img_end != std::string::npos) {
                std::string path = text.substr(img_start + 5, img_end - (img_start + 5));
                std::error_code ec;
                if (std::filesystem::exists(path, ec)) {
                    GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(path.c_str(), 180, 110, TRUE, nullptr);
                    if (pb) {
                        GdkTexture* tex = gdk_texture_new_for_pixbuf(pb);
                        g_object_unref(pb);
                        if (tex) {
                            g_object_set_data_full(G_OBJECT(tex), "img-path", g_strdup(path.c_str()), g_free);
                            gtk_text_buffer_insert_paintable(buf, &iter, GDK_PAINTABLE(tex));
                            g_object_unref(tex);
                        }
                    }
                }
                pos = img_end + 1;
            } else {
                std::string rest = text.substr(img_start);
                gtk_text_buffer_insert(buf, &iter, rest.c_str(), rest.size());
                break;
            }
        } else {
            std::string rest = text.substr(pos);
            gtk_text_buffer_insert(buf, &iter, rest.c_str(), rest.size());
            break;
        }
    }
}

static void open_add_composer(AppState* s);

static void insert_image_into_detail_view(AppState* s, const std::string& saved_path) {
    if (!s || saved_path.empty()) return;
    std::error_code ec;
    if (!std::filesystem::exists(saved_path, ec)) return;

    if (!s->add_expanded) {
        open_add_composer(s);
    }
    if (!s->detail_view) return;

    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(s->detail_view));
    if (!buf) return;

    GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(saved_path.c_str(), 180, 110, TRUE, nullptr);
    if (!pb) return;
    GdkTexture* tex = gdk_texture_new_for_pixbuf(pb);
    g_object_unref(pb);
    if (!tex) return;

    g_object_set_data_full(G_OBJECT(tex), "img-path", g_strdup(saved_path.c_str()), g_free);

    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buf, &iter, gtk_text_buffer_get_insert(buf));

    // If iter is not at the start of a line and previous char is not newline, add newline first
    if (!gtk_text_iter_starts_line(&iter)) {
        gunichar prev_ch = 0;
        GtkTextIter prev_iter = iter;
        if (gtk_text_iter_backward_char(&prev_iter)) {
            prev_ch = gtk_text_iter_get_char(&prev_iter);
        }
        if (prev_ch != '\n') {
            gtk_text_buffer_insert(buf, &iter, "\n", 1);
        }
    }

    gtk_text_buffer_insert_paintable(buf, &iter, GDK_PAINTABLE(tex));
    g_object_unref(tex);

    // Enter newline after image so the next detail is on a new line!
    gtk_text_buffer_insert(buf, &iter, "\n", 1);

    tk::play_click_sound();
}

static std::string read_entire_stream_as_string(GInputStream* in) {
    if (!in) return "";
    std::string out;
    char buf[4096];
    gssize n = 0;
    while ((n = g_input_stream_read(in, buf, sizeof(buf), nullptr, nullptr)) > 0) {
        out.append(buf, n);
    }
    return out;
}

static std::string save_stream_to_storage(GInputStream* in, const std::string& mime) {
    if (!in) return "";
    auto dir = get_images_storage_dir();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string ext = ".png";
    if (mime.find("jpeg") != std::string::npos || mime.find("jpg") != std::string::npos) ext = ".jpg";
    else if (mime.find("webp") != std::string::npos) ext = ".webp";
    else if (mime.find("bmp") != std::string::npos) ext = ".bmp";
    else if (mime.find("gif") != std::string::npos) ext = ".gif";

    std::string filename = "img_" + std::to_string(now_ms) + ext;
    auto dst = dir / filename;
    std::error_code ec;
    int counter = 1;
    while (std::filesystem::exists(dst, ec)) {
        filename = "img_" + std::to_string(now_ms) + "_" + std::to_string(counter++) + ext;
        dst = dir / filename;
    }

    std::ofstream ofs(dst, std::ios::binary);
    if (!ofs.is_open()) return "";

    char buf[8192];
    gssize n = 0;
    while ((n = g_input_stream_read(in, buf, sizeof(buf), nullptr, nullptr)) > 0) {
        ofs.write(buf, n);
    }
    ofs.close();

    if (std::filesystem::exists(dst, ec) && std::filesystem::file_size(dst, ec) > 0) {
        return dst.string();
    }
    std::filesystem::remove(dst, ec);
    return "";
}

static bool clipboard_has_image(GdkClipboard* clip) {
    if (!clip) return false;
    GdkContentFormats* f = gdk_clipboard_get_formats(clip);
    if (!f) return false;
    if (gdk_content_formats_contain_gtype(f, GDK_TYPE_TEXTURE)) return true;
    if (gdk_content_formats_contain_gtype(f, GDK_TYPE_FILE_LIST)) return true;
    gsize n = 0;
    const char* const* mimes = gdk_content_formats_get_mime_types(f, &n);
    for (gsize i = 0; i < n; ++i) {
        std::string m = mimes[i];
        if (m.rfind("image/", 0) == 0) return true;
        if (m == "text/uri-list" || m == "x-special/gnome-copied-files") return true;
    }
    return false;
}

static void read_clipboard_and_insert_image(AppState* s, GdkClipboard* clip, bool from_key_press = false) {
    if (!s || !clip) return;
    GdkContentFormats* f = gdk_clipboard_get_formats(clip);
    if (!f) return;

    bool has_texture = gdk_content_formats_contain_gtype(f, GDK_TYPE_TEXTURE);
    bool has_image_mime = false;
    bool has_uri_mime = false;
    gsize n = 0;
    const char* const* mimes = gdk_content_formats_get_mime_types(f, &n);
    for (gsize i = 0; i < n; ++i) {
        std::string m = mimes[i];
        if (m.rfind("image/", 0) == 0) has_image_mime = true;
        if (m == "text/uri-list" || m == "x-special/gnome-copied-files") has_uri_mime = true;
    }

    if (has_texture || has_image_mime) {
        gdk_clipboard_read_texture_async(clip, nullptr, [](GObject* src, GAsyncResult* res, gpointer ud) {
            auto* app = static_cast<AppState*>(ud);
            GError* err = nullptr;
            GdkTexture* tex = gdk_clipboard_read_texture_finish(GDK_CLIPBOARD(src), res, &err);
            if (tex) {
                int w = gdk_texture_get_width(tex);
                int h = gdk_texture_get_height(tex);
                if (w > 0 && h > 0) {
                    std::string saved = save_texture_to_storage(tex);
                    if (!saved.empty()) {
                        insert_image_into_detail_view(app, saved);
                    }
                }
                g_object_unref(tex);
            } else {
                if (err) g_error_free(err);
                const char* img_mimes[] = {
                    "image/png", "image/jpeg", "image/webp", "image/bmp", "image/gif", "image/tiff", nullptr
                };
                gdk_clipboard_read_async(GDK_CLIPBOARD(src), img_mimes, G_PRIORITY_DEFAULT, nullptr,
                    [](GObject* s2, GAsyncResult* r2, gpointer ud2) {
                        auto* app2 = static_cast<AppState*>(ud2);
                        const char* out_mime = nullptr;
                        GInputStream* in = gdk_clipboard_read_finish(GDK_CLIPBOARD(s2), r2, &out_mime, nullptr);
                        if (in) {
                            std::string saved = save_stream_to_storage(in, out_mime ? out_mime : "image/png");
                            g_object_unref(in);
                            if (!saved.empty()) {
                                insert_image_into_detail_view(app2, saved);
                            }
                        }
                    }, app);
            }
        }, s);
        return;
    }

    if (has_uri_mime || gdk_content_formats_contain_gtype(f, GDK_TYPE_FILE_LIST)) {
        struct UriCtx {
            AppState* app;
            bool from_key_press;
        };
        auto* ctx = new UriCtx{s, from_key_press};
        const char* uri_mimes[] = { "text/uri-list", "x-special/gnome-copied-files", "text/plain", nullptr };
        gdk_clipboard_read_async(clip, uri_mimes, G_PRIORITY_DEFAULT, nullptr,
            [](GObject* src, GAsyncResult* res, gpointer ud) {
                auto* uctx = static_cast<UriCtx*>(ud);
                auto* app = uctx->app;
                bool key_press = uctx->from_key_press;
                delete uctx;

                const char* out_mime = nullptr;
                GInputStream* in = gdk_clipboard_read_finish(GDK_CLIPBOARD(src), res, &out_mime, nullptr);
                if (in) {
                    std::string content = read_entire_stream_as_string(in);
                    g_object_unref(in);
                    auto paths = parse_paths_or_uris(content);
                    bool attached = false;
                    for (const auto& p : paths) {
                        if (is_image_file(p)) {
                            std::string saved = save_image_file_to_storage(p);
                            if (!saved.empty()) {
                                insert_image_into_detail_view(app, saved);
                                attached = true;
                            }
                        }
                    }
                    if (!attached && app->detail_view && key_press) {
                        GtkTextBuffer* b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->detail_view));
                        gtk_text_buffer_insert_at_cursor(b, content.c_str(), -1);
                    }
                }
            }, ctx);
        return;
    }
}

static gboolean on_detail_drop(GtkDropTarget*, const GValue* value, double, double, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    if (!s) return FALSE;

    bool handled = false;

    if (G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST)) {
        GdkFileList* fl = static_cast<GdkFileList*>(g_value_get_boxed(value));
        if (fl) {
            GSList* files = gdk_file_list_get_files(fl);
            for (GSList* l = files; l != nullptr; l = l->next) {
                GFile* file = G_FILE(l->data);
                if (file) {
                    char* path = g_file_get_path(file);
                    if (path) {
                        std::string p(path);
                        g_free(path);
                        if (is_image_file(p)) {
                            std::string saved = save_image_file_to_storage(p);
                            if (!saved.empty()) {
                                insert_image_into_detail_view(s, saved);
                                handled = true;
                            }
                        }
                    }
                }
            }
            g_slist_free(files);
        }
    } else if (G_VALUE_HOLDS(value, G_TYPE_FILE)) {
        GFile* file = G_FILE(g_value_get_object(value));
        if (file) {
            char* path = g_file_get_path(file);
            if (path) {
                std::string p(path);
                g_free(path);
                if (is_image_file(p)) {
                    std::string saved = save_image_file_to_storage(p);
                    if (!saved.empty()) {
                        insert_image_into_detail_view(s, saved);
                        handled = true;
                    }
                }
            }
        }
    } else if (G_VALUE_HOLDS(value, GDK_TYPE_TEXTURE)) {
        GdkTexture* tex = GDK_TEXTURE(g_value_get_object(value));
        if (tex) {
            int w = gdk_texture_get_width(tex);
            int h = gdk_texture_get_height(tex);
            if (w > 0 && h > 0) {
                std::string saved = save_texture_to_storage(tex);
                if (!saved.empty()) {
                    insert_image_into_detail_view(s, saved);
                    handled = true;
                }
            }
        }
    } else if (G_VALUE_HOLDS(value, G_TYPE_STRING)) {
        const char* str = g_value_get_string(value);
        if (str) {
            auto paths = parse_paths_or_uris(str);
            for (const auto& p : paths) {
                if (is_image_file(p)) {
                    std::string saved = save_image_file_to_storage(p);
                    if (!saved.empty()) {
                        insert_image_into_detail_view(s, saved);
                        handled = true;
                    }
                }
            }
        }
    }

    return handled ? TRUE : FALSE;
}

static void setup_drop_target(GtkWidget* widget, AppState* s) {
    if (!widget) return;
    GType drop_types[] = { GDK_TYPE_FILE_LIST, G_TYPE_FILE, GDK_TYPE_TEXTURE, G_TYPE_STRING };
    GtkDropTarget* target = gtk_drop_target_new(G_TYPE_INVALID,
        static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE));
    gtk_drop_target_set_gtypes(target, drop_types, G_N_ELEMENTS(drop_types));
    g_signal_connect(target, "drop", G_CALLBACK(on_detail_drop), s);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(target));
}

static gboolean on_editor_key_pressed(GtkEventControllerKey*, guint keyval, guint, GdkModifierType, gpointer);

static gboolean on_detail_key_pressed(GtkEventControllerKey* ctrl, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    GdkModifierType mods = static_cast<GdkModifierType>(state & gtk_accelerator_get_default_mod_mask());
    bool is_paste = (mods == GDK_CONTROL_MASK && (keyval == GDK_KEY_v || keyval == GDK_KEY_V)) ||
                    (mods == GDK_SHIFT_MASK && keyval == GDK_KEY_Insert);
    if (is_paste) {
        GdkClipboard* clip = gtk_widget_get_clipboard(s->detail_view);
        if (clipboard_has_image(clip)) {
            read_clipboard_and_insert_image(s, clip, true);
            return TRUE;
        }
    }
    return on_editor_key_pressed(ctrl, keyval, keycode, state, user_data);
}

static gboolean on_title_key_pressed(GtkEventControllerKey* ctrl, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    GdkModifierType mods = static_cast<GdkModifierType>(state & gtk_accelerator_get_default_mod_mask());
    bool is_paste = (mods == GDK_CONTROL_MASK && (keyval == GDK_KEY_v || keyval == GDK_KEY_V)) ||
                    (mods == GDK_SHIFT_MASK && keyval == GDK_KEY_Insert);
    if (is_paste) {
        GdkClipboard* clip = gtk_widget_get_clipboard(s->title_entry);
        if (clipboard_has_image(clip)) {
            read_clipboard_and_insert_image(s, clip, true);
            return TRUE;
        }
    }
    return on_editor_key_pressed(ctrl, keyval, keycode, state, user_data);
}

static gboolean on_editor_key_pressed(GtkEventControllerKey*, guint keyval, guint, GdkModifierType, gpointer) {
    // Ignore pure modifier presses
    switch (keyval) {
        case GDK_KEY_Shift_L: case GDK_KEY_Shift_R:
        case GDK_KEY_Control_L: case GDK_KEY_Control_R:
        case GDK_KEY_Alt_L: case GDK_KEY_Alt_R:
        case GDK_KEY_Super_L: case GDK_KEY_Super_R:
        case GDK_KEY_Caps_Lock: case GDK_KEY_Num_Lock:
        case GDK_KEY_Meta_L: case GDK_KEY_Meta_R:
        case GDK_KEY_ISO_Level3_Shift:
            return FALSE;
        default:
            break;
    }
    tk::play_typing_sound();
    return FALSE; // Allow default event processing
}

static void on_title_entry_changed(GtkEditable* editable, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    const char* txt = gtk_editable_get_text(editable);
    if (txt && txt[0] != '\0' && s->form_err) {
        gtk_widget_set_visible(s->form_err, FALSE);
    }
    tk::play_typing_sound();
}

static void open_add_composer(AppState* s) {
    if (!s->add_expanded) {
        if (!s->has_draft_times) {
            time_t now = std::time(nullptr);
            now -= (now % 60);

            s->draft_times[0] = now;
            s->draft_times[1] = now + 3600;
            s->draft_times[2] = now + 3600;
            s->slot_touched[0] = false;
            s->slot_touched[1] = false;
            s->slot_touched[2] = false;
            s->active_slot = 0;
            s->editing_minute = false;
            s->has_draft_times = true;
        }

        gtk_widget_set_visible(s->form_err, FALSE);
        sync_picker_ui(s);

        // Restore previously typed title & details if composer was closed accidentally
        if (s->title_entry && !s->draft_title.empty()) {
            const char* cur_txt = gtk_editable_get_text(GTK_EDITABLE(s->title_entry));
            if (!cur_txt || cur_txt[0] == '\0') {
                gtk_editable_set_text(GTK_EDITABLE(s->title_entry), s->draft_title.c_str());
            }
        }
        if (s->detail_view && !s->draft_detail.empty()) {
            GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(s->detail_view));
            GtkTextIter start_iter, end_iter;
            gtk_text_buffer_get_bounds(buf, &start_iter, &end_iter);
            if (gtk_text_iter_equal(&start_iter, &end_iter)) {
                populate_detail_buffer(buf, s->draft_detail);
            }
        }

        s->add_expanded = true;
        gtk_widget_add_css_class(s->add_card, "expanded");
        gtk_widget_set_visible(s->add_revealer, TRUE);
        gtk_revealer_set_reveal_child(GTK_REVEALER(s->add_revealer), TRUE);
    }
    if (s->add_hint_lbl) {
        const char* cur_title = s->title_entry ? gtk_editable_get_text(GTK_EDITABLE(s->title_entry)) : "";
        if (!cur_title || cur_title[0] == '\0') {
            gtk_widget_set_visible(s->add_hint_lbl, FALSE);
        } else {
            gtk_widget_set_visible(s->add_hint_lbl, FALSE);
        }
    }
    gtk_widget_set_can_focus(s->title_entry, TRUE);
    gtk_widget_grab_focus(s->title_entry);
}

static void close_add_composer(AppState* s, bool clear_draft = false) {
    if (s->title_entry) {
        const char* txt = gtk_editable_get_text(GTK_EDITABLE(s->title_entry));
        s->draft_title = txt ? txt : "";
    }
    if (s->detail_view) {
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(s->detail_view));
        s->draft_detail = serialize_detail_buffer(buf);
    }

    if (clear_draft) {
        s->draft_title.clear();
        s->draft_detail.clear();
        s->has_draft_times = false;
        if (s->title_entry) {
            gtk_editable_set_text(GTK_EDITABLE(s->title_entry), "");
        }
        if (s->detail_view) {
            GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(s->detail_view));
            gtk_text_buffer_set_text(buf, "", -1);
        }
    }

    s->add_expanded = false;
    gtk_widget_set_visible(s->form_err, FALSE);
    gtk_widget_remove_css_class(s->add_card, "expanded");
    gtk_revealer_set_reveal_child(GTK_REVEALER(s->add_revealer), FALSE);
    if (s->add_hint_lbl) {
        const char* txt = s->title_entry ? gtk_editable_get_text(GTK_EDITABLE(s->title_entry)) : "";
        bool has_title = (txt && txt[0] != '\0');
        gtk_widget_set_visible(s->add_hint_lbl, !has_title);
    }
    gtk_widget_set_can_focus(s->title_entry, FALSE);
    if (s->window) {
        gtk_window_set_focus(GTK_WINDOW(s->window), nullptr);
    }
}

static void on_add_revealer_done(GObject* obj, GParamSpec*, gpointer) {
    GtkRevealer* rev = GTK_REVEALER(obj);
    if (!gtk_revealer_get_reveal_child(rev) && !gtk_revealer_get_child_revealed(rev)) {
        gtk_widget_set_visible(GTK_WIDGET(rev), FALSE);
    }
}

static void on_add_banner_pressed(GtkGestureClick*, int, double, double, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    if (!s->add_expanded) {
        open_add_composer(s);
    } else {
        close_add_composer(s);
    }
}

static void on_window_pressed(GtkGestureClick* gesture, int, double x, double y, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    tk::play_click_sound();

    if (!s->add_expanded) return;
    GtkWidget* win = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    GtkWidget* picked = gtk_widget_pick(win, x, y, GTK_PICK_DEFAULT);
    if (picked && (picked == s->add_card || gtk_widget_is_ancestor(picked, s->add_card))) {
        return;
    }
    const char* txt = gtk_editable_get_text(GTK_EDITABLE(s->title_entry));
    if (!txt || trim(txt).empty()) {
        close_add_composer(s, true);
    }
}

static gboolean on_window_key_pressed(GtkEventControllerKey*, guint keyval, guint,
                                      GdkModifierType, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    if (keyval == GDK_KEY_Escape && s->add_expanded) {
        close_add_composer(s);
        return TRUE;
    }
    return FALSE;
}

static void submit_add_task(AppState* s) {
    std::string title = trim(gtk_editable_get_text(GTK_EDITABLE(s->title_entry)));
    if (title.empty()) {
        gtk_label_set_text(GTK_LABEL(s->form_err), "Enter a task title");
        gtk_widget_set_visible(s->form_err, TRUE);
        gtk_widget_grab_focus(s->title_entry);
        return;
    }

    std::string detail;
    if (s->detail_view) {
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(s->detail_view));
        detail = trim(serialize_detail_buffer(buf));
    }

    time_t start_at = s->draft_times[0];
    time_t deadline_at = s->draft_times[1];
    time_t expires_at = s->draft_times[2];

    if (deadline_at < start_at) {
        gtk_label_set_text(GTK_LABEL(s->form_err), "Deadline must be after Start");
        gtk_widget_set_visible(s->form_err, TRUE);
        return;
    }
    if (expires_at < start_at) {
        gtk_label_set_text(GTK_LABEL(s->form_err), "Expiration must be after Start");
        gtk_widget_set_visible(s->form_err, TRUE);
        return;
    }

    s->store.add(title, start_at, deadline_at, expires_at, detail);
    close_add_composer(s, true); // Task created successfully, clear draft
    on_timer_tick(s);
    refresh_ui(s);
}

static void on_submit_clicked(GtkButton*, gpointer user_data) {
    submit_add_task(static_cast<AppState*>(user_data));
}

static void on_entry_activate(GtkEntry*, gpointer user_data) {
    submit_add_task(static_cast<AppState*>(user_data));
}

static void on_cancel_clicked(GtkButton*, gpointer user_data) {
    close_add_composer(static_cast<AppState*>(user_data));
}

static GtkWidget* build_add_composer(AppState* s) {
    s->add_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(s->add_card, "tk-add-card");
    gtk_widget_set_hexpand(s->add_card, TRUE);

    // --- Top row: [+] + inline title input with centered "Add New Task  Ctrl+N" ---
    GtkWidget* top_overlay = gtk_overlay_new();
    gtk_widget_set_hexpand(top_overlay, TRUE);

    GtkWidget* top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_valign(top_row, GTK_ALIGN_CENTER);

    GtkWidget* plus_icon = gtk_image_new_from_icon_name("list-add-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(plus_icon), 14);
    GtkWidget* plus_badge = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(plus_badge, "tk-add-plus-badge");
    gtk_widget_set_valign(plus_badge, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(plus_badge), plus_icon);
    gtk_box_append(GTK_BOX(top_row), plus_badge);

    s->title_entry = gtk_entry_new();
    gtk_widget_add_css_class(s->title_entry, "tk-add-inline-entry");
    gtk_widget_set_hexpand(s->title_entry, TRUE);
    gtk_widget_set_can_focus(s->title_entry, FALSE);
    g_signal_connect(s->title_entry, "changed", G_CALLBACK(on_title_entry_changed), s);
    g_signal_connect(s->title_entry, "activate", G_CALLBACK(on_entry_activate), s);
    GtkEventController* title_key_ctrl = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(title_key_ctrl, GTK_PHASE_CAPTURE);
    g_signal_connect(title_key_ctrl, "key-pressed", G_CALLBACK(on_title_key_pressed), s);
    gtk_widget_add_controller(s->title_entry, title_key_ctrl);
    GtkEditable* title_delegate = gtk_editable_get_delegate(GTK_EDITABLE(s->title_entry));
    if (title_delegate) {
        GtkEventController* del_key_ctrl = gtk_event_controller_key_new();
        gtk_event_controller_set_propagation_phase(del_key_ctrl, GTK_PHASE_CAPTURE);
        g_signal_connect(del_key_ctrl, "key-pressed", G_CALLBACK(on_title_key_pressed), s);
        gtk_widget_add_controller(GTK_WIDGET(title_delegate), del_key_ctrl);
    }
    gtk_box_append(GTK_BOX(top_row), s->title_entry);

    gtk_overlay_set_child(GTK_OVERLAY(top_overlay), top_row);

    s->add_hint_lbl = gtk_label_new("Add New Task  Ctrl+N");
    gtk_widget_add_css_class(s->add_hint_lbl, "tk-add-hint");
    gtk_widget_set_halign(s->add_hint_lbl, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(s->add_hint_lbl, GTK_ALIGN_CENTER);
    gtk_widget_set_can_target(s->add_hint_lbl, FALSE);
    gtk_overlay_add_overlay(GTK_OVERLAY(top_overlay), s->add_hint_lbl);

    GtkGesture* banner_click = gtk_gesture_click_new();
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(banner_click), GTK_PHASE_CAPTURE);
    g_signal_connect(banner_click, "pressed", G_CALLBACK(on_add_banner_pressed), s);
    gtk_widget_add_controller(top_overlay, GTK_EVENT_CONTROLLER(banner_click));

    gtk_box_append(GTK_BOX(s->add_card), top_overlay);

    // --- Expandable schedule panel merged inside the same card ---
    s->add_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(s->add_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(s->add_revealer), 150);
    gtk_revealer_set_reveal_child(GTK_REVEALER(s->add_revealer), FALSE);
    gtk_widget_set_visible(s->add_revealer, FALSE);
    g_signal_connect(s->add_revealer, "notify::child-revealed", G_CALLBACK(on_add_revealer_done), s);

    GtkWidget* details_wrap = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(details_wrap, "tk-add-details");

    GtkWidget* details_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_hexpand(details_box, TRUE);

    GtkWidget* seg_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(seg_box, "tk-seg");
    gtk_widget_set_hexpand(seg_box, TRUE);
    gtk_box_set_homogeneous(GTK_BOX(seg_box), TRUE);
    const char* seg_names[3] = {"Start", "Deadline", "Expires"};
    for (int i = 0; i < 3; ++i) {
        GtkWidget* btn = gtk_button_new();
        gtk_widget_add_css_class(btn, "tk-seg-btn");
        g_object_set_data(G_OBJECT(btn), "seg-idx",
                          reinterpret_cast<gpointer>(static_cast<intptr_t>(i)));
        g_signal_connect(btn, "clicked", G_CALLBACK(on_seg_clicked), s);

        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
        GtkWidget* nm = gtk_label_new(seg_names[i]);
        gtk_widget_add_css_class(nm, "tk-seg-name");
        GtkWidget* vl = gtk_label_new("00:00");
        gtk_widget_add_css_class(vl, "tk-seg-val");
        gtk_box_append(GTK_BOX(inner), nm);
        gtk_box_append(GTK_BOX(inner), vl);
        gtk_button_set_child(GTK_BUTTON(btn), inner);

        s->seg_btn[i] = btn;
        s->seg_val[i] = vl;
        gtk_box_append(GTK_BOX(seg_box), btn);
    }
    gtk_box_append(GTK_BOX(details_box), seg_box);

    // Main body: Left side is task details (notes/description), Right side is clock & calendar
    GtkWidget* body_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_widget_set_hexpand(body_row, TRUE);

    // --- Left column: Details (subtasks checklist) ---
    GtkWidget* left_desc_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_hexpand(left_desc_col, TRUE);

    GtkWidget* desc_lbl = gtk_label_new("Details");
    gtk_widget_add_css_class(desc_lbl, "tk-detail-field-title");
    gtk_label_set_xalign(GTK_LABEL(desc_lbl), 0.0f);
    gtk_box_append(GTK_BOX(left_desc_col), desc_lbl);

    GtkWidget* desc_scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(desc_scroller),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(desc_scroller, TRUE);
    gtk_widget_set_hexpand(desc_scroller, TRUE);
    gtk_widget_set_size_request(desc_scroller, 180, 190);

    s->detail_view = gtk_text_view_new();
    gtk_widget_add_css_class(s->detail_view, "tk-detail-entry");
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(s->detail_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(s->detail_view), FALSE);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(s->detail_view), 8);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(s->detail_view), 8);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(s->detail_view), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(s->detail_view), 10);
    GtkEventController* detail_key_ctrl = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(detail_key_ctrl, GTK_PHASE_CAPTURE);
    g_signal_connect(detail_key_ctrl, "key-pressed", G_CALLBACK(on_detail_key_pressed), s);
    gtk_widget_add_controller(s->detail_view, detail_key_ctrl);

    // Setup invisible drag and drop targets
    setup_drop_target(s->detail_view, s);
    setup_drop_target(desc_scroller, s);
    setup_drop_target(s->add_card, s);

    // Connect paste-done on text buffer to handle context-menu Paste
    GtkTextBuffer* detail_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(s->detail_view));
    g_signal_connect(detail_buf, "paste-done", G_CALLBACK(+[](GtkTextBuffer*, GdkClipboard* clip, gpointer ud) {
        auto* app = static_cast<AppState*>(ud);
        if (clipboard_has_image(clip)) {
            read_clipboard_and_insert_image(app, clip);
        }
    }), s);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(desc_scroller), s->detail_view);
    gtk_box_append(GTK_BOX(left_desc_col), desc_scroller);

    gtk_box_append(GTK_BOX(body_row), left_desc_col);

    // --- Right column: Clock & Calendar moved to the right ---
    GtkWidget* picker_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(picker_row, GTK_ALIGN_END);

    GtkWidget* clock_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(clock_box, "tk-clock-box");
    gtk_widget_set_valign(clock_box, GTK_ALIGN_FILL);

    GtkWidget* time_hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_halign(time_hdr, GTK_ALIGN_CENTER);

    s->hour_btn = gtk_button_new_with_label("00");
    gtk_widget_add_css_class(s->hour_btn, "tk-clock-num");
    g_signal_connect(s->hour_btn, "clicked", G_CALLBACK(on_hour_btn_clicked), s);
    GtkEventController* hscroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(hscroll, "scroll", G_CALLBACK(on_time_scroll), s);
    gtk_widget_add_controller(s->hour_btn, hscroll);

    GtkWidget* colon = gtk_label_new(":");
    gtk_widget_add_css_class(colon, "tk-clock-colon");

    s->min_btn = gtk_button_new_with_label("00");
    gtk_widget_add_css_class(s->min_btn, "tk-clock-num");
    g_signal_connect(s->min_btn, "clicked", G_CALLBACK(on_min_btn_clicked), s);
    GtkEventController* mscroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(mscroll, "scroll", G_CALLBACK(on_time_scroll), s);
    gtk_widget_add_controller(s->min_btn, mscroll);

    s->ampm_btn = gtk_button_new_with_label("PM");
    gtk_widget_add_css_class(s->ampm_btn, "tk-ampm");
    gtk_widget_set_valign(s->ampm_btn, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(s->ampm_btn, 4);
    g_signal_connect(s->ampm_btn, "clicked", G_CALLBACK(on_ampm_clicked), s);

    gtk_box_append(GTK_BOX(time_hdr), s->hour_btn);
    gtk_box_append(GTK_BOX(time_hdr), colon);
    gtk_box_append(GTK_BOX(time_hdr), s->min_btn);
    gtk_box_append(GTK_BOX(time_hdr), s->ampm_btn);
    gtk_box_append(GTK_BOX(clock_box), time_hdr);

    s->clock_area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(s->clock_area), 146);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(s->clock_area), 146);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(s->clock_area), draw_clock_face, s, nullptr);

    GtkGesture* drag = gtk_gesture_drag_new();
    g_signal_connect(drag, "drag-begin", G_CALLBACK(on_clock_drag_begin), s);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_clock_drag_update), s);
    g_signal_connect(drag, "drag-end", G_CALLBACK(on_clock_drag_end), s);
    gtk_widget_add_controller(s->clock_area, GTK_EVENT_CONTROLLER(drag));

    gtk_box_append(GTK_BOX(clock_box), s->clock_area);
    gtk_box_append(GTK_BOX(picker_row), clock_box);

    s->calendar = gtk_calendar_new();
    gtk_widget_add_css_class(s->calendar, "tk-cal");
    g_signal_connect(s->calendar, "day-selected", G_CALLBACK(on_calendar_day_selected), s);
    gtk_box_append(GTK_BOX(picker_row), s->calendar);

    gtk_box_append(GTK_BOX(body_row), picker_row);
    gtk_box_append(GTK_BOX(details_box), body_row);

    s->form_err = gtk_label_new("");
    gtk_widget_add_css_class(s->form_err, "tk-form-err");
    gtk_label_set_xalign(GTK_LABEL(s->form_err), 0.0f);
    gtk_widget_set_visible(s->form_err, FALSE);
    gtk_box_append(GTK_BOX(details_box), s->form_err);

    GtkWidget* actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(actions, GTK_ALIGN_END);

    GtkWidget* cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel_btn, "tk-btn-cancel");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_clicked), s);

    GtkWidget* add_btn = gtk_button_new_with_label("Add");
    gtk_widget_add_css_class(add_btn, "tk-btn-primary");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_submit_clicked), s);

    gtk_box_append(GTK_BOX(actions), cancel_btn);
    gtk_box_append(GTK_BOX(actions), add_btn);
    gtk_box_append(GTK_BOX(details_box), actions);

    gtk_box_append(GTK_BOX(details_wrap), details_box);
    gtk_revealer_set_child(GTK_REVEALER(s->add_revealer), details_wrap);
    gtk_box_append(GTK_BOX(s->add_card), s->add_revealer);

    return s->add_card;
}

static GtkWidget* make_date_picker_btn(GtkWidget*& popover_out, GtkWidget*& cal_out,
                                       GCallback on_selected, gpointer user_data) {
    GtkWidget* btn = gtk_menu_button_new();
    gtk_widget_add_css_class(btn, "tk-period-date");
    popover_out = gtk_popover_new();
    gtk_widget_add_css_class(popover_out, "tk-popover");
    cal_out = gtk_calendar_new();
    gtk_widget_add_css_class(cal_out, "tk-cal");
    g_signal_connect(cal_out, "day-selected", on_selected, user_data);
    gtk_popover_set_child(GTK_POPOVER(popover_out), cal_out);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(btn), popover_out);
    return btn;
}

static GtkWidget* build_stats_view(AppState* s) {
    GtkWidget* wrap = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_vexpand(wrap, TRUE);

    GtkWidget* filter_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    GtkWidget* seg = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_widget_add_css_class(seg, "tk-seg");
    gtk_box_set_homogeneous(GTK_BOX(seg), TRUE);
    const char* scope_names[4] = {"Range", "Day", "Month", "Year"};
    for (int i = 0; i < 4; ++i) {
        GtkWidget* b = gtk_button_new_with_label(scope_names[i]);
        gtk_widget_add_css_class(b, "tk-seg-btn");
        g_object_set_data(G_OBJECT(b), "scope-idx",
                          reinterpret_cast<gpointer>(static_cast<intptr_t>(i)));
        g_signal_connect(b, "clicked", G_CALLBACK(on_scope_clicked), s);
        s->scope_btn[i] = b;
        gtk_box_append(GTK_BOX(seg), b);
    }
    gtk_box_append(GTK_BOX(filter_box), seg);

    GtkWidget* nav_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(nav_row, GTK_ALIGN_CENTER);

    s->period_nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* prev_btn = gtk_button_new_from_icon_name("go-previous-symbolic");
    gtk_widget_add_css_class(prev_btn, "tk-period-arrow");
    g_signal_connect(prev_btn, "clicked", G_CALLBACK(on_period_prev), s);

    s->period_menu_btn = make_date_picker_btn(
        s->period_popover, s->stat_calendar, G_CALLBACK(on_stat_cal_selected), s);

    GtkWidget* next_btn = gtk_button_new_from_icon_name("go-next-symbolic");
    gtk_widget_add_css_class(next_btn, "tk-period-arrow");
    g_signal_connect(next_btn, "clicked", G_CALLBACK(on_period_next), s);

    gtk_box_append(GTK_BOX(s->period_nav_box), prev_btn);
    gtk_box_append(GTK_BOX(s->period_nav_box), s->period_menu_btn);
    gtk_box_append(GTK_BOX(s->period_nav_box), next_btn);

    s->range_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    s->from_menu_btn = make_date_picker_btn(
        s->from_popover, s->from_calendar, G_CALLBACK(on_from_cal_selected), s);
    GtkWidget* dash = gtk_label_new("–");
    gtk_widget_add_css_class(dash, "tk-range-sep");
    s->to_menu_btn = make_date_picker_btn(
        s->to_popover, s->to_calendar, G_CALLBACK(on_to_cal_selected), s);

    gtk_box_append(GTK_BOX(s->range_bar), s->from_menu_btn);
    gtk_box_append(GTK_BOX(s->range_bar), dash);
    gtk_box_append(GTK_BOX(s->range_bar), s->to_menu_btn);

    gtk_box_append(GTK_BOX(nav_row), s->period_nav_box);
    gtk_box_append(GTK_BOX(nav_row), s->range_bar);
    gtk_box_append(GTK_BOX(filter_box), nav_row);

    gtk_box_append(GTK_BOX(wrap), filter_box);

    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(card, "tk-stat-card");

    s->pie_area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(s->pie_area), 260);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(s->pie_area), 170);
    gtk_widget_set_halign(s->pie_area, GTK_ALIGN_CENTER);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(s->pie_area), draw_pie_chart, s, nullptr);
    gtk_box_append(GTK_BOX(card), s->pie_area);

    GtkWidget* legend_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    gtk_widget_set_halign(legend_row, GTK_ALIGN_CENTER);

    struct Leg { const char* color; const char* name; };
    const Leg legs[3] = {
        {"green",  "Finished"},
        {"orange", "Completed Late"},
        {"red",    "Unfinished"},
    };
    for (int i = 0; i < 3; ++i) {
        GtkWidget* item = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget* dot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(dot, "tk-dot");
        gtk_widget_add_css_class(dot, legs[i].color);
        gtk_widget_set_valign(dot, GTK_ALIGN_CENTER);

        GtkWidget* nm = gtk_label_new(legs[i].name);
        gtk_widget_add_css_class(nm, "tk-legend-name");

        gtk_box_append(GTK_BOX(item), dot);
        gtk_box_append(GTK_BOX(item), nm);
        gtk_box_append(GTK_BOX(legend_row), item);
    }
    gtk_box_append(GTK_BOX(card), legend_row);

    s->stat_total_lbl = gtk_label_new("You have killed 0 tasks.");
    gtk_widget_add_css_class(s->stat_total_lbl, "tk-stat-total");
    gtk_widget_set_halign(s->stat_total_lbl, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(card), s->stat_total_lbl);

    gtk_box_append(GTK_BOX(wrap), card);

    GtkWidget* vic_hdr = gtk_label_new("Victims");
    gtk_widget_add_css_class(vic_hdr, "tk-section-title");
    gtk_label_set_xalign(GTK_LABEL(vic_hdr), 0.0f);
    gtk_box_append(GTK_BOX(wrap), vic_hdr);

    GtkWidget* vscroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(vscroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(vscroll, TRUE);

    GtkWidget* vwrap = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    s->victims_list = gtk_list_box_new();
    gtk_widget_add_css_class(s->victims_list, "tk-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(s->victims_list), GTK_SELECTION_NONE);

    s->victims_empty = gtk_label_new("No data for this period");
    gtk_widget_add_css_class(s->victims_empty, "tk-empty");

    gtk_box_append(GTK_BOX(vwrap), s->victims_list);
    gtk_box_append(GTK_BOX(vwrap), s->victims_empty);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(vscroll), vwrap);
    gtk_box_append(GTK_BOX(wrap), vscroll);

    return wrap;
}

static void on_quit_action(GSimpleAction*, GVariant*, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    g_application_release(G_APPLICATION(s->app));
    g_application_quit(G_APPLICATION(s->app));
}

static void on_new_task_action(GSimpleAction*, GVariant*, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    if (!s->add_expanded) {
        open_add_composer(s);
    } else {
        close_add_composer(s);
    }
}

static void build_window(AppState* s) {
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css, tk::APP_CSS);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    time_t now = std::time(nullptr);
    tk::localtime_safe(&now, &s->stat_date);
    s->range_to = s->stat_date;
    time_t week_ago = now - 6 * 86400;
    tk::localtime_safe(&week_ago, &s->range_from);

    s->window = gtk_application_window_new(s->app);
    gtk_widget_add_css_class(s->window, "tk-window");
    gtk_window_set_title(GTK_WINDOW(s->window), "Task Killer");
    gtk_window_set_default_size(GTK_WINDOW(s->window), 860, 600);
    gtk_widget_set_size_request(s->window, 560, 480);
    gtk_window_set_hide_on_close(GTK_WINDOW(s->window), TRUE);

    GtkGesture* win_click = gtk_gesture_click_new();
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(win_click), GTK_PHASE_CAPTURE);
    g_signal_connect(win_click, "pressed", G_CALLBACK(on_window_pressed), s);
    gtk_widget_add_controller(s->window, GTK_EVENT_CONTROLLER(win_click));

    GtkEventController* win_key = gtk_event_controller_key_new();
    g_signal_connect(win_key, "key-pressed", G_CALLBACK(on_window_key_pressed), s);
    gtk_widget_add_controller(s->window, win_key);

    GtkWidget* header = gtk_header_bar_new();
    gtk_widget_add_css_class(header, "tk-header");
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), TRUE);

    // Toggle menu di kiri atas
    s->menu_btn = gtk_menu_button_new();
    gtk_widget_add_css_class(s->menu_btn, "tk-nav-menu-btn");
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(s->menu_btn), "open-menu-symbolic");

    s->menu_popover = gtk_popover_new();
    gtk_widget_add_css_class(s->menu_popover, "tk-popover");

    GtkWidget* menu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(menu_box, 6);
    gtk_widget_set_margin_end(menu_box, 6);
    gtk_widget_set_margin_top(menu_box, 6);
    gtk_widget_set_margin_bottom(menu_box, 6);

    s->menu_achiev_btn = gtk_button_new();
    gtk_widget_add_css_class(s->menu_achiev_btn, "tk-menu-item-btn");
    GtkWidget* achiev_btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* achiev_icon = gtk_image_new_from_icon_name("emblem-ok-symbolic");
    GtkWidget* achiev_lbl = gtk_label_new("Achievements");
    gtk_box_append(GTK_BOX(achiev_btn_box), achiev_icon);
    gtk_box_append(GTK_BOX(achiev_btn_box), achiev_lbl);
    gtk_button_set_child(GTK_BUTTON(s->menu_achiev_btn), achiev_btn_box);
    g_signal_connect(s->menu_achiev_btn, "clicked", G_CALLBACK(on_menu_achiev_clicked), s);

    s->menu_stats_btn = gtk_button_new();
    gtk_widget_add_css_class(s->menu_stats_btn, "tk-menu-item-btn");
    GtkWidget* stats_btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* stats_icon = gtk_image_new_from_icon_name("utilities-system-monitor-symbolic");
    GtkWidget* stats_lbl = gtk_label_new("Statistics");
    gtk_widget_set_hexpand(stats_lbl, TRUE);
    gtk_label_set_xalign(GTK_LABEL(stats_lbl), 0.0f);
    gtk_box_append(GTK_BOX(stats_btn_box), stats_lbl);
    gtk_box_append(GTK_BOX(stats_btn_box), stats_icon);
    gtk_button_set_child(GTK_BUTTON(s->menu_stats_btn), stats_btn_box);
    g_signal_connect(s->menu_stats_btn, "clicked", G_CALLBACK(on_menu_stats_clicked), s);

    gtk_box_append(GTK_BOX(menu_box), s->menu_achiev_btn);
    gtk_box_append(GTK_BOX(menu_box), s->menu_stats_btn);
    gtk_popover_set_child(GTK_POPOVER(s->menu_popover), menu_box);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(s->menu_btn), s->menu_popover);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), s->menu_btn);

    GtkWidget* app_title = gtk_label_new("Task Killer");
    gtk_widget_add_css_class(app_title, "tk-app-title");
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), app_title);

    gtk_window_set_titlebar(GTK_WINDOW(s->window), header);

    // Main 2-column container: Left = collapsible side panel (Achievements/Stats), Right = Active Tasks (spacious)
    GtkWidget* main_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(main_container, TRUE);
    gtk_widget_set_vexpand(main_container, TRUE);

    // --- Left side panel (Drawer for Achievements and Statistics) ---
    s->side_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(s->side_panel, "tk-side-panel");
    gtk_widget_set_size_request(s->side_panel, 310, -1);
    gtk_widget_set_vexpand(s->side_panel, TRUE);
    gtk_widget_set_visible(s->side_panel, FALSE); // Collapsed by default

    // Side panel header with title and close button
    GtkWidget* side_hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(side_hdr, "tk-side-hdr");

    s->side_title = gtk_label_new("Details");
    gtk_widget_add_css_class(s->side_title, "tk-side-title");
    gtk_label_set_xalign(GTK_LABEL(s->side_title), 0.0f);
    gtk_widget_set_hexpand(s->side_title, TRUE);

    s->side_close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(s->side_close_btn, "tk-side-close");
    gtk_widget_set_tooltip_text(s->side_close_btn, "Close panel");
    g_signal_connect(s->side_close_btn, "clicked", G_CALLBACK(on_side_close_clicked), s);

    gtk_box_append(GTK_BOX(side_hdr), s->side_title);
    gtk_box_append(GTK_BOX(side_hdr), s->side_close_btn);
    gtk_box_append(GTK_BOX(s->side_panel), side_hdr);

    // Side panel stack containing Achievements & Statistics views
    s->side_stack = gtk_stack_new();
    gtk_widget_set_vexpand(s->side_stack, TRUE);
    gtk_widget_set_hexpand(s->side_stack, TRUE);

    // 1. Achievements view in stack
    GtkWidget* achiev_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(achiev_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(achiev_scroll, TRUE);
    GtkWidget* achiev_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    s->achiev_list_box = gtk_list_box_new();
    gtk_widget_add_css_class(s->achiev_list_box, "tk-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(s->achiev_list_box), GTK_SELECTION_NONE);

    s->achiev_empty_label = gtk_label_new("No achievements yet");
    gtk_widget_add_css_class(s->achiev_empty_label, "tk-empty");

    gtk_box_append(GTK_BOX(achiev_box), s->achiev_list_box);
    gtk_box_append(GTK_BOX(achiev_box), s->achiev_empty_label);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(achiev_scroll), achiev_box);
    gtk_stack_add_named(GTK_STACK(s->side_stack), achiev_scroll, "achiev");

    // 2. Statistics view in stack
    GtkWidget* stats_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(stats_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(stats_scroll, TRUE);
    GtkWidget* stats_view = build_stats_view(s);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(stats_scroll), stats_view);
    gtk_stack_add_named(GTK_STACK(s->side_stack), stats_scroll, "stats");

    gtk_box_append(GTK_BOX(s->side_panel), s->side_stack);
    gtk_box_append(GTK_BOX(main_container), s->side_panel);

    // --- Right side: Tasks column (spacious, wide & high) ---
    GtkWidget* right_column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(right_column, "tk-tasks-panel");
    gtk_widget_set_hexpand(right_column, TRUE);
    gtk_widget_set_vexpand(right_column, TRUE);
    gtk_widget_set_margin_start(right_column, 16);
    gtk_widget_set_margin_end(right_column, 16);
    gtk_widget_set_margin_top(right_column, 8);
    gtk_widget_set_margin_bottom(right_column, 14);

    // Tasks title row with count
    GtkWidget* title_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_valign(title_box, GTK_ALIGN_CENTER);

    s->view_title = gtk_label_new("Tasks");
    gtk_widget_add_css_class(s->view_title, "tk-view-title");
    gtk_label_set_xalign(GTK_LABEL(s->view_title), 0.0f);

    s->view_count_lbl = gtk_label_new("(0)");
    gtk_widget_add_css_class(s->view_count_lbl, "tk-view-count");
    gtk_widget_set_valign(s->view_count_lbl, GTK_ALIGN_BASELINE_FILL);

    gtk_box_append(GTK_BOX(title_box), s->view_title);
    gtk_box_append(GTK_BOX(title_box), s->view_count_lbl);
    gtk_box_append(GTK_BOX(right_column), title_box);

    // --- Merged inline Add Task composer below title ---
    gtk_box_append(GTK_BOX(right_column), build_add_composer(s));

    // Active tasks scrolled list (takes maximum width and height)
    GtkWidget* tasks_scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tasks_scroller),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(tasks_scroller, TRUE);
    gtk_widget_set_hexpand(tasks_scroller, TRUE);

    s->list_box = gtk_list_box_new();
    gtk_widget_add_css_class(s->list_box, "tk-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(s->list_box), GTK_SELECTION_NONE);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tasks_scroller), s->list_box);
    gtk_box_append(GTK_BOX(right_column), tasks_scroller);

    gtk_box_append(GTK_BOX(main_container), right_column);
    gtk_window_set_child(GTK_WINDOW(s->window), main_container);

    g_application_hold(G_APPLICATION(s->app));
    g_timeout_add_seconds(5, on_timer_tick, s);
}

static void on_activate(GtkApplication*, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    if (!s->window) {
        build_window(s);
        on_timer_tick(s);
    }
    refresh_ui(s);
    gtk_window_present(GTK_WINDOW(s->window));
    if (!s->add_expanded) {
        gtk_window_set_focus(GTK_WINDOW(s->window), nullptr);
    }
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    if (!tk::setup_windows_environment(L"Task Killer")) {
        return 1;
    }
#else
    notify_init("Task Killer");
#endif

    AppState state;
    state.store.load();

    state.app = gtk_application_new("com.taskkiller.app", G_APPLICATION_DEFAULT_FLAGS);

    GSimpleAction* quit_act = g_simple_action_new("quit", nullptr);
    g_signal_connect(quit_act, "activate", G_CALLBACK(on_quit_action), &state);
    g_action_map_add_action(G_ACTION_MAP(state.app), G_ACTION(quit_act));
    const char* quit_accels[] = {"<Control>q", nullptr};
    gtk_application_set_accels_for_action(state.app, "app.quit", quit_accels);

    GSimpleAction* new_act = g_simple_action_new("new-task", nullptr);
    g_signal_connect(new_act, "activate", G_CALLBACK(on_new_task_action), &state);
    g_action_map_add_action(G_ACTION_MAP(state.app), G_ACTION(new_act));
    const char* new_accels[] = {"<Control>n", nullptr};
    gtk_application_set_accels_for_action(state.app, "app.new-task", new_accels);

    g_signal_connect(state.app, "activate", G_CALLBACK(on_activate), &state);
    int status = g_application_run(G_APPLICATION(state.app), argc, argv);

    g_object_unref(quit_act);
    g_object_unref(new_act);
    g_object_unref(state.app);
#ifndef _WIN32
    notify_uninit();
#endif
    return status;
}
