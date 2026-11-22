#include <gtk/gtk.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "installer_ui.hpp"

namespace {

enum class Step {
    Welcome = 0,
    License = 1,
    Directory = 2,
    Installing = 3,
    Finish = 4
};

struct InstallerState {
    GtkApplication* app = nullptr;
    GtkWidget* window = nullptr;
    GtkWidget* stack = nullptr;

    // Navigation buttons
    GtkWidget* btn_back = nullptr;
    GtkWidget* btn_next = nullptr;
    GtkWidget* btn_cancel = nullptr;

    // Step 1: License Agreement
    GtkWidget* chk_accept = nullptr;

    // Step 2: Directory selection
    GtkWidget* entry_dir = nullptr;
    GtkWidget* chk_desktop_icon = nullptr;
    GtkWidget* chk_start_menu = nullptr;

    // Step 3: Installing Progress
    GtkWidget* progress_bar = nullptr;
    GtkWidget* status_lbl = nullptr;
    GtkWidget* detail_lbl = nullptr;

    // Step 4: Finish
    GtkWidget* chk_launch_now = nullptr;
    GtkWidget* finish_title = nullptr;
    GtkWidget* finish_body = nullptr;

    Step current_step = Step::Welcome;
    double progress_val = 0.0;
    guint install_timer_id = 0;
    std::string installed_binary_path;
    bool install_success = true;
};

static void update_navigation(InstallerState* s);

static void cleanup_timer(InstallerState* s) {
    if (s->install_timer_id != 0) {
        g_source_remove(s->install_timer_id);
        s->install_timer_id = 0;
    }
}

static void set_step(InstallerState* s, Step step) {
    s->current_step = step;
    const char* name = "welcome";
    if (step == Step::License) name = "license";
    else if (step == Step::Directory) name = "directory";
    else if (step == Step::Installing) name = "installing";
    else if (step == Step::Finish) {
        name = "finish";
        if (s->finish_title && s->finish_body) {
            if (s->install_success) {
                gtk_label_set_text(GTK_LABEL(s->finish_title), "Completing the Task Killer Setup Wizard");
                gtk_label_set_text(GTK_LABEL(s->finish_body),
                    "Setup has finished installing Task Killer on your computer.\n"
                    "The application may be launched by selecting the installed icons.\n\n"
                    "Click Finish to exit Setup.");
                gtk_widget_set_visible(s->chk_launch_now, TRUE);
            } else {
                gtk_label_set_text(GTK_LABEL(s->finish_title), "Setup Incomplete");
                gtk_label_set_text(GTK_LABEL(s->finish_body),
                    "The wizard was unable to copy the Task Killer executable into the destination directory.\n"
                    "Please verify target directory permissions or compile the application first using 'make all'.\n\n"
                    "Click Finish to close Setup.");
                gtk_widget_set_visible(s->chk_launch_now, FALSE);
            }
        }
    }

    gtk_stack_set_visible_child_name(GTK_STACK(s->stack), name);
    update_navigation(s);
}

static void update_navigation(InstallerState* s) {
    switch (s->current_step) {
        case Step::Welcome:
            gtk_widget_set_visible(s->btn_back, TRUE);
            gtk_widget_set_sensitive(s->btn_back, FALSE);
            gtk_button_set_label(GTK_BUTTON(s->btn_next), "Next >");
            gtk_widget_set_sensitive(s->btn_next, TRUE);
            gtk_widget_set_visible(s->btn_cancel, TRUE);
            break;

        case Step::License:
            gtk_widget_set_visible(s->btn_back, TRUE);
            gtk_widget_set_sensitive(s->btn_back, TRUE);
            gtk_button_set_label(GTK_BUTTON(s->btn_next), "Next >");
            gtk_widget_set_sensitive(s->btn_next, gtk_check_button_get_active(GTK_CHECK_BUTTON(s->chk_accept)));
            gtk_widget_set_visible(s->btn_cancel, TRUE);
            break;

        case Step::Directory:
            gtk_widget_set_visible(s->btn_back, TRUE);
            gtk_widget_set_sensitive(s->btn_back, TRUE);
            gtk_button_set_label(GTK_BUTTON(s->btn_next), "Install");
            gtk_widget_set_sensitive(s->btn_next, TRUE);
            gtk_widget_set_visible(s->btn_cancel, TRUE);
            break;

        case Step::Installing:
            gtk_widget_set_sensitive(s->btn_back, FALSE);
            gtk_widget_set_sensitive(s->btn_next, FALSE);
            gtk_widget_set_sensitive(s->btn_cancel, FALSE);
            break;

        case Step::Finish:
            gtk_widget_set_visible(s->btn_back, FALSE);
            gtk_button_set_label(GTK_BUTTON(s->btn_next), "Finish");
            gtk_widget_set_sensitive(s->btn_next, TRUE);
            gtk_widget_set_visible(s->btn_cancel, FALSE);
            break;
    }
}

// Locate source binary dynamically across candidate locations
static std::filesystem::path find_source_binary() {
    std::error_code ec;
#ifdef _WIN32
    const char* candidates[] = {
        "build/task-killer.exe",
        "task-killer.exe",
        "../task-killer.exe",
        "dist/task-killer.exe"
    };
#else
    const char* candidates[] = {
        "build/task-killer",
        "task-killer",
        "../task-killer",
        "dist/task-killer"
    };
#endif
    for (const char* c : candidates) {
        if (std::filesystem::exists(c, ec)) {
            return std::filesystem::canonical(c, ec);
        }
    }
    return "";
}

// Locate sound assets to copy alongside installation
static std::filesystem::path find_source_sound() {
    std::error_code ec;
    const char* candidates[] = {
        "assets/beeb.wav",
        "beeb.wav",
        "../assets/beeb.wav"
    };
    for (const char* c : candidates) {
        if (std::filesystem::exists(c, ec)) {
            return std::filesystem::canonical(c, ec);
        }
    }
    return "";
}

static gboolean on_install_progress(gpointer user_data) {
    auto* s = static_cast<InstallerState*>(user_data);
    s->progress_val += 0.05;

    if (s->progress_val >= 1.0) {
        s->install_timer_id = 0;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(s->progress_bar), 1.0);
        gtk_label_set_text(GTK_LABEL(s->status_lbl), "Installation completed successfully.");
        gtk_label_set_text(GTK_LABEL(s->detail_lbl), "Registered PowerStack Corp. components and desktop links.");

        // Target Directory resolution
        std::string target_dir_str = gtk_editable_get_text(GTK_EDITABLE(s->entry_dir));
        if (target_dir_str.empty()) {
            const char* home = g_get_home_dir();
            target_dir_str = std::string(home) + "/.local/bin";
        }
        std::filesystem::path target_dir(target_dir_str);
        std::error_code ec;
        std::filesystem::create_directories(target_dir, ec);

#ifdef _WIN32
        std::string exe_name = "task-killer.exe";
#else
        std::string exe_name = "task-killer";
#endif
        std::filesystem::path dest_bin = target_dir / exe_name;
        s->installed_binary_path = dest_bin.string();

        // 1. Copy application executable
        std::filesystem::path src_bin = find_source_binary();
        if (!src_bin.empty() && std::filesystem::exists(src_bin, ec)) {
            std::filesystem::copy_file(src_bin, dest_bin, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                s->install_success = false;
            } else {
#ifndef _WIN32
                std::filesystem::permissions(dest_bin,
                    std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
                    std::filesystem::perms::others_read | std::filesystem::perms::others_exec, ec);
#endif
                s->install_success = true;
            }
        } else {
            s->install_success = false;
        }

        // 2. Copy audio asset if present
        std::filesystem::path src_snd = find_source_sound();
        if (!src_snd.empty()) {
            std::filesystem::path assets_dir = target_dir / "assets";
            std::filesystem::create_directories(assets_dir, ec);
            std::filesystem::copy_file(src_snd, assets_dir / "beeb.wav", std::filesystem::copy_options::overwrite_existing, ec);
        }

#ifndef _WIN32
        // 3. Desktop Shortcut (.desktop)
        if (gtk_check_button_get_active(GTK_CHECK_BUTTON(s->chk_desktop_icon))) {
            const char* desktop_env = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
            std::filesystem::path desktop_dir = desktop_env ? std::filesystem::path(desktop_env) : (std::filesystem::path(g_get_home_dir()) / "Desktop");
            if (std::filesystem::exists(desktop_dir, ec)) {
                std::ofstream ofs(desktop_dir / "TaskKiller.desktop");
                if (ofs.is_open()) {
                    ofs << "[Desktop Entry]\n"
                        << "Version=1.0\n"
                        << "Type=Application\n"
                        << "Name=Task Killer\n"
                        << "GenericName=Productivity & Deadline Manager\n"
                        << "Comment=Executive Task Manager by PowerStack Corp.\n"
                        << "Exec=\"" << dest_bin.string() << "\"\n"
                        << "Icon=utilities-terminal\n"
                        << "Terminal=false\n"
                        << "Categories=Utility;Office;\n";
                    ofs.close();
                    std::filesystem::permissions(desktop_dir / "TaskKiller.desktop",
                        std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::others_read, ec);
                }
            }
        }

        // 4. System Application Launcher Menu registration
        if (gtk_check_button_get_active(GTK_CHECK_BUTTON(s->chk_start_menu))) {
            const char* data_home = g_get_user_data_dir();
            std::filesystem::path apps_dir = std::filesystem::path(data_home) / "applications";
            std::filesystem::create_directories(apps_dir, ec);
            std::ofstream ofs(apps_dir / "task-killer.desktop");
            if (ofs.is_open()) {
                ofs << "[Desktop Entry]\n"
                    << "Version=1.0\n"
                    << "Type=Application\n"
                    << "Name=Task Killer\n"
                    << "GenericName=Productivity & Deadline Manager\n"
                    << "Comment=Executive Task Manager by PowerStack Corp.\n"
                    << "Exec=\"" << dest_bin.string() << "\"\n"
                    << "Icon=utilities-terminal\n"
                    << "Terminal=false\n"
                    << "Categories=Utility;Office;\n";
                ofs.close();
            }
        }
#endif

        // Switch to finish step
        set_step(s, Step::Finish);
        return G_SOURCE_REMOVE;
    }

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(s->progress_bar), s->progress_val);
    if (s->progress_val < 0.25) {
        gtk_label_set_text(GTK_LABEL(s->status_lbl), "Extracting Core Files...");
        gtk_label_set_text(GTK_LABEL(s->detail_lbl), "Extracting libcanberra sound engine & store assets...");
    } else if (s->progress_val < 0.60) {
        gtk_label_set_text(GTK_LABEL(s->status_lbl), "Deploying PowerStack Components...");
        gtk_label_set_text(GTK_LABEL(s->detail_lbl), "Configuring SQLite database schema & timing subsystems...");
    } else if (s->progress_val < 0.85) {
        gtk_label_set_text(GTK_LABEL(s->status_lbl), "Registering Desktop Environment...");
        gtk_label_set_text(GTK_LABEL(s->detail_lbl), "Installing icons, mime types, and system notifications...");
    } else {
        gtk_label_set_text(GTK_LABEL(s->status_lbl), "Finishing Installation...");
        gtk_label_set_text(GTK_LABEL(s->detail_lbl), "Finalizing PowerStack Corp. registry settings...");
    }

    return G_SOURCE_CONTINUE;
}

static void on_next_clicked(GtkButton*, gpointer user_data) {
    auto* s = static_cast<InstallerState*>(user_data);
    switch (s->current_step) {
        case Step::Welcome:
            set_step(s, Step::License);
            break;
        case Step::License:
            set_step(s, Step::Directory);
            break;
        case Step::Directory:
            set_step(s, Step::Installing);
            s->progress_val = 0.0;
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(s->progress_bar), 0.0);
            cleanup_timer(s);
            s->install_timer_id = g_timeout_add(100, on_install_progress, s);
            break;
        case Step::Finish:
            if (gtk_check_button_get_active(GTK_CHECK_BUTTON(s->chk_launch_now))) {
                if (!s->installed_binary_path.empty() && std::filesystem::exists(s->installed_binary_path)) {
                    char* argv_launch[] = { const_cast<char*>(s->installed_binary_path.c_str()), nullptr };
                    GError* err = nullptr;
                    g_spawn_async(nullptr, argv_launch, nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, &err);
                    if (err) {
                        g_error_free(err);
                    }
                }
            }
            cleanup_timer(s);
            g_application_quit(G_APPLICATION(s->app));
            break;
        default:
            break;
    }
}

static void on_back_clicked(GtkButton*, gpointer user_data) {
    auto* s = static_cast<InstallerState*>(user_data);
    if (s->current_step == Step::License) {
        set_step(s, Step::Welcome);
    } else if (s->current_step == Step::Directory) {
        set_step(s, Step::License);
    }
}

static void on_cancel_clicked(GtkButton*, gpointer user_data) {
    auto* s = static_cast<InstallerState*>(user_data);
    cleanup_timer(s);
    g_application_quit(G_APPLICATION(s->app));
}

static gboolean on_close_request(GtkWindow*, gpointer user_data) {
    auto* s = static_cast<InstallerState*>(user_data);
    cleanup_timer(s);
    return FALSE; // Allow window destruction
}

static void on_license_toggled(GtkCheckButton*, gpointer user_data) {
    auto* s = static_cast<InstallerState*>(user_data);
    update_navigation(s);
}

// Builds the classic Windows 7 step header banner
static GtkWidget* create_step_header(const char* title, const char* subtitle) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(box, "win7-header-banner");

    GtkWidget* lbl_t = gtk_label_new(title);
    gtk_widget_add_css_class(lbl_t, "win7-header-title");
    gtk_label_set_xalign(GTK_LABEL(lbl_t), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_t);

    GtkWidget* lbl_s = gtk_label_new(subtitle);
    gtk_widget_add_css_class(lbl_s, "win7-header-subtitle");
    gtk_label_set_xalign(GTK_LABEL(lbl_s), 0.0f);
    gtk_box_append(GTK_BOX(box), lbl_s);

    return box;
}

static GtkWidget* build_welcome_page() {
    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    // Left Classic Sidebar with PowerStack Corp. Branding
    GtkWidget* sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(sidebar, "win7-sidebar");
    gtk_widget_set_size_request(sidebar, 170, -1);

    GtkWidget* corp_badge = gtk_label_new("POWERSTACK CORP.");
    gtk_widget_add_css_class(corp_badge, "win7-sidebar-corp");
    gtk_label_set_xalign(GTK_LABEL(corp_badge), 0.0f);
    gtk_box_append(GTK_BOX(sidebar), corp_badge);

    GtkWidget* title = gtk_label_new("Task Killer\nSetup");
    gtk_widget_add_css_class(title, "win7-sidebar-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(sidebar), title);

    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(sidebar), spacer);

    GtkWidget* ver_lbl = gtk_label_new("Version 1.0.0\nRelease Build 2026");
    gtk_widget_add_css_class(ver_lbl, "win7-sidebar-corp");
    gtk_label_set_xalign(GTK_LABEL(ver_lbl), 0.0f);
    gtk_box_append(GTK_BOX(sidebar), ver_lbl);

    gtk_box_append(GTK_BOX(page), sidebar);

    // Right Content Box
    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(content, "win7-content");
    gtk_widget_set_hexpand(content, TRUE);
    gtk_widget_set_vexpand(content, TRUE);

    GtkWidget* h1 = gtk_label_new("Welcome to the Task Killer Setup Wizard");
    gtk_widget_add_css_class(h1, "win7-bold");
    gtk_label_set_xalign(GTK_LABEL(h1), 0.0f);
    gtk_box_append(GTK_BOX(content), h1);

    GtkWidget* p1 = gtk_label_new(
        "This wizard will guide you through the installation of Task Killer, an executive "
        "productivity and deadline management suite developed by PowerStack Corp.\n\n"
        "It is recommended that you close all other applications before continuing.\n\n"
        "Click Next to continue, or Cancel to exit Setup.");
    gtk_label_set_wrap(GTK_LABEL(p1), TRUE);
    gtk_label_set_xalign(GTK_LABEL(p1), 0.0f);
    gtk_box_append(GTK_BOX(content), p1);

    gtk_box_append(GTK_BOX(page), content);
    return page;
}

static GtkWidget* build_license_page(InstallerState* s) {
    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget* hdr = create_step_header(
        "License Agreement",
        "Please review the license terms before installing Task Killer by PowerStack Corp.");
    gtk_box_append(GTK_BOX(page), hdr);

    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(content, "win7-content");
    gtk_widget_set_vexpand(content, TRUE);

    GtkWidget* desc = gtk_label_new("Press Page Down to see the rest of the agreement.");
    gtk_widget_add_css_class(desc, "win7-dim");
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0f);
    gtk_box_append(GTK_BOX(content), desc);

    // EULA Scrollable Text
    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_set_hexpand(scroll, TRUE);

    GtkWidget* text_view = gtk_text_view_new();
    gtk_widget_add_css_class(text_view, "win7-eula");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);

    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buf, tk_installer::EULA_TEXT, -1);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), text_view);
    gtk_box_append(GTK_BOX(content), scroll);

    s->chk_accept = gtk_check_button_new_with_label(
        "I accept the agreement and terms specified by PowerStack Corp.");
    gtk_widget_add_css_class(s->chk_accept, "win7-check");
    g_signal_connect(s->chk_accept, "toggled", G_CALLBACK(on_license_toggled), s);
    gtk_box_append(GTK_BOX(content), s->chk_accept);

    gtk_box_append(GTK_BOX(page), content);
    return page;
}

static void on_browse_dir_clicked(GtkButton*, gpointer user_data) {
    auto* s = static_cast<InstallerState*>(user_data);
    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Select Destination Folder");
    gtk_file_dialog_select_folder(dialog, GTK_WINDOW(s->window), nullptr,
        +[](GObject* source, GAsyncResult* res, gpointer user_data) {
            auto* s = static_cast<InstallerState*>(user_data);
            GError* err = nullptr;
            GFile* file = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source), res, &err);
            if (file) {
                char* path = g_file_get_path(file);
                if (path) {
                    gtk_editable_set_text(GTK_EDITABLE(s->entry_dir), path);
                    g_free(path);
                }
                g_object_unref(file);
            }
            if (err) {
                g_clear_error(&err);
            }
        }, s);
}

static GtkWidget* build_directory_page(InstallerState* s) {
    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget* hdr = create_step_header(
        "Select Destination Location",
        "Where should Task Killer be installed on your system?");
    gtk_box_append(GTK_BOX(page), hdr);

    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(content, "win7-content");
    gtk_widget_set_vexpand(content, TRUE);

    GtkWidget* desc = gtk_label_new("Setup will install Task Killer into the following folder.");
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0f);
    gtk_box_append(GTK_BOX(content), desc);

    // Destination Entry with Browse Button
#ifdef _WIN32
    const char* localapp = g_getenv("LOCALAPPDATA");
    std::string default_dest = (localapp && *localapp) ? (std::string(localapp) + "\\Programs\\TaskKiller") : "C:\\Program Files\\TaskKiller";
#else
    const char* home = g_get_home_dir();
    std::string default_dest = std::string(home) + "/.local/bin";
#endif
    GtkWidget* dir_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    s->entry_dir = gtk_entry_new();
    gtk_widget_add_css_class(s->entry_dir, "win7-entry");
    gtk_editable_set_text(GTK_EDITABLE(s->entry_dir), default_dest.c_str());
    gtk_widget_set_hexpand(s->entry_dir, TRUE);
    gtk_box_append(GTK_BOX(dir_hbox), s->entry_dir);

    GtkWidget* btn_browse = gtk_button_new_with_label("Browse...");
    gtk_widget_add_css_class(btn_browse, "win7-btn");
    g_signal_connect(btn_browse, "clicked", G_CALLBACK(on_browse_dir_clicked), s);
    gtk_box_append(GTK_BOX(dir_hbox), btn_browse);

    gtk_box_append(GTK_BOX(content), dir_hbox);

    GtkWidget* req_lbl = gtk_label_new("At least 5.0 MB of free disk space is required.");
    gtk_widget_add_css_class(req_lbl, "win7-dim");
    gtk_label_set_xalign(GTK_LABEL(req_lbl), 0.0f);
    gtk_box_append(GTK_BOX(content), req_lbl);

    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(content), sep);

    GtkWidget* opt_hdr = gtk_label_new("Select Additional Tasks:");
    gtk_widget_add_css_class(opt_hdr, "win7-bold");
    gtk_label_set_xalign(GTK_LABEL(opt_hdr), 0.0f);
    gtk_box_append(GTK_BOX(content), opt_hdr);

    s->chk_desktop_icon = gtk_check_button_new_with_label("Create a desktop icon");
    gtk_widget_add_css_class(s->chk_desktop_icon, "win7-check");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(s->chk_desktop_icon), TRUE);
    gtk_box_append(GTK_BOX(content), s->chk_desktop_icon);

    s->chk_start_menu = gtk_check_button_new_with_label("Register with system application launcher");
    gtk_widget_add_css_class(s->chk_start_menu, "win7-check");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(s->chk_start_menu), TRUE);
    gtk_box_append(GTK_BOX(content), s->chk_start_menu);

    gtk_box_append(GTK_BOX(page), content);
    return page;
}

static GtkWidget* build_installing_page(InstallerState* s) {
    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget* hdr = create_step_header(
        "Installing",
        "Please wait while Setup installs Task Killer on your computer.");
    gtk_box_append(GTK_BOX(page), hdr);

    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(content, "win7-content");
    gtk_widget_set_vexpand(content, TRUE);

    s->status_lbl = gtk_label_new("Extracting files...");
    gtk_widget_add_css_class(s->status_lbl, "win7-bold");
    gtk_label_set_xalign(GTK_LABEL(s->status_lbl), 0.0f);
    gtk_box_append(GTK_BOX(content), s->status_lbl);

    s->progress_bar = gtk_progress_bar_new();
    gtk_widget_add_css_class(s->progress_bar, "win7-progress");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(s->progress_bar), 0.0);
    gtk_box_append(GTK_BOX(content), s->progress_bar);

    s->detail_lbl = gtk_label_new("PowerStack Corp. Setup is processing packages...");
    gtk_widget_add_css_class(s->detail_lbl, "win7-dim");
    gtk_label_set_xalign(GTK_LABEL(s->detail_lbl), 0.0f);
    gtk_box_append(GTK_BOX(content), s->detail_lbl);

    gtk_box_append(GTK_BOX(page), content);
    return page;
}

static GtkWidget* build_finish_page(InstallerState* s) {
    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    // Left Classic Sidebar with Finish Theme
    GtkWidget* sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(sidebar, "win7-sidebar");
    gtk_widget_set_size_request(sidebar, 170, -1);

    GtkWidget* corp_badge = gtk_label_new("POWERSTACK CORP.");
    gtk_widget_add_css_class(corp_badge, "win7-sidebar-corp");
    gtk_label_set_xalign(GTK_LABEL(corp_badge), 0.0f);
    gtk_box_append(GTK_BOX(sidebar), corp_badge);

    GtkWidget* title = gtk_label_new("Completed\nSetup");
    gtk_widget_add_css_class(title, "win7-sidebar-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(sidebar), title);

    gtk_box_append(GTK_BOX(page), sidebar);

    // Right Content Box
    GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(content, "win7-content");
    gtk_widget_set_hexpand(content, TRUE);
    gtk_widget_set_vexpand(content, TRUE);

    GtkWidget* h1 = gtk_label_new("Completing the Task Killer Setup Wizard");
    gtk_widget_add_css_class(h1, "win7-bold");
    gtk_label_set_xalign(GTK_LABEL(h1), 0.0f);
    gtk_box_append(GTK_BOX(content), h1);
    s->finish_title = h1;

    GtkWidget* p1 = gtk_label_new(
        "Setup has finished installing Task Killer on your computer.\n"
        "The application may be launched by selecting the installed icons.\n\n"
        "Click Finish to exit Setup.");
    gtk_label_set_wrap(GTK_LABEL(p1), TRUE);
    gtk_label_set_xalign(GTK_LABEL(p1), 0.0f);
    gtk_box_append(GTK_BOX(content), p1);
    s->finish_body = p1;

    s->chk_launch_now = gtk_check_button_new_with_label("Launch Task Killer now");
    gtk_widget_add_css_class(s->chk_launch_now, "win7-check");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(s->chk_launch_now), TRUE);
    gtk_box_append(GTK_BOX(content), s->chk_launch_now);

    gtk_box_append(GTK_BOX(page), content);
    return page;
}

static void on_activate(GtkApplication* app, gpointer user_data) {
    auto* s = static_cast<InstallerState*>(user_data);

    // Apply Windows 7 / Aero Installer CSS theme
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, tk_installer::WIN7_INSTALLER_CSS);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    s->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(s->window), "Setup - Task Killer (PowerStack Corp.)");
    gtk_window_set_default_size(GTK_WINDOW(s->window), 540, 390);
    gtk_window_set_resizable(GTK_WINDOW(s->window), FALSE);
    gtk_widget_add_css_class(s->window, "win7-installer");

    g_signal_connect(s->window, "close-request", G_CALLBACK(on_close_request), s);

    GtkWidget* main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    // Stack containing wizard pages
    s->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(s->stack), GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_widget_set_vexpand(s->stack, TRUE);

    gtk_stack_add_named(GTK_STACK(s->stack), build_welcome_page(), "welcome");
    gtk_stack_add_named(GTK_STACK(s->stack), build_license_page(s), "license");
    gtk_stack_add_named(GTK_STACK(s->stack), build_directory_page(s), "directory");
    gtk_stack_add_named(GTK_STACK(s->stack), build_installing_page(s), "installing");
    gtk_stack_add_named(GTK_STACK(s->stack), build_finish_page(s), "finish");

    gtk_box_append(GTK_BOX(main_vbox), s->stack);

    // Bottom Navigation Bar
    GtkWidget* bottom_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(bottom_bar, "win7-bottom-bar");

    GtkWidget* corp_foot = gtk_label_new("PowerStack Corp. (R)");
    gtk_widget_add_css_class(corp_foot, "win7-dim");
    gtk_box_append(GTK_BOX(bottom_bar), corp_foot);

    GtkWidget* h_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(h_spacer, TRUE);
    gtk_box_append(GTK_BOX(bottom_bar), h_spacer);

    s->btn_back = gtk_button_new_with_label("< Back");
    gtk_widget_add_css_class(s->btn_back, "win7-btn");
    g_signal_connect(s->btn_back, "clicked", G_CALLBACK(on_back_clicked), s);
    gtk_box_append(GTK_BOX(bottom_bar), s->btn_back);

    s->btn_next = gtk_button_new_with_label("Next >");
    gtk_widget_add_css_class(s->btn_next, "win7-btn");
    gtk_widget_add_css_class(s->btn_next, "primary");
    g_signal_connect(s->btn_next, "clicked", G_CALLBACK(on_next_clicked), s);
    gtk_box_append(GTK_BOX(bottom_bar), s->btn_next);

    s->btn_cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(s->btn_cancel, "win7-btn");
    g_signal_connect(s->btn_cancel, "clicked", G_CALLBACK(on_cancel_clicked), s);
    gtk_box_append(GTK_BOX(bottom_bar), s->btn_cancel);

    gtk_box_append(GTK_BOX(main_vbox), bottom_bar);

    gtk_window_set_child(GTK_WINDOW(s->window), main_vbox);
    set_step(s, Step::Welcome);

    gtk_window_present(GTK_WINDOW(s->window));
}

} // namespace

int main(int argc, char* argv[]) {
    InstallerState state;
    GtkApplication* app = gtk_application_new("com.powerstack.taskkiller.installer", G_APPLICATION_DEFAULT_FLAGS);
    state.app = app;
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), &state);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
