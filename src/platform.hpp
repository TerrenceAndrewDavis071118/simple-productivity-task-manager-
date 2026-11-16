#pragma once

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace tk {

inline void localtime_safe(const time_t* t, std::tm* out) {
    if (!t || !out) return;
#ifdef _WIN32
    time_t clamped = (*t < 0) ? 0 : *t;
    if (localtime_s(out, &clamped) != 0) {
        std::memset(out, 0, sizeof(*out));
    }
#else
    if (!localtime_r(t, out)) {
        std::memset(out, 0, sizeof(*out));
    }
#endif
}

inline std::filesystem::path get_executable_path() {
    std::error_code ec;
#ifdef _WIN32
    std::vector<wchar_t> buf(MAX_PATH);
    while (true) {
        DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (len == 0) {
            return "";
        }
        if (len < buf.size()) {
            auto p = std::filesystem::path(buf.data());
            return p.lexically_normal();
        }
        buf.resize(buf.size() * 2);
    }
#else
    auto exe = std::filesystem::canonical("/proc/self/exe", ec);
    if (!ec && !exe.empty()) return exe;
    return "";
#endif
}

inline std::string normalize_path_string(const std::filesystem::path& p) {
    std::error_code ec;
#ifdef _WIN32
    auto abs = std::filesystem::absolute(p, ec);
    if (!ec) return abs.lexically_normal().string();
    return p.lexically_normal().string();
#else
    auto canon = std::filesystem::canonical(p, ec);
    if (!ec) return canon.string();
    return p.string();
#endif
}

inline std::filesystem::path get_default_data_dir() {
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg) {
        return std::filesystem::path(xdg) / "task-killer";
    }
#ifdef _WIN32
    if (const char* appdata = std::getenv("APPDATA")) {
        if (*appdata) return std::filesystem::path(appdata) / "task-killer";
    }
    if (const char* localappdata = std::getenv("LOCALAPPDATA")) {
        if (*localappdata) return std::filesystem::path(localappdata) / "task-killer";
    }
    if (const char* userprofile = std::getenv("USERPROFILE")) {
        if (*userprofile) return std::filesystem::path(userprofile) / "AppData" / "Roaming" / "task-killer";
    }
    return std::filesystem::path(".") / "task-killer";
#else
    const char* home = std::getenv("HOME");
    std::filesystem::path base = (home && *home) ? std::filesystem::path(home) : ".";
    return base / ".local" / "share" / "task-killer";
#endif
}

} // namespace tk
