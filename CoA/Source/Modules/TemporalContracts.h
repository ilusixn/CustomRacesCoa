#ifndef COA_SOURCE_MODULES_TEMPORALCONTRACTS_H
#define COA_SOURCE_MODULES_TEMPORALCONTRACTS_H

#include <cstdint>
#include <string>
#include <vector>

// AscTemporalContracts.cpp: C_TemporalContracts (16 natives) and the TemporalContractMgr behind it (static 0x10D3C1F0
// via FUN_10339160; module init FUN_10339120: the natives after the world state's (0x1033bcb0), the glue reset
// FUN_10337120, TEMPORAL_CONTRACTS_UPDATED, enter-world FUN_1033b820 and the glue-Lua init FUN_103392f0).
//
// Manager: +0x04 an unordered_map (only ever cleared), +0x24 the contract types (100 bytes each), +0x30 a deque of
// pending activations (0x20 each). Type: str Name +0, u32 RemainingCompletions +0x18, MoneyCost +0x1C, TokenCost +0x20,
// vector<u32> quest groups +0x24, MinimumLevel +0x30, str display name +0x34, str content type +0x4C. The code that
// fills the type vector sits in the VM-protected part of the DLL and is not recoverable, so the list stays empty and
// every reader returns its empty shape.
//
// Enter-world starts a 50 ms timer (0x403370, 0x1033B840) that sends one queued activation per tick through
// FUN_10339320 -- CMSG 0x55B: u32 length, the type name's bytes (no NUL), u8 flag, u32 count -- and re-arms itself
// without storing the new id, so the glue-Lua init's cancel (FUN_103392f0) only ever stops the first tick.
//
// Quest records are the client quest cache (0x67DE90 on 0xC5DA48, through Deps): +0x10 ZoneOrSort, +0x34 reward money,
// +0x44 honor (scaled by RealmInfo +0x10), +0x54 reward items x4, +0x64 amounts x4, +0x24A0 arena points. Quest groups
// come from the DLL quest store (CoA::BridgeStore, Quest.dbc).
namespace CoA::TemporalContracts {
    // What the module reads of one quest cache record.
    struct QuestRecord {
        uint32_t zoneOrSort = 0;   // +0x10
        uint32_t money = 0;        // +0x34
        uint32_t honor = 0;        // +0x44
        uint32_t items[4] = {};    // +0x54
        uint32_t amounts[4] = {};  // +0x64
        uint32_t arena = 0;        // +0x24A0, read raw (past the client struct's arena points, inside its texts)
    };

    struct Deps {
        bool (*questRecord)(uint32_t quest, QuestRecord& out) = nullptr;   // 0x67DE90: false when not cached
        const char* (*areaName)(uint32_t id) = nullptr;                    // FUN_100b1240: AreaTable +0x2C, or nullptr
        const char* (*questSortName)(uint32_t id) = nullptr;               // FUN_100b1ea0: QuestSort +4, or nullptr
        // AscQuestAddon::Get: the quest's Callboard cache points, installed by CoA::QuestAddon::Init; unset answers as the original does
        // without a record (no points).
        bool (*questAddonPoints)(uint32_t quest, uint32_t& points) = nullptr;
        // AscCallboard::ForEachCompletion (FUN_10337270): each sub-category with completions left handed to fn once per
        // completion (all categories, or those whose content type or name is contentType); true when fn ran. Installed by
        // CoA::Callboard::Init; unset answers as the original does before the Callboard view is loaded (false, nothing run).
        bool (*forEachCompletion)(const std::string* contentType, void (*fn)(const std::string& type, void* ctx), void* ctx) = nullptr;
    };

    Deps& Dependencies();

    // A contract type as the manager holds it (+0x24 element).
    struct ContractType {
        std::string name;                // +0x00
        uint32_t remaining = 0;          // +0x18
        uint32_t moneyCost = 0;          // +0x1C
        uint32_t tokenCost = 0;          // +0x20
        std::vector<uint32_t> groups;    // +0x24
        uint32_t minimumLevel = 0;       // +0x30
        std::string displayName;         // +0x34
        std::string contentType;         // +0x4C
    };

    void Init();
    // Tests only: the manager back to its constructed state, Deps forgotten.
    void Reset();
    // Tests only: what the VM-protected filler would have put in +0x24.
    void SetTypes(const std::vector<ContractType>& types);
}

#endif
