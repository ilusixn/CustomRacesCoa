#ifndef COA_SOURCE_MODULES_QUESTLOGEXTENDED_H
#define COA_SOURCE_MODULES_QUESTLOGEXTENDED_H

#include <cstdint>
#include <vector>

struct PlayerQuestLogEntry;

// CoA64 addition: the quest log slots 25..63 the server sends in SMSG 0x8F7 (the native log stops at 25). Slots
// below kNativeSlots stay in the player descriptor; this store only holds the extension.
namespace CoA::QuestLogExtended {
    constexpr uint32_t kNativeSlots = 25, kMaxSlots = 64;

    struct Entry {
        uint32_t questId, state;
        uint16_t counters[4];
        uint32_t time, level, rewardXP;
    };

    // Slot 25..63; nullptr outside that range or when the slot is empty.
    const Entry* Get(uint32_t slot);
    // CGPlayer::GetQuestLogEntry's view of slot 25..63, refreshed from the store on every call: a zeroed entry
    // when the slot is empty, nullptr outside 25..63. Each slot has its own entry, so several may be held at once.
    PlayerQuestLogEntry* Mirror(uint32_t slot);
    // The log size the server sent; kNativeSlots until a packet sets it.
    uint32_t LogSize();
    // Runs after every applied 0x8F7.
    void OnChanged(void (*cb)());

    struct Change {
        uint32_t slot;
        Entry old;   // zeroed when the slot was empty
    };
    // While OnChanged runs: the slots the packet changed, in slot order. None for the first packet after Clear, which
    // fills the log as a login does (the descriptor's mirror handlers do not run for a new object either).
    const std::vector<Change>& Changes();
    // While OnChanged runs: true for the first packet after Clear (the login snapshot).
    bool FirstSinceClear();

    // OnUpdateQuest's per-slot decisions for `slot`, from the quest ids now in slots 0..63 (`ids`) and the slot's
    // entry before the change. A quest another slot still holds has moved (the server's promotion): no accept, no drop.
    struct SlotDecision {
        bool accepted;    // "Quest accepted": message, tutorials, QUEST_ACCEPTED
        bool removePoi;   // RemoveQuestPointDisplay(oldQuest)
    };
    SlotDecision DecideSlot(const uint32_t* ids, uint32_t slot, uint32_t state, uint32_t oldQuest, uint32_t oldState);
    // Sets the interface's MAX_QUESTLOG_QUESTS and MAX_WATCHABLE_QUESTS to LogSize() in the main Lua state.
    void ApplyInterfaceLimit();
    // CMSG 0x8F6 { u8 version = 1, u8 clientMaxSlots = 64 }.
    void SendHello();
    // The store emptied and LogSize back to kNativeSlots.
    void Clear();

    void Init();
    // Tests only.
    void Reset();
}

#endif
