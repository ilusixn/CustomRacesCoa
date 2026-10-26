#include "CoA/Source/Modules/AddOnWatch.h"
#include <cctype>

namespace {
    std::vector<std::string> s_pending;
    bool s_all = false;
    bool s_dirty = false;
    uint32_t s_lastChange = 0;

    bool SameName(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) {
            return false;
        }

        for (size_t i = 0; i < a.size(); i++) {
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }

        return true;
    }

    bool EndsWithToc(const std::string& s) {
        return s.size() > 4 && SameName(s.substr(s.size() - 4), ".toc");
    }
}

std::string CoA::AddOnWatch::AddOnForChange(const char* relativePath) {
    if (!relativePath) {
        return {};
    }

    std::string path(relativePath);
    size_t first = path.find_first_of("\\/");
    std::string name = path.substr(0, first);

    if (name.empty() || name[0] == '.') {
        return {};
    }

    // The folder itself: added, removed or renamed.
    if (first == std::string::npos) {
        return name;
    }

    // A .toc directly in it; anything deeper (or any other file) leaves the list as it is.
    std::string rest = path.substr(first + 1);

    if (rest.find_first_of("\\/") != std::string::npos || !EndsWithToc(rest)) {
        return {};
    }

    return name;
}

void CoA::AddOnWatch::Note(const std::string& name, uint32_t now) {
    s_dirty = true;
    s_lastChange = now;

    for (const std::string& pending : s_pending) {
        if (SameName(pending, name)) {
            return;
        }
    }

    s_pending.push_back(name);
}

void CoA::AddOnWatch::NoteAll(uint32_t now) {
    s_dirty = true;
    s_all = true;
    s_lastChange = now;
}

bool CoA::AddOnWatch::Take(uint32_t now, std::vector<std::string>& names, bool& all) {
    if (!s_dirty || now - s_lastChange < kSettleMs) {
        return false;
    }

    names.swap(s_pending);
    s_pending.clear();
    all = s_all;
    s_all = false;
    s_dirty = false;
    return true;
}

void CoA::AddOnWatch::Reset() {
    s_pending.clear();
    s_all = false;
    s_dirty = false;
    s_lastChange = 0;
}
