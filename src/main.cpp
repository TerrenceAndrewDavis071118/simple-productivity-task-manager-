#include <gtk/gtk.h>
#include <libnotify/notify.h>

#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>

#include "store.hpp"
#include "style.hpp"

namespace {

constexpr double PI = 3.14159265358979323846;

enum class NavPage { Active = 0, Achieved = 1, Stats = 2 };

struct AppState {
    GtkApplication* app = nullptr;
    GtkWidget* window = nullptr;
    GtkWidget* list_scroller = nullptr;
    GtkWidget* list_box = nullptr;
    GtkWidget* view_title = nullptr;
    GtkWidget* empty_label = nullptr;
    GtkWidget* add_menu_btn = nullptr;
    GtkWidget* add_popover = nullptr;
    GtkWidget* title_entry = nullptr;
    GtkWidget* seg_btn[3]{};
    GtkWidget* seg_val[3]{};
    GtkWidget* calendar = nullptr;
    GtkWidget* clock_area = nullptr;
    GtkWidget* hour_btn = nullptr;
    GtkWidget* min_btn = nullptr;
    GtkWidget* ampm_btn = nullptr;
    GtkWidget* form_err = nullptr;
    GtkWidget* nav_btn[3]{};
    GtkWidget* nav_count[2]{};

    // Statistics widgets
    GtkWidget* stats_box = nullptr;
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

    NavPage current_view = NavPage::Active;
    tk::Store store;
};

struct NotifCtx {
    AppState* state;
    long task_id;
};

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

static void send_task_notification(AppState* s, const tk::Task& t) {
    std::string body = "Deadline " + tk::format_hm(t.deadline_at);
    const char* icon = std::filesystem::exists(
        "/usr/share/icons/hicolor/scalable/apps/com.taskkiller.app.svg")
        ? "com.taskkiller.app"
        : "appointment-soon-symbolic";

    NotifyNotification* n = notify_notification_new(t.title.c_str(), body.c_str(), icon);
    notify_notification_set_urgency(n, NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout(n, NOTIFY_EXPIRES_DEFAULT);

    auto* ctx = g_new(NotifCtx, 1);
    ctx->state = s;
    ctx->task_id = t.id;
    notify_notification_add_action(n, "done", "Done", on_notif_done, ctx, g_free);
    g_signal_connect(n, "closed", G_CALLBACK(on_notif_closed), nullptr);

    if (!notify_notification_show(n, nullptr)) {
        g_object_unref(n);
    }
}

static gboolean on_timer_tick(gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    bool changed = false;
    auto notify_ids = s->store.tick(std::time(nullptr), changed);
    for (long id : notify_ids) {
        if (const tk::Task* t = s->store.find(id)) {
            send_task_notification(s, *t);
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
    s->store.set_status(id, tk::Status::Achieved);
    refresh_ui(s);
}

static void on_delete_clicked(GtkButton* btn, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    long id = widget_task_id(GTK_WIDGET(btn));
    if (s->current_view == NavPage::Active) {
        s->store.set_status(id, tk::Status::Trash);
    } else {
        s->store.remove_permanently(id);
    }
    refresh_ui(s);
}

static GtkWidget* build_task_row(AppState* s, const tk::Task& t) {
    GtkWidget* row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(row_box, "tk-row");
    gtk_widget_set_valign(row_box, GTK_ALIGN_CENTER);

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
    gtk_box_append(GTK_BOX(row_box), check);

    GtkWidget* title = gtk_label_new(t.title.c_str());
    gtk_widget_add_css_class(title, "tk-task-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_box_append(GTK_BOX(row_box), title);

    std::string hm = tk::format_hm(t.deadline_at);
    GtkWidget* deadline_lbl = gtk_label_new(hm.c_str());
    gtk_widget_add_css_class(deadline_lbl, "tk-task-time");
    gtk_box_append(GTK_BOX(row_box), deadline_lbl);

    GtkWidget* del_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(del_btn, "tk-del-btn");
    gtk_widget_set_tooltip_text(del_btn,
        t.status == tk::Status::Active ? "Remove task" : "Delete permanently");
    set_widget_task_id(del_btn, t.id);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_clicked), s);
    gtk_box_append(GTK_BOX(row_box), del_btn);

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
    double r = std::min(cx - 94.0, cy - 18.0);
    if (r < 48.0) r = std::min(cx, cy) - 8.0;

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
    for (int i = 0; i < 4; ++i) {
        if (i == sc) gtk_widget_add_css_class(s->scope_btn[i], "active");
        else gtk_widget_remove_css_class(s->scope_btn[i], "active");
    }

    bool is_range = (s->stat_scope == tk::StatScope::Range);
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
    const char* titles[] = {"Tasks", "Achievements", "Statistics"};
    const char* empties[] = {"No tasks", "No achievements yet"};
    int idx = static_cast<int>(s->current_view);

    gtk_label_set_text(GTK_LABEL(s->view_title), titles[idx]);

    for (int i = 0; i < 3; ++i) {
        if (i == idx) gtk_widget_add_css_class(s->nav_btn[i], "active");
        else gtk_widget_remove_css_class(s->nav_btn[i], "active");
    }
    for (int i = 0; i < 2; ++i) {
        int c = s->store.count(static_cast<tk::Status>(i));
        gtk_label_set_text(GTK_LABEL(s->nav_count[i]), std::to_string(c).c_str());
    }

    bool is_stats = (s->current_view == NavPage::Stats);
    gtk_widget_set_visible(s->list_scroller, !is_stats);
    gtk_widget_set_visible(s->stats_box, is_stats);

    if (is_stats) {
        refresh_stats_view(s);
        return;
    }

    tk::Status target = (s->current_view == NavPage::Active)
        ? tk::Status::Active
        : tk::Status::Achieved;
    gtk_list_box_remove_all(GTK_LIST_BOX(s->list_box));

    int visible_rows = 0;
    for (const auto& t : s->store.tasks) {
        if (t.status == target) {
            gtk_list_box_append(GTK_LIST_BOX(s->list_box), build_task_row(s, t));
            ++visible_rows;
        }
    }

    gtk_label_set_text(GTK_LABEL(s->empty_label), empties[idx]);
    gtk_widget_set_visible(s->empty_label, visible_rows == 0);
}

static void on_nav_clicked(GtkButton* btn, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    int idx = static_cast<int>(
        reinterpret_cast<intptr_t>(g_object_get_data(G_OBJECT(btn), "nav-idx")));
    s->current_view = static_cast<NavPage>(idx);
    refresh_ui(s);
}

static void on_scope_clicked(GtkButton* btn, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    int idx = static_cast<int>(
        reinterpret_cast<intptr_t>(g_object_get_data(G_OBJECT(btn), "scope-idx")));
    s->stat_scope = static_cast<tk::StatScope>(idx);
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
    localtime_r(&cur, &tm_cur);
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
    localtime_r(&s->draft_times[s->active_slot], &tm_cur);
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
    localtime_r(&s->draft_times[s->active_slot], &tm_cur);

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
    localtime_r(&s->draft_times[s->active_slot], &tm_cur);
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
    localtime_r(&s->draft_times[s->active_slot], &tm_cur);
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

static void on_popover_visible(GObject* obj, GParamSpec*, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    if (!gtk_widget_get_visible(GTK_WIDGET(obj))) return;

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

    gtk_editable_set_text(GTK_EDITABLE(s->title_entry), "");
    gtk_widget_set_visible(s->form_err, FALSE);
    sync_picker_ui(s);
    gtk_widget_grab_focus(s->title_entry);
}

static void submit_add_task(AppState* s) {
    std::string title = trim(gtk_editable_get_text(GTK_EDITABLE(s->title_entry)));
    if (title.empty()) {
        gtk_label_set_text(GTK_LABEL(s->form_err), "Enter a task title");
        gtk_widget_set_visible(s->form_err, TRUE);
        gtk_widget_grab_focus(s->title_entry);
        return;
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

    s->store.add(title, start_at, deadline_at, expires_at);
    s->current_view = NavPage::Active;
    gtk_popover_popdown(GTK_POPOVER(s->add_popover));
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
    auto* s = static_cast<AppState*>(user_data);
    gtk_popover_popdown(GTK_POPOVER(s->add_popover));
}

static GtkWidget* build_add_popover(AppState* s) {
    GtkWidget* pop = gtk_popover_new();
    gtk_widget_add_css_class(pop, "tk-popover");
    gtk_popover_set_autohide(GTK_POPOVER(pop), TRUE);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    s->title_entry = gtk_entry_new();
    gtk_widget_add_css_class(s->title_entry, "tk-entry");
    gtk_entry_set_placeholder_text(GTK_ENTRY(s->title_entry), "Task title");
    g_signal_connect(s->title_entry, "activate", G_CALLBACK(on_entry_activate), s);
    gtk_box_append(GTK_BOX(box), s->title_entry);

    GtkWidget* seg_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(seg_box, "tk-seg");
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
    gtk_box_append(GTK_BOX(box), seg_box);

    GtkWidget* picker_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    s->calendar = gtk_calendar_new();
    gtk_widget_add_css_class(s->calendar, "tk-cal");
    g_signal_connect(s->calendar, "day-selected", G_CALLBACK(on_calendar_day_selected), s);
    gtk_box_append(GTK_BOX(picker_row), s->calendar);

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
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(s->clock_area), 152);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(s->clock_area), 152);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(s->clock_area), draw_clock_face, s, nullptr);

    GtkGesture* drag = gtk_gesture_drag_new();
    g_signal_connect(drag, "drag-begin", G_CALLBACK(on_clock_drag_begin), s);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_clock_drag_update), s);
    g_signal_connect(drag, "drag-end", G_CALLBACK(on_clock_drag_end), s);
    gtk_widget_add_controller(s->clock_area, GTK_EVENT_CONTROLLER(drag));

    gtk_box_append(GTK_BOX(clock_box), s->clock_area);
    gtk_box_append(GTK_BOX(picker_row), clock_box);
    gtk_box_append(GTK_BOX(box), picker_row);

    s->form_err = gtk_label_new("");
    gtk_widget_add_css_class(s->form_err, "tk-form-err");
    gtk_label_set_xalign(GTK_LABEL(s->form_err), 0.0f);
    gtk_widget_set_visible(s->form_err, FALSE);
    gtk_box_append(GTK_BOX(box), s->form_err);

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
    gtk_box_append(GTK_BOX(box), actions);

    gtk_popover_set_child(GTK_POPOVER(pop), box);
    g_signal_connect(pop, "notify::visible", G_CALLBACK(on_popover_visible), s);
    return pop;
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

    GtkWidget* filter_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    GtkWidget* seg = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_widget_add_css_class(seg, "tk-seg");
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

    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);

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

    gtk_box_append(GTK_BOX(filter_row), seg);
    gtk_box_append(GTK_BOX(filter_row), spacer);
    gtk_box_append(GTK_BOX(filter_row), s->period_nav_box);
    gtk_box_append(GTK_BOX(filter_row), s->range_bar);
    gtk_box_append(GTK_BOX(wrap), filter_row);

    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(card, "tk-stat-card");

    s->pie_area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(s->pie_area), 380);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(s->pie_area), 220);
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

static GtkWidget* build_sidebar(AppState* s) {
    GtkWidget* sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(sidebar, "tk-sidebar");
    gtk_widget_set_size_request(sidebar, 195, -1);
    gtk_widget_set_vexpand(sidebar, TRUE);

    struct Item { const char* icon; const char* label; bool has_count; };
    const Item items[3] = {
        {"view-list-symbolic",      "Tasks",        true},
        {"emblem-ok-symbolic",      "Achievements", true},
        {"view-pie-chart-symbolic", "Statistics",   false},
    };

    for (int i = 0; i < 3; ++i) {
        GtkWidget* btn = gtk_button_new();
        gtk_widget_add_css_class(btn, "tk-nav");
        g_object_set_data(G_OBJECT(btn), "nav-idx",
                          reinterpret_cast<gpointer>(static_cast<intptr_t>(i)));
        g_signal_connect(btn, "clicked", G_CALLBACK(on_nav_clicked), s);

        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
        GtkWidget* icon = gtk_image_new_from_icon_name(items[i].icon);
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 15);
        GtkWidget* lbl = gtk_label_new(items[i].label);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_widget_set_hexpand(lbl, TRUE);

        gtk_box_append(GTK_BOX(row), icon);
        gtk_box_append(GTK_BOX(row), lbl);

        if (items[i].has_count) {
            GtkWidget* cnt = gtk_label_new("0");
            gtk_widget_add_css_class(cnt, "tk-nav-count");
            gtk_box_append(GTK_BOX(row), cnt);
            s->nav_count[i] = cnt;
        }

        gtk_button_set_child(GTK_BUTTON(btn), row);
        s->nav_btn[i] = btn;
        gtk_box_append(GTK_BOX(sidebar), btn);
    }
    return sidebar;
}

static void on_quit_action(GSimpleAction*, GVariant*, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    g_application_release(G_APPLICATION(s->app));
    g_application_quit(G_APPLICATION(s->app));
}

static void on_new_task_action(GSimpleAction*, GVariant*, gpointer user_data) {
    auto* s = static_cast<AppState*>(user_data);
    gtk_menu_button_popup(GTK_MENU_BUTTON(s->add_menu_btn));
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
    localtime_r(&now, &s->stat_date);
    s->range_to = s->stat_date;
    time_t week_ago = now - 6 * 86400;
    localtime_r(&week_ago, &s->range_from);

    s->window = gtk_application_window_new(s->app);
    gtk_widget_add_css_class(s->window, "tk-window");
    gtk_window_set_title(GTK_WINDOW(s->window), "Task Killer");
    gtk_window_set_default_size(GTK_WINDOW(s->window), 820, 600);
    gtk_widget_set_size_request(s->window, 650, 500);
    gtk_window_set_hide_on_close(GTK_WINDOW(s->window), TRUE);

    GtkWidget* header = gtk_header_bar_new();
    gtk_widget_add_css_class(header, "tk-header");
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), TRUE);

    GtkWidget* app_title = gtk_label_new("Task Killer");
    gtk_widget_add_css_class(app_title, "tk-app-title");
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), app_title);

    s->add_menu_btn = gtk_menu_button_new();
    gtk_widget_add_css_class(s->add_menu_btn, "tk-add-btn");
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(s->add_menu_btn), "list-add-symbolic");
    gtk_widget_set_tooltip_text(s->add_menu_btn, "Add task (Ctrl+N)");
    s->add_popover = build_add_popover(s);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(s->add_menu_btn), s->add_popover);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), s->add_menu_btn);

    gtk_window_set_titlebar(GTK_WINDOW(s->window), header);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(root), build_sidebar(s));

    GtkWidget* right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_hexpand(right, TRUE);
    gtk_widget_set_vexpand(right, TRUE);
    gtk_widget_set_margin_start(right, 22);
    gtk_widget_set_margin_end(right, 22);
    gtk_widget_set_margin_top(right, 8);
    gtk_widget_set_margin_bottom(right, 16);

    s->view_title = gtk_label_new("Tasks");
    gtk_widget_add_css_class(s->view_title, "tk-view-title");
    gtk_label_set_xalign(GTK_LABEL(s->view_title), 0.0f);
    gtk_box_append(GTK_BOX(right), s->view_title);

    s->list_scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(s->list_scroller),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(s->list_scroller, TRUE);

    GtkWidget* list_wrap = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    s->list_box = gtk_list_box_new();
    gtk_widget_add_css_class(s->list_box, "tk-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(s->list_box), GTK_SELECTION_NONE);

    s->empty_label = gtk_label_new("No tasks");
    gtk_widget_add_css_class(s->empty_label, "tk-empty");

    gtk_box_append(GTK_BOX(list_wrap), s->list_box);
    gtk_box_append(GTK_BOX(list_wrap), s->empty_label);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s->list_scroller), list_wrap);
    gtk_box_append(GTK_BOX(right), s->list_scroller);

    s->stats_box = build_stats_view(s);
    gtk_widget_set_visible(s->stats_box, FALSE);
    gtk_box_append(GTK_BOX(right), s->stats_box);

    gtk_box_append(GTK_BOX(root), right);
    gtk_window_set_child(GTK_WINDOW(s->window), root);

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
}

} // namespace

int main(int argc, char** argv) {
    notify_init("Task Killer");

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
    notify_uninit();
    return status;
}
