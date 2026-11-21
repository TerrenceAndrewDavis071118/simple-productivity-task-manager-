#pragma once

#include <glib.h>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
#ifndef _WIN32
#include <dlfcn.h>
#endif

#include "platform.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

namespace tk {

enum class SoundEvent {
    Alert,       // Alert / timer notification
    Complete,    // Task finished on time ("complete")
    Click,       // Tap / general UI notification ("message")
    Typing       // Keystroke typing feedback ("audio-volume-change")
};

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
            return normalize_path_string(p);
        }
    }

    // 2. Check relative to executable directory
    auto exe = get_executable_path();
    if (!exe.empty()) {
        auto dir = exe.parent_path();
        const std::filesystem::path exe_paths[] = {
            dir / "assets" / "beeb.wav",
            dir / "assets" / "sounds" / "beeb.wav",
            dir / "share" / "task-killer" / "sounds" / "beeb.wav",
            dir / ".." / "share" / "task-killer" / "sounds" / "beeb.wav",
            dir / ".." / "assets" / "beeb.wav",
            dir / "beeb.wav",
            dir / ".." / "beeb.wav"
        };
        for (const auto& ep : exe_paths) {
            if (std::filesystem::exists(ep, ec)) {
                return normalize_path_string(ep);
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

#ifndef _WIN32
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
#endif

inline bool play_sound(SoundEvent ev, bool force_ignore_debounce = false) {
#ifndef _WIN32
    static std::string player;
    if (player.empty()) {
        player = get_player_command();
        if (player.empty()) return false;
    }
#endif

    static auto last_played = std::chrono::steady_clock::now();
    static bool has_played = false;
    auto now = std::chrono::steady_clock::now();
    // Allow responsive UI clicks (debounce 60ms), but debounce alert/completion by 400ms
    int debounce_ms = (ev == SoundEvent::Click) ? 60 : 400;
    if (!force_ignore_debounce && has_played &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_played).count() < debounce_ms) {
        return false;
    }
    has_played = true;
    last_played = now;

#ifdef _WIN32
    if (waveOutGetNumDevs() == 0) {
        return true;
    }
    if (ev == SoundEvent::Alert) {
        static std::string sound_file = locate_sound_file();
        if (!sound_file.empty()) {
            if (PlaySoundA(sound_file.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) {
                return true;
            }
        }
    }
    const char* alias = "SystemAsterisk";
    if (ev == SoundEvent::Complete) {
        alias = "SystemNotification";
    } else if (ev == SoundEvent::Click) {
        alias = "CCSelect"; // Windows click / select sound
    }
    if (PlaySoundA(alias, NULL, SND_ALIAS | SND_ASYNC | SND_NODEFAULT)) {
        return true;
    }
    return PlaySoundA("SystemAsterisk", NULL, SND_ALIAS | SND_ASYNC | SND_NODEFAULT) != 0;
#else
    std::vector<const char*> argv;
    argv.push_back(player.c_str());

    const char* event_id = "message";
    if (ev == SoundEvent::Complete) {
        event_id = "complete";
    } else if (ev == SoundEvent::Alert) {
        event_id = "message-new-instant";
    }

    if (player == "canberra-gtk-play") {
        argv.push_back("-i");
        argv.push_back(event_id);
        argv.push_back("-d");
        argv.push_back("Task Killer");
    } else {
        // Fallback for pw-play / paplay / aplay with custom sound file if canberra is not available
        static std::string sound_file = locate_sound_file();
        if (sound_file.empty()) return false;
        if (player == "aplay") {
            argv.push_back("-q");
        }
        argv.push_back(sound_file.c_str());
    }
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
#endif
}

#ifndef _WIN32
// Fast in-process canberra sound playback via dynamic linking to avoid fork overhead on every keystroke
struct CanberraFastPlayer {
    typedef struct ca_context ca_context;
    typedef int (*ca_context_create_fn)(ca_context **c);
    typedef int (*ca_context_open_fn)(ca_context *c);
    typedef int (*ca_context_destroy_fn)(ca_context *c);
    typedef int (*ca_context_play_fn)(ca_context *c, uint32_t id, ...);

    void* handle = nullptr;
    ca_context* ctx = nullptr;
    ca_context_play_fn play_fn = nullptr;
    bool attempted = false;

    CanberraFastPlayer() = default;

    ~CanberraFastPlayer() {
        if (ctx && handle) {
            auto destroy_fn = reinterpret_cast<ca_context_destroy_fn>(dlsym(handle, "ca_context_destroy"));
            if (destroy_fn) destroy_fn(ctx);
        }
        if (handle) {
            dlclose(handle);
        }
    }

    bool init() {
        if (attempted) return (ctx != nullptr && play_fn != nullptr);
        attempted = true;
        handle = dlopen("libcanberra.so.0", RTLD_LAZY);
        if (!handle) {
            handle = dlopen("libcanberra.so", RTLD_LAZY);
        }
        if (!handle) return false;

        auto create_fn = reinterpret_cast<ca_context_create_fn>(dlsym(handle, "ca_context_create"));
        auto open_fn = reinterpret_cast<ca_context_open_fn>(dlsym(handle, "ca_context_open"));
        play_fn = reinterpret_cast<ca_context_play_fn>(dlsym(handle, "ca_context_play"));

        if (!create_fn || !open_fn || !play_fn) {
            dlclose(handle);
            handle = nullptr;
            play_fn = nullptr;
            return false;
        }

        if (create_fn(&ctx) != 0 || !ctx) {
            ctx = nullptr;
            dlclose(handle);
            handle = nullptr;
            play_fn = nullptr;
            return false;
        }

        open_fn(ctx);
        return true;
    }

    bool play_event(const char* event_id) {
        if (!init()) return false;
        // ca_context_play(ctx, id, "event.id", event_id, NULL)
        return (play_fn(ctx, 0, "event.id", event_id, nullptr) == 0);
    }
};

inline CanberraFastPlayer& get_canberra_player() {
    static CanberraFastPlayer s_player;
    return s_player;
}
#endif

inline bool play_typing_sound() {
    static auto last_typed = std::chrono::steady_clock::now();
    static bool has_typed = false;
    auto now = std::chrono::steady_clock::now();

    // 20ms minimal debounce to keep keyboard sound crisp without stutter
    if (has_typed &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_typed).count() < 20) {
        return false;
    }
    has_typed = true;
    last_typed = now;

#ifdef _WIN32
    if (waveOutGetNumDevs() == 0) return true;
    return PlaySoundA("CCSelect", NULL, SND_ALIAS | SND_ASYNC | SND_NODEFAULT) != 0;
#else
    // Try ultra-fast zero-fork libcanberra in-process call
    if (get_canberra_player().play_event("audio-volume-change")) {
        return true;
    }

    // Fallback to canberra-gtk-play or generic player via g_spawn_async
    static const char* cmd[] = {"canberra-gtk-play", "-i", "audio-volume-change", "-d", "Task Killer", nullptr};
    GError* err = nullptr;
    gboolean ok = g_spawn_async(
        nullptr,
        const_cast<char**>(cmd),
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
#endif
}

inline bool play_alert_sound(bool force_ignore_debounce = false) {
    return play_sound(SoundEvent::Alert, force_ignore_debounce);
}

inline bool play_complete_sound() {
    return play_sound(SoundEvent::Complete, true);
}

inline bool play_click_sound() {
    return play_sound(SoundEvent::Click, false);
}

} // namespace tk
