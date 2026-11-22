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
#include <glib.h>
#include <cstdio>
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

#ifdef _WIN32
inline LONG WINAPI unhandled_crash_filter(EXCEPTION_POINTERS* ep) {
    wchar_t msg[512];
    swprintf(msg, 512,
        L"Task Killer encountered a critical error and needs to close.\n"
        L"Exception Code: 0x%08lX\n"
        L"Fault Address: 0x%p\n\n"
        L"Please ensure that all files from task-killer_1.0.0_windows_x64.zip are fully extracted.",
        ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0,
        ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr);
    MessageBoxW(nullptr, msg, L"Task Killer - Fatal Error", MB_ICONERROR | MB_OK);
    return EXCEPTION_CONTINUE_SEARCH;
}

inline bool setup_windows_environment(const wchar_t* app_name = L"Task Killer") {
    SetUnhandledExceptionFilter(unhandled_crash_filter);

    auto exe = get_executable_path();
    if (exe.empty()) return true;
    auto dir = exe.parent_path();

    // 1. Prepend application directory to PATH so child processes & helper binaries find DLLs
    std::wstring cur_path;
    DWORD len = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    if (len > 0) {
        cur_path.resize(len);
        GetEnvironmentVariableW(L"PATH", cur_path.data(), len);
        cur_path.resize(len - 1);
    }
    std::wstring new_path = dir.wstring() + L";" + cur_path;
    SetEnvironmentVariableW(L"PATH", new_path.c_str());

    // 2. Configure GTK / GLib schema directory
    auto schema_dir = dir / "share" / "glib-2.0" / "schemas";
    auto schema_file = schema_dir / "gschemas.compiled";
    std::error_code ec;
    if (std::filesystem::exists(schema_file, ec) && !ec) {
        SetEnvironmentVariableW(L"GSETTINGS_SCHEMA_DIR", schema_dir.wstring().c_str());
        std::string u8_schema = schema_dir.u8string();
        g_setenv("GSETTINGS_SCHEMA_DIR", u8_schema.c_str(), TRUE);
    } else {
        std::wstring title = std::wstring(app_name) + L" - Incomplete Release Extraction";
        MessageBoxW(nullptr,
            L"Task Killer could not find required data files (share/glib-2.0/schemas).\n\n"
            L"If you opened the application directly from inside a ZIP file, please click 'Extract All' (Ekstrak Semua) to extract all files before launching.",
            title.c_str(), MB_ICONWARNING | MB_OK);
        return false;
    }

    // 3. Configure GDK Pixbuf loaders
    auto pixbuf_cache = dir / "lib" / "gdk-pixbuf-2.0" / "2.10.0" / "loaders.cache";
    if (std::filesystem::exists(pixbuf_cache, ec) && !ec) {
        SetEnvironmentVariableW(L"GDK_PIXBUF_MODULE_FILE", pixbuf_cache.wstring().c_str());
        std::string u8_pixbuf = pixbuf_cache.u8string();
        g_setenv("GDK_PIXBUF_MODULE_FILE", u8_pixbuf.c_str(), TRUE);
    }

    // 4. Set GTK and XDG data prefixes
    auto share_dir = dir / "share";
    if (std::filesystem::exists(share_dir, ec) && !ec) {
        SetEnvironmentVariableW(L"XDG_DATA_DIRS", share_dir.wstring().c_str());
        std::string u8_share = share_dir.u8string();
        g_setenv("XDG_DATA_DIRS", u8_share.c_str(), TRUE);
    }
    SetEnvironmentVariableW(L"GTK_EXE_PREFIX", dir.wstring().c_str());
    std::string u8_dir = dir.u8string();
    g_setenv("GTK_EXE_PREFIX", u8_dir.c_str(), TRUE);
    SetEnvironmentVariableW(L"GTK_DATA_PREFIX", dir.wstring().c_str());
    g_setenv("GTK_DATA_PREFIX", u8_dir.c_str(), TRUE);

    return true;
}
#endif

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
