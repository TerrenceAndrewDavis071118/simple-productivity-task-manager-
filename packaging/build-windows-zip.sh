#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

APP_NAME="task-killer"
VERSION="1.0.0"
DIST_DIR="dist"
STAGE_DIR="build/windows-pkg/${APP_NAME}"
ZIP_FILE="${DIST_DIR}/${APP_NAME}_${VERSION}_windows_x64.zip"

echo "=== Packaging Windows Release for ${APP_NAME} ${VERSION} ==="

if [ ! -f "build/task-killer.exe" ]; then
    echo "Error: build/task-killer.exe not found! Please run 'make all' first."
    exit 1
fi

rm -rf "build/windows-pkg"
mkdir -p "${STAGE_DIR}"
mkdir -p "${DIST_DIR}"

# 1. Copy application binary
cp build/task-killer.exe "${STAGE_DIR}/"

# 2. Copy assets
if [ -d "assets" ]; then
    cp -r assets "${STAGE_DIR}/"
fi
if [ -f "beeb.wav" ]; then
    cp beeb.wav "${STAGE_DIR}/"
fi

MINGW_PREFIX_DIR="${MINGW_PREFIX:-/ucrt64}"
MINGW_BIN="${MINGW_PREFIX_DIR}/bin"

# 3. Copy GLib process spawn helpers (required for non-blocking notifications via g_spawn_async)
for helper in gspawn-win64-helper.exe gspawn-win64-helper-console.exe gspawn-win32-helper.exe gspawn-win32-helper-console.exe; do
    if [ -f "${MINGW_BIN}/${helper}" ]; then
        cp "${MINGW_BIN}/${helper}" "${STAGE_DIR}/"
    fi
done

# 4. Copy GLib Schemas (required by GTK 4 on Windows)
if [ -d "${MINGW_PREFIX_DIR}/share/glib-2.0/schemas" ]; then
    mkdir -p "${STAGE_DIR}/share/glib-2.0/schemas"
    cp -r "${MINGW_PREFIX_DIR}"/share/glib-2.0/schemas/* "${STAGE_DIR}/share/glib-2.0/schemas/" 2>/dev/null || true
    if command -v glib-compile-schemas >/dev/null 2>&1; then
        glib-compile-schemas "${STAGE_DIR}/share/glib-2.0/schemas" 2>/dev/null || true
    fi
fi

# 5. Copy GTK Icons & update cache
if [ -d "${MINGW_PREFIX_DIR}/share/icons" ]; then
    mkdir -p "${STAGE_DIR}/share/icons"
    for icon_theme in Adwaita hicolor; do
        if [ -d "${MINGW_PREFIX_DIR}/share/icons/${icon_theme}" ]; then
            cp -r "${MINGW_PREFIX_DIR}/share/icons/${icon_theme}" "${STAGE_DIR}/share/icons/" 2>/dev/null || true
            if command -v gtk4-update-icon-cache >/dev/null 2>&1; then
                gtk4-update-icon-cache -q "${STAGE_DIR}/share/icons/${icon_theme}" 2>/dev/null || true
            fi
        fi
    done
fi

# 6. Copy gdk-pixbuf loaders if present
if [ -d "${MINGW_PREFIX_DIR}/lib/gdk-pixbuf-2.0" ]; then
    mkdir -p "${STAGE_DIR}/lib/gdk-pixbuf-2.0"
    cp -r "${MINGW_PREFIX_DIR}/lib/gdk-pixbuf-2.0"/* "${STAGE_DIR}/lib/gdk-pixbuf-2.0/" 2>/dev/null || true
fi

# 7. Iteratively detect and copy all required runtime DLLs
echo "Collecting runtime DLL dependencies..."

copy_dll_if_needed() {
    local dll_name="$1"
    # Skip standard Windows system DLLs that must not be bundled
    case "$dll_name" in
        advapi32.dll|kernel32.dll|user32.dll|gdi32.dll|shell32.dll|msvcrt.dll|ucrtbase.dll|ntdll.dll|winmm.dll|ws2_32.dll|ole32.dll|oleaut32.dll|comctl32.dll|comdlg32.dll|imm32.dll|shlwapi.dll|version.dll|secur32.dll|crypt32.dll|bcrypt.dll|rpcrt4.dll|wldap32.dll|dnsapi.dll|iphlpapi.dll|setupapi.dll|uxtheme.dll|dwmapi.dll)
            return 0
            ;;
    esac

    if [ ! -f "${STAGE_DIR}/${dll_name}" ]; then
        if [ -f "${MINGW_BIN}/${dll_name}" ]; then
            cp "${MINGW_BIN}/${dll_name}" "${STAGE_DIR}/"
            return 1
        fi
        local lower_name
        lower_name=$(echo "$dll_name" | tr '[:upper:]' '[:lower:]')
        if [ ! -f "${STAGE_DIR}/${lower_name}" ] && [ -f "${MINGW_BIN}/${lower_name}" ]; then
            cp "${MINGW_BIN}/${lower_name}" "${STAGE_DIR}/"
            return 1
        fi
    fi
    return 0
}

copy_dependencies() {
    local added=1
    local round=1
    while [ "$added" -gt 0 ]; do
        added=0
        echo "  Dependency resolution round ${round}..."
        round=$((round + 1))

        # Check all executables and DLLs in STAGE_DIR and subdirectories
        while IFS= read -r binary; do
            [ -f "$binary" ] || continue

            # Method A: objdump PE import table analysis
            if command -v objdump >/dev/null 2>&1; then
                for dll in $(objdump -p "$binary" 2>/dev/null | grep -i 'DLL Name:' | awk '{print $3}'); do
                    if ! copy_dll_if_needed "$dll"; then
                        added=$((added + 1))
                    fi
                done
            fi

            # Method B: ntldd (MSYS2 PE dependency analyzer)
            if command -v ntldd >/dev/null 2>&1; then
                for dll_path in $(ntldd -R "$binary" 2>/dev/null | grep -iE "${MINGW_PREFIX_DIR}|mingw|ucrt" | awk '{print $3}'); do
                    if [ -f "$dll_path" ]; then
                        dll_file=$(basename "$dll_path")
                        if [ ! -f "${STAGE_DIR}/${dll_file}" ]; then
                            cp "$dll_path" "${STAGE_DIR}/"
                            added=$((added + 1))
                        fi
                    fi
                done
            fi

            # Method C: ldd fallback
            if command -v ldd >/dev/null 2>&1; then
                for dll_path in $(ldd "$binary" 2>/dev/null | grep -iE "${MINGW_PREFIX_DIR}|mingw|ucrt" | awk '{print $3}'); do
                    if [ -f "$dll_path" ]; then
                        dll_file=$(basename "$dll_path")
                        if [ ! -f "${STAGE_DIR}/${dll_file}" ]; then
                            cp "$dll_path" "${STAGE_DIR}/"
                            added=$((added + 1))
                        fi
                    fi
                done
            fi
        done < <(find "${STAGE_DIR}" -type f \( -name "*.exe" -o -name "*.dll" \))
    done
}

copy_dependencies

# 8. Create ZIP archive
ABS_ZIP_FILE="$(cd "${DIST_DIR}" && pwd)/$(basename "${ZIP_FILE}")"
rm -f "${ABS_ZIP_FILE}"

echo "Archiving release to ${ZIP_FILE}..."
if command -v 7z >/dev/null 2>&1; then
    (cd build/windows-pkg && 7z a -tzip "${ABS_ZIP_FILE}" "${APP_NAME}")
elif command -v zip >/dev/null 2>&1; then
    (cd build/windows-pkg && zip -r -q "${ABS_ZIP_FILE}" "${APP_NAME}")
elif command -v powershell.exe >/dev/null 2>&1; then
    powershell.exe -NoProfile -NonInteractive -Command "Compress-Archive -Path 'build/windows-pkg/${APP_NAME}' -DestinationPath '${ABS_ZIP_FILE}' -Force"
else
    echo "Warning: neither 7z, zip, nor powershell found, could not create zip archive."
fi

if [ -f "${ABS_ZIP_FILE}" ]; then
    echo "Windows packaging complete: ${ZIP_FILE} ($(du -sh "${ABS_ZIP_FILE}" | cut -f1))"
else
    echo "Packaging finished: ${STAGE_DIR}"
fi
