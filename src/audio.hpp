#pragma once

#include <glib.h>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace tk {

inline std::string locate_sound_file() {
    std::error_code ec;

    // 1. Check relative to current working directory
    const char* rel_paths[] = {
        "assets/beeb.wav",
        "assets/sounds/beeb.wav",
        "beeb.wav"
    };
    for (const char* p : rel_paths) {
        if (std::filesystem::exists(p, ec)) {
            auto abs = std::filesystem::canonical(p, ec);
            if (!ec) return abs.string();
            return p;
        }
    }

    // 2. Check relative to executable directory
    auto exe = std::filesystem::canonical("/proc/self/exe", ec);
    if (!ec && !exe.empty()) {
        auto dir = exe.parent_path();
        const std::filesystem::path exe_paths[] = {
            dir / "../share/task-killer/sounds/beeb.wav",
            dir / "assets/beeb.wav",
            dir / "../assets/beeb.wav",
            dir / "beeb.wav",
            dir / "../beeb.wav"
        };
        for (const auto& ep : exe_paths) {
            if (std::filesystem::exists(ep, ec)) {
                auto canon = std::filesystem::canonical(ep, ec);
                if (!ec) return canon.string();
                return ep.string();
            }
        }
    }

    // 3. Check system installed location
    static const char* kSystemPath = "/usr/share/task-killer/sounds/beeb.wav";
    if (std::filesystem::exists(kSystemPath, ec)) {
        return kSystemPath;
    }

    return "";
}

inline std::string get_player_command() {
    const char* players[] = {"canberra-gtk-play", "pw-play", "paplay", "aplay"};
    for (const char* cmd : players) {
        gchar* path = g_find_program_in_path(cmd);
        if (path) {
            std::string result = cmd;
            g_free(path);
            return result;
        }
    }
    return "";
}

inline bool play_alert_sound(bool force_ignore_debounce = false) {
    static std::string sound_file;
    if (sound_file.empty()) {
        sound_file = locate_sound_file();
        if (sound_file.empty()) return false;
    }

    static std::string player;
    if (player.empty()) {
        player = get_player_command();
        if (player.empty()) return false;
    }

    static bool has_played = false;
    static auto last_played = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (!force_ignore_debounce && has_played &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_played).count() < 500) {
        return false;
    }
    has_played = true;
    last_played = now;

    std::vector<const char*> argv;
    argv.push_back(player.c_str());
    if (player == "canberra-gtk-play") {
        argv.push_back("-d");
        argv.push_back("Task Killer");
        argv.push_back("-f");
    } else if (player == "aplay") {
        argv.push_back("-q");
    }
    argv.push_back(sound_file.c_str());
    argv.push_back(nullptr);

    GError* err = nullptr;
    gboolean ok = g_spawn_async(
        nullptr,
        const_cast<char**>(argv.data()),
        nullptr,
        static_cast<GSpawnFlags>(G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL),
        nullptr,
        nullptr,
        nullptr,
        &err
    );
    if (!ok && err) {
        g_error_free(err);
        return false;
    }
    return ok;
}

} // namespace tk
