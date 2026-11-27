// QuestLogExtended.h's store and SMSG 0x8F7 handler.
#include "CoA/Source/Modules/QuestLogExtended.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/Object/Player.h"
#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <vector>

namespace {
    constexpr uint32_t kEntryBytes = 29;
    constexpr uint8_t kVersion = 1;

    struct Slot {
        bool used = false;
        CoA::QuestLogExtended::Entry entry = {};
    };

    Slot s_slots[CoA::QuestLogExtended::kMaxSlots];
    uint32_t s_logSize = CoA::QuestLogExtended::kNativeSlots;
    std::vector<void (*)()> s_changed;
    std::vector<CoA::QuestLogExtended::Change> s_changes;
    bool s_filled = false;   // a packet has arrived since the last Clear
    bool s_first = false;    // the packet being applied is the first since the last Clear
    PlayerQuestLogEntry s_mirror[CoA::QuestLogExtended::kMaxSlots - CoA::QuestLogExtended::kNativeSlots];

    struct Parsed {
        uint8_t slot;
        CoA::QuestLogExtended::Entry entry;
    };

    int32_t OnQuestLog(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint8_t version = CoA::Read::U8(msg);
        const uint8_t flags = CoA::Read::U8(msg);
        const uint8_t logSize = CoA::Read::U8(msg);
        const uint8_t count = CoA::Read::U8(msg);

        if (CoA::Read::Failed(msg) || version != kVersion || CoA::Read::Remaining(msg) < count * kEntryBytes) {
            return 1;
        }

        // Parse everything first so a bad packet applies nothing.
        std::vector<Parsed> parsed(count);

        for (Parsed& p : parsed) {
            p.slot = CoA::Read::U8(msg);
            p.entry.questId = CoA::Read::U32(msg);
            p.entry.state = CoA::Read::U32(msg);

            for (uint16_t& c : p.entry.counters) {
                c = CoA::Read::U16(msg);
            }

            p.entry.time = CoA::Read::U32(msg);
            p.entry.level = CoA::Read::U32(msg);
            p.entry.rewardXP = CoA::Read::U32(msg);

            if (p.slot < CoA::QuestLogExtended::kNativeSlots || p.slot >= CoA::QuestLogExtended::kMaxSlots) {
                return 1;
            }
        }

        Slot before[CoA::QuestLogExtended::kMaxSlots];
        std::copy(std::begin(s_slots), std::end(s_slots), before);

        if (flags & 1) {
            for (Slot& s : s_slots) {
                s = Slot();
            }
        }

        for (const Parsed& p : parsed) {
            Slot& s = s_slots[p.slot];
            s.used = p.entry.questId != 0;
            s.entry = s.used ? p.entry : CoA::QuestLogExtended::Entry();
        }

        s_changes.clear();
        s_first = !s_filled;

        if (s_filled) {
            for (uint32_t i = CoA::QuestLogExtended::kNativeSlots; i < CoA::QuestLogExtended::kMaxSlots; ++i) {
                if (memcmp(&before[i].entry, &s_slots[i].entry, sizeof(CoA::QuestLogExtended::Entry)) != 0) {
                    s_changes.push_back({ i, before[i].entry });
                }
            }
        }

        s_filled = true;

        // A bad server value must not drive the UI limit outside the supported range.
        s_logSize = logSize < CoA::QuestLogExtended::kNativeSlots ? CoA::QuestLogExtended::kNativeSlots
                  : logSize > CoA::QuestLogExtended::kMaxSlots ? CoA::QuestLogExtended::kMaxSlots : logSize;
        CoA::QuestLogExtended::ApplyInterfaceLimit();

        for (void (*cb)() : s_changed) {
            cb();
        }

        s_changes.clear();
        s_first = false;
        return 1;
    }
}

const CoA::QuestLogExtended::Entry* CoA::QuestLogExtended::Get(uint32_t slot) {
    if (slot < kNativeSlots || slot >= kMaxSlots || !s_slots[slot].used) {
        return nullptr;
    }

    return &s_slots[slot].entry;
}

PlayerQuestLogEntry* CoA::QuestLogExtended::Mirror(uint32_t slot) {
    if (slot < kNativeSlots || slot >= kMaxSlots) {
        return nullptr;
    }

    PlayerQuestLogEntry& out = s_mirror[slot - kNativeSlots];
    out = PlayerQuestLogEntry();

    if (const Entry* e = Get(slot)) {
        out.m_questId = e->questId;
        out.m_state = e->state;

        for (uint32_t i = 0; i < 4; ++i) {
            out.m_counts[i] = e->counters[i];
        }

        out.m_time = e->time;
    }

    return &out;
}

uint32_t CoA::QuestLogExtended::LogSize() {
    return s_logSize;
}

void CoA::QuestLogExtended::OnChanged(void (*cb)()) {
    s_changed.push_back(cb);
}

const std::vector<CoA::QuestLogExtended::Change>& CoA::QuestLogExtended::Changes() {
    return s_changes;
}

bool CoA::QuestLogExtended::FirstSinceClear() {
    return s_first;
}

namespace {
    bool InOtherSlot(const uint32_t* ids, uint32_t slot, uint32_t questId) {
        for (uint32_t i = 0; i < CoA::QuestLogExtended::kMaxSlots; ++i) {
            if (i != slot && ids[i] == questId) {
                return true;
            }
        }

        return false;
    }
}

CoA::QuestLogExtended::SlotDecision CoA::QuestLogExtended::DecideSlot(const uint32_t* ids, uint32_t slot, uint32_t state,
                                                                      uint32_t oldQuest, uint32_t oldState) {
    const uint32_t quest = ids[slot];
    const bool movedIn = quest && quest != oldQuest && InOtherSlot(ids, slot, quest);
    SlotDecision d;
    d.accepted = quest && !movedIn && (quest != oldQuest || (!(state & 2) && (oldState & 2)));
    d.removePoi = oldQuest && quest != oldQuest && !InOtherSlot(ids, slot, oldQuest);
    return d;
}

void CoA::QuestLogExtended::ApplyInterfaceLimit() {
    if (lua_State* L = CoA::ClientState::MainLuaState()) {
        // MAX_WATCHABLE_QUESTS caps FrameXML's auto-watch on accept (and addons read it); the client takes 64 watches.
        for (const char* name : { "MAX_QUESTLOG_QUESTS", "MAX_WATCHABLE_QUESTS" }) {
            lua_pushnumber(L, static_cast<lua_Number>(s_logSize));
            lua_setfield(L, LUA_GLOBALSINDEX, name);
        }
    }
}

void CoA::QuestLogExtended::SendHello() {
    CoA::Packet packet(0x8F6);
    packet.U8(kVersion);
    packet.U8(kMaxSlots);
    packet.Send();
}

void CoA::QuestLogExtended::Clear() {
    for (Slot& s : s_slots) {
        s = Slot();
    }

    s_logSize = kNativeSlots;
    s_filled = false;
    s_first = false;
}

void CoA::QuestLogExtended::Reset() {
    Clear();
    s_changed.clear();
    s_changes.clear();
}

void CoA::QuestLogExtended::Init() {
    COA_PACKET(0x8F7, OnQuestLog);
    CoA::Lifecycle::OnGlueScreen(Clear);
    // FrameXML sets MAX_QUESTLOG_QUESTS to 25 each time the interface loads.
    CoA::ClientHooks::afterEnterWorld.Add(ApplyInterfaceLimit);
    CoA::ClientHooks::afterUiReload.Add(ApplyInterfaceLimit);
}
