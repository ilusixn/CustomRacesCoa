#include "CoA/Source/Modules/RealmData.h"
#include <cstdio>
#include <cstring>
#include <fstream>

namespace {
    CoA::RealmData::State s_state;
}

std::vector<std::string> CoA::RealmData::ListArchive(const std::string& dir) {
    std::vector<std::string> names;
    std::ifstream list(dir + "\\" + "listarchive");
    std::string line;

    while (std::getline(list, line)) {
        names.push_back(line);
    }

    return names;
}

uint32_t CoA::RealmData::BuildNumber(const std::string& dir) {
    unsigned build = 0x3034;
    std::ifstream info(dir + "buildinfo");

    if (info) {
        info >> build;
    }

    return build;
}

std::string CoA::RealmData::CachePath(const char* locale, const char* realm) {
    char path[260];
    snprintf(path, sizeof(path), "Cache/%s/%s/%s", "WDB", locale, realm);
    return path;
}

const std::string& CoA::RealmData::CurrentPath() {
    return s_state.path;
}

bool CoA::RealmData::NeedsSwitch(const std::string& path) {
    if (path == s_state.path) {
        return false;
    }

    return !(s_state.active && !s_state.finished && path == s_state.pending);
}

bool CoA::RealmData::HdPatchLoaded() {
    return s_state.hdPatch;
}

bool CoA::RealmData::IsHdOnlyArchive(const char* name) {
    static const char prefix[] = "Data\\patch-";
    static const char suffix[] = "-HD.MPQ";
    const size_t prefixLength = sizeof(prefix) - 1;
    const size_t suffixLength = sizeof(suffix) - 1;
    const size_t length = name ? strlen(name) : 0;

    if (length <= prefixLength + suffixLength) {
        return false;
    }

    return _strnicmp(name, prefix, prefixLength) == 0 && _stricmp(name + length - suffixLength, suffix) == 0;
}

CoA::RealmData::State& CoA::RealmData::GetState() {
    return s_state;
}

void CoA::RealmData::ResetState(const std::string& path) {
    s_state = CoA::RealmData::State();
    s_state.path = path;
}
