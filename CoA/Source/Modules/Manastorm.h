#ifndef COA_SOURCE_MODULES_MANASTORM_H
#define COA_SOURCE_MODULES_MANASTORM_H

#include <cstdint>
#include <string>
#include <vector>

class CDataStore;

// AscManastorm.cpp: C_Manastorm (24 natives, registrar 0x102A7FE0) over ManastormMgr (FUN_102a4760, static 0x10BE31A8;
// module init 0x102A4540). Checked against the genuine DLL.
//
//   +0x00 levels completed this run (0x65E appends, ENTER_MANASTORM_OK clears)
//   +0x0C active level  +0x10 active Manastorm.dbc id  +0x14 ActiveManastormType (index into SOLO..MAX)
//   +0x18 u32 / +0x1C float / +0x20 u32  cache info
//   +0x24 completed levels received before the player existed (+0x84 set), moved to +0x30 after the idle tick
//   +0x30 unordered_map<u64 guid, vector<vector<u32>>> completed levels per type, each list sorted ascending
//   +0x50 chaotic link  +0x54 the spell of the loadout change in flight  +0x58 loadout (slot -> spell)
//   +0x64 unordered_map<u64 guid, unordered_map<u32 item, reward>> reward visibility
//   0x10BE31A0 i64: ShowObjectiveIcon's time + 10 (never compared with the clock; > 0 means on, never reset)
//
//   SMSG 0x652 ENTER_RESULT, 0x666 LEAVE_RESULT (C string); 0x660 ACTIVE_DATA {u32 level, u32 id, C string type, u32,
//   f32, u32}; 0x65F the player's completed levels; 0x65D {u64 guid, the same lists}; 0x65E {u32 level}; 0x67B fail;
//   0x67F {u32 chaotic link}; 0x688 {u32 n, n x u32 spell}; 0x68A {u32 slot, C string}; 0x68B {u32 slot, u32 spell};
//   0x6AD reward visibility (bounds-checked); 0x67D / 0x67E Manastorm.dbc / ManastormMessages.dbc row patches.
//   CMSG 0x651 Enter {u32 level}, 0x665 Leave, 0x689 SetLoadoutSpellAtIndex {u32 slot, u32 spell}.
//
// Entering (active level 0 -> non-zero) turns 0x80DB37's jne into jmp (ClientHooks::manastormSkipsModalSpell) and
// sets the flags (+0x14) of SpellRange.dbc rows 114 and 445 to 0; leaving restores the objective-icon scale
// (FUN_102a6c10(1.0)), the jne, and those flags to 2.
namespace CoA::Manastorm {
    // The client questions; ManastormClient.cpp installs them.
    struct Deps {
        bool (*hasPlayer)() = nullptr;                                        // 0x4038F0
        uint64_t (*playerGuid)() = nullptr;                                   // 0x4D3790
        uint32_t (*playerLevel)() = nullptr;                                  // descriptor +0xD8
        bool (*playerHasAura)(uint32_t spell) = nullptr;                      // UnitHasAuraSpell on the player
        bool (*knowsSpell)(uint32_t spell) = nullptr;                         // 0x7260E0 on the player
        uint32_t (*firstRank)(uint32_t spell) = nullptr;                      // AscSpellRank::FirstRank
        const std::vector<uint32_t>* (*rankChain)(uint32_t first) = nullptr;  // AscSpellRank::Chain
        uint64_t (*partyGuid)(uint32_t index) = nullptr;                      // CGPartyInfo::m_members (0xBD1948), 4
        uint32_t (*raidCount)() = nullptr;                                    // CGRaidInfo::m_numMembers (0xBEB608)
        uint64_t (*raidGuid)(uint32_t index) = nullptr;                       // 0xBEB568
        // FetchSpell (0x100B2CB0): Spell.dbc EffectBasePoints[0] / [1] (+0x140 / +0x144).
        bool (*spellBasePoints)(uint32_t spell, int32_t& first, int32_t& second) = nullptr;
        uint32_t (*instanceDifficulty)() = nullptr;                           // CGGameUI::m_instanceDifficulty (0xBD0894)
        void (*unitTokens)(uint64_t guid, std::vector<std::string>& out) = nullptr;   // 0x60BB70
        bool (*unitTokenGuid)(const char* token, uint64_t& guid) = nullptr;
        void (*setSpellRangeFlags)(uint32_t flags) = nullptr;                // FUN_100b2590 rows 0x72 / 0x1BD, +0x14
        void (*setObjectiveIconScale)(float scale) = nullptr;                // FUN_102a6c10 (PlayerName icon quad)
        // FUN_102a4a10's object questions: its raid target index (0x5728C0), type (+0x14), and both positions
        // (vtable +0x2C); false without an active player.
        int32_t (*raidTargetIndex)(void* object) = nullptr;
        uint32_t (*objectType)(void* object) = nullptr;
        bool (*positions)(void* object, float player[3], float other[3]) = nullptr;
        int64_t (*now)() = nullptr;                                           // _time64(nullptr) when unset
    };

    Deps& Dependencies();

    // FUN_102a4a00: the active level is set.
    bool InManastorm();
    float ObjectiveIconScale();   // the last scale written (tests)
    const std::vector<uint32_t>& CompletedThisRun();   // +0x00 (nothing in the original reads it)

    int32_t OnEnterResult(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);              // 0x652
    int32_t OnLeaveResult(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);              // 0x666
    int32_t OnActiveData(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);               // 0x660
    int32_t OnData(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);                     // 0x65F
    int32_t OnUpdateMaxCompletedLevel(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);  // 0x65D
    int32_t OnCompletedLevel(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);           // 0x65E
    int32_t OnFail(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);                     // 0x67B
    int32_t OnChaoticLink(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);              // 0x67F
    int32_t OnLoadout(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);                  // 0x688
    int32_t OnLoadoutResult(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);            // 0x68A
    int32_t OnLoadoutUpdate(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);            // 0x68B
    int32_t OnRewardVisibility(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);         // 0x6AD

    void OnGlueScreen();             // FUN_102a0d70
    void ApplyPending();             // FUN_102a4ba0, after the idle tick
    void OnPreAnimate(void* object); // FUN_102a4a10, after 0x743EC0

    void Init();
    // Tests only: the manager, the icon time and the allowed-level set cleared, Deps forgotten, the client flag off.
    void Reset();
}

#endif
