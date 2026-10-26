// AscSpawnVisibility.cpp (see SpawnVisibility.h): spawn visibility through ObjectSpawnVisibilitySettings.dbc.
#include "CoA/Source/Modules/SpawnVisibility.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Log.h"
#include <cstring>
#include <map>
#include <vector>

namespace {
    CoA::SpawnVisibility::Deps s_deps;

    using Groups = std::map<uint32_t, std::vector<const uint8_t*>>;

    // FUN_10211db0: +0x14 a completed quest, +0x18 an earned achievement (0 = no requirement).
    bool RowPasses(const uint8_t* row) {
        const uint32_t quest = CoA::Dbc::Table::U32(row, 0x14);

        if (quest != 0 && !(s_deps.questCompleted && s_deps.questCompleted(quest))) {
            return false;
        }

        const uint32_t achievement = CoA::Dbc::Table::U32(row, 0x18);

        if (achievement != 0) {
            return s_deps.achievementCompleted && s_deps.achievementCompleted(achievement);
        }

        return true;
    }

    bool SetPasses(const Groups& groups) {
        if (groups.empty()) {
            return true;
        }

        for (const auto& group : groups) {
            bool all = true;

            for (const uint8_t* row : group.second) {
                if (!RowPasses(row)) {
                    all = false;
                    break;
                }
            }

            if (all) {
                return true;
            }
        }

        return false;
    }

    // FUN_100a03b0: the GUID's counter -- the low 24 bits except for players and the kinds below.
    uint32_t GuidCounter(uint64_t guid) {
        const uint32_t lo = static_cast<uint32_t>(guid);
        const uint32_t hi = static_cast<uint32_t>(guid >> 32);

        if (hi < 0x10000000) {
            return lo;
        }

        switch (hi >> 16) {
            case 0xF101:
            case 0x1FC0:
            case 0x1F40:
            case 0x1F50:
            case 0x4000:
            case 0xF100:
                return lo;
            default:
                return lo & 0xFFFFFF;
        }
    }
}

CoA::SpawnVisibility::Deps& CoA::SpawnVisibility::Dependencies() {
    return s_deps;
}

uint32_t CoA::SpawnVisibility::TypeIdByName(const char* name, uint32_t fallback) {
    static const char* const kNames[] = { "TYPEID_OBJECT", "TYPEID_ITEM", "TYPEID_CONTAINER", "TYPEID_UNIT", "TYPEID_PLAYER",
        "TYPEID_GAMEOBJECT", "TYPEID_DYNAMICOBJECT", "TYPEID_CORPSE" };

    for (uint32_t i = 0; i < 8; ++i) {
        if (strcmp(name, kNames[i]) == 0) {
            return i;
        }
    }

    CoA::Log::Printf("Unexpected Value: %s", name);
    return fallback;
}

bool CoA::SpawnVisibility::Visible(uint32_t type, uint32_t entry, uint32_t counter) {
    CoA::Dbc::Table& t = CoA::Dbc::Get("DBFilesClient\\ObjectSpawnVisibilitySettings.dbc");
    Groups byEntry;
    Groups byCounter;

    for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
        const uint8_t* row = t.Row(id);

        if (!row || TypeIdByName(t.Str(row, 8), 0) != type) {
            continue;
        }

        const uint32_t e = CoA::Dbc::Table::U32(row, 0xC);

        if (e != 0 && e == entry) {
            byEntry[CoA::Dbc::Table::U32(row, 4)].push_back(row);
        }

        const uint32_t c = CoA::Dbc::Table::U32(row, 0x10);

        if (c != 0 && c == counter) {
            byCounter[CoA::Dbc::Table::U32(row, 4)].push_back(row);
        }
    }

    return SetPasses(byEntry) && SetPasses(byCounter);
}

bool CoA::SpawnVisibility::BeforeAuraApply(void* object, uint32_t, const void* spell) {
    uint32_t id;
    memcpy(&id, spell, 4);

    if (id != 0x16E59) {
        return true;
    }

    ObjectInfo info;

    if (!s_deps.objectInfo || !s_deps.objectInfo(object, info)) {
        return true;
    }

    return !Visible(info.type, info.entry, GuidCounter(info.guid));
}

void CoA::SpawnVisibility::Init() {
    // FUN_10278430 (list 0x10BE2B6C, before 0x724820) at 0x101A2025.
    CoA::ClientHooks::addAuraEffect.Add(CoA::SpawnVisibility::BeforeAuraApply, 0x101A2025);
}

void CoA::SpawnVisibility::Reset() {
    s_deps = Deps();
}
