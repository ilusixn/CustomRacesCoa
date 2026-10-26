// AscInitHooks.cpp (see InitHooks.h): the decisions of the main init's detours.
#include "CoA/Source/Modules/InitHooks.h"
#include "CoA/Source/Modules/GlobalsJ.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include <cstring>

namespace {
    CoA::InitHooks::Deps s_deps;
    bool s_trajectoryM2Collision = true;   // the byte 1 at 0x6FD335 / 0x6FD4C7 / 0x6FD59E

    const uint32_t kQuestLogSlots = 0x19;

    uint32_t PlayerAddon(uint32_t index) {
        const uint64_t guid = s_deps.activePlayerGuid ? s_deps.activePlayerGuid() : 0;
        return CoA::ObjectAddon::Unit(guid)[index & 0xFFFF].value;
    }

    // The quest id in a slot; false without an active player.
    bool SlotQuest(uint32_t slot, uint32_t& questId) {
        questId = 0;
        return s_deps.questLogQuest && s_deps.questLogQuest(slot, questId);
    }

    // The first slot whose quest is questId, or -1 (also without a player).
    int32_t FirstSlot(uint32_t questId) {
        for (uint32_t i = 0; i < kQuestLogSlots; ++i) {
            uint32_t id = 0;

            if (!SlotQuest(i, id)) {
                return -1;
            }

            if (id != 0 && id == questId) {
                return static_cast<int32_t>(i);
            }
        }

        return -1;
    }
}

CoA::InitHooks::Deps& CoA::InitHooks::Dependencies() {
    return s_deps;
}

bool CoA::InitHooks::QuestExperience(uint32_t questId, int32_t& xp) {
    const int32_t slot = FirstSlot(questId);

    if (slot < 0) {
        return false;
    }

    xp = static_cast<int32_t>(PlayerAddon(static_cast<uint32_t>(slot) + 0x24));
    return true;
}

bool CoA::InitHooks::QuestLevel(uint32_t questId, int32_t& level) {
    const int32_t slot = FirstSlot(questId);

    if (slot < 0) {
        return false;
    }

    level = static_cast<int32_t>(PlayerAddon(static_cast<uint32_t>(slot) + 0x3D));
    return true;
}

int32_t CoA::InitHooks::QuestLinkLevel(uint32_t questId, int32_t level) {
    for (uint32_t i = 0; i < kQuestLogSlots; ++i) {
        uint32_t id = 0;

        if (!SlotQuest(i, id)) {
            return level;
        }

        if (id != 0 && id == questId) {
            level = static_cast<int32_t>(PlayerAddon(i + 0x3D));
        }
    }

    return level;
}

bool CoA::InitHooks::AutoRangedCombatValue(const char* value, bool rangedEquipped, int32_t& out) {
    if (!value || strcmp(value, "1") != 0) {
        return false;
    }

    out = rangedEquipped ? 1 : 0;
    return true;
}

uint32_t CoA::InitHooks::FadeTime(uint32_t time) {
    return time < 0x186A1 ? time : 1000;
}

bool CoA::InitHooks::AlwaysUsableMounted(uint32_t entry) {
    return entry == 0xDBBAA;
}

const char* CoA::InitHooks::FlagToken(uint32_t index) {
    return index ? "AllianceFlag" : "HordeFlag";
}

void CoA::InitHooks::BeforeMissileFirePos(int32_t spellClassSet, bool haveVisual, int32_t hasMissile, int32_t impactKit,
    int32_t followGroundFlags) {
    if (haveVisual && spellClassSet && hasMissile && impactKit && followGroundFlags) {
        s_trajectoryM2Collision = false;
    }
}

void CoA::InitHooks::BeforeTrajectoryTargeting() {
    s_trajectoryM2Collision = true;
}

uint32_t CoA::InitHooks::TrajectoryIntersectFlags() {
    return s_trajectoryM2Collision ? 0x100111u : 0x100011u;
}

bool CoA::InitHooks::UnitCombatLogSkipped() {
    return CoA::GlobalsJ::CombatLogAllDisabled();
}

void CoA::InitHooks::Reset() {
    s_deps = Deps();
    s_trajectoryM2Collision = true;
}
