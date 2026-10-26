// AscModelNames.cpp (see ModelNames.h): PopulateModelNameCache / GetModelNameCompletions.
#include "CoA/Source/Modules/ModelNames.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <unordered_set>

namespace {
    CoA::ModelNames::Deps s_deps;
    std::vector<std::string> s_paths;   // 0x10D3D73C

    struct Adder {
        std::unordered_set<std::string> seen;

        void Add(const char* path) {
            if (!path || !this->seen.insert(path).second) {
                return;
            }

            std::string normal = path;
            std::replace(normal.begin(), normal.end(), '\\', '/');
            s_paths.push_back(normal);
        }
    };

    void AddPath(const char* path, void* ctx) {
        static_cast<Adder*>(ctx)->Add(path);
    }

    // FUN_10a701b0: returns 1 with nothing pushed (the caller gets whatever is on top).
    int PopulateModelNameCache(lua_State*) {
        CoA::ModelNames::Populate();
        return 1;
    }

    // FUN_10a6ef50
    int GetModelNameCompletions(lua_State* L) {
        const std::string query = CoA::Script::CheckString(L, 1);
        const uint32_t limit = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2)));
        const std::vector<std::string> matches = CoA::ModelNames::Completions(query, limit);
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        int32_t index = 1;

        for (const std::string& path : matches) {
            CoA::Script::PushInt(L, index++);
            CoA::Script::PushStr(L, path.c_str());
            lua_settable(L, -3);
        }

        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "PopulateModelNameCache", PopulateModelNameCache),
        COA_NATIVE("", "GetModelNameCompletions", GetModelNameCompletions),
    };
}

CoA::ModelNames::Deps& CoA::ModelNames::Dependencies() {
    return s_deps;
}

void CoA::ModelNames::Populate() {
    s_paths.clear();
    Adder adder;

    if (s_deps.clientPaths) {
        s_deps.clientPaths(AddPath, &adder);
    }

    CoA::Dbc::Table& collections = CoA::Dbc::Get("DBFilesClient\\ItemDisplayInfoCollections.dbc");
    const uint32_t max = collections.MaxId();

    for (uint32_t id = collections.MinId(); id < max + 1; ++id) {
        if (const uint8_t* row = collections.Row(id)) {
            adder.Add(collections.Str(row, 0x10));
        }
    }
}

const std::vector<std::string>& CoA::ModelNames::Paths() {
    return s_paths;
}

std::vector<std::string> CoA::ModelNames::Completions(const std::string& query, uint32_t limit) {
    std::vector<std::string> tokens;
    std::string current;

    for (char c : query) {
        if (c == ' ') {
            tokens.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    std::vector<std::string> quoted;
    std::vector<std::string> plain;

    for (const std::string& t : tokens) {
        if (t.size() < 3) {
            continue;
        }

        if (t.front() == '"' && t.back() == '"') {
            quoted.push_back(t.substr(1, t.size() - 2));
        } else {
            plain.push_back(t);
        }
    }

    struct Match {
        std::string path;
        uint32_t score;
    };

    std::vector<Match> matches;
    auto contains = [](const std::string& path, const std::string& term) {
        return term.size() <= path.size() && (term.empty() || path.find(term) != std::string::npos);
    };

    for (const std::string& path : s_paths) {
        uint32_t score = 0;
        bool any = false;

        for (const std::string& q : quoted) {
            if (contains(path, q)) {
                ++score;
                any = true;
            }
        }

        if (!quoted.empty() && !any) {
            continue;
        }

        for (const std::string& p : plain) {
            if (contains(path, p)) {
                ++score;
                any = true;
            }
        }

        if (any) {
            matches.push_back({ path, score });
        }
    }

    std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) { return a.score > b.score; });

    if (limit < matches.size()) {
        matches.resize(limit);
    }

    std::vector<std::string> out;

    for (const Match& m : matches) {
        out.push_back(m.path);
    }

    return out;
}

void CoA::ModelNames::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}

void CoA::ModelNames::Reset() {
    s_deps = Deps();
    s_paths.clear();
}
