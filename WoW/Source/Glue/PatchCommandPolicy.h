#pragma once

#include <cctype>
#include <cstring>

// CoA64: security hardening (not in the 32-bit client). The patch-download prepatch.lst runs unauthenticated on
// Ascension (Extensions.dll patch 0x4E57FE), so any server can ship one. Its "extract" / "delete" paths are confined
// to the download folder, "extract" may not write runnable files, and "execute" is refused (CGlueMgr::
// PatchDownloadExecuteCommand) -- together the server-controlled code execution RCEPatcher2 closes.

// Whether a path component names a folder the prepatch may not touch (user decision 2026-10-26: addon code and
// settings): Interface and WTF, compared the way Windows resolves them (case-insensitive, trailing dots / spaces
// ignored).
inline bool PatchProtectedFolder(const char* component, size_t length) {
    while (length && (component[length - 1] == '.' || component[length - 1] == ' ')) {
        length--;
    }

    return (length == 9 && _strnicmp(component, "interface", 9) == 0) || (length == 3 && _strnicmp(component, "wtf", 3) == 0);
}

// Relative, no drive / device / stream, no parent-directory component, no 8.3 short name ('~'), and not inside
// Interface\ or WTF\.
inline bool PatchPathConfined(const char* path) {
    if (!path || !*path || path[0] == '\\' || path[0] == '/' || strchr(path, ':') || strchr(path, '~')) {
        return false;
    }

    const char* component = path;
    bool first = true;

    for (const char* c = path;; c++) {
        if (*c == '\\' || *c == '/' || *c == '\0') {
            const size_t length = static_cast<size_t>(c - component);

            if (length == 2 && component[0] == '.' && component[1] == '.') {
                return false;
            }

            // "." components do not move the path; the first real one is the top-level folder.
            if (first && !(length == 1 && component[0] == '.')) {
                if (PatchProtectedFolder(component, length)) {
                    return false;
                }

                first = false;
            }

            if (*c == '\0') {
                return true;
            }

            component = c + 1;
        }
    }
}

inline bool PatchExtractAllowed(const char* path) {
    if (!PatchPathConfined(path)) {
        return false;
    }

    // Windows ignores trailing dots and spaces in a file name.
    size_t end = strlen(path);

    while (end && (path[end - 1] == '.' || path[end - 1] == ' ')) {
        end--;
    }

    static const char* const kRunnable[] = {
        ".exe", ".dll", ".com", ".bat", ".cmd", ".ps1", ".vbs", ".vbe", ".js", ".jse", ".wsf", ".wsh", ".scr",
        ".msi", ".msp", ".lnk", ".pif", ".cpl", ".hta", ".reg", ".sys", ".ocx", ".jar",
    };

    for (const char* extension : kRunnable) {
        const size_t length = strlen(extension);

        if (end >= length && _strnicmp(path + end - length, extension, length) == 0) {
            return false;
        }
    }

    return true;
}
