#pragma once

#include <cctype>
#include <cstring>

// CoA64: any server can ship an unauthenticated prepatch.lst; this confines it (RCEPatcher2's fix in source).
// Rationale: docs/2026-11-26-prepatch-path-policy.md.

// Interface\ and WTF\ are off limits (user decision), compared as Windows resolves them.
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

            // Windows drops trailing dots / spaces, so "..", ".. " and "..." all climb; only a bare "." is allowed.
            size_t trimmed = length;

            while (trimmed && (component[trimmed - 1] == '.' || component[trimmed - 1] == ' ')) {
                trimmed--;
            }

            if (trimmed == 0 && length != 0 && !(length == 1 && component[0] == '.')) {
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
