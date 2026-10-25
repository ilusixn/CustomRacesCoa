#ifndef COA_SOURCE_MODULES_APPEARANCEINTERNAL_H
#define COA_SOURCE_MODULES_APPEARANCEINTERNAL_H

// What Appearance.cpp and AppearanceBrowser.cpp share (the reconstruction's AscAppearance.cpp anonymous namespace).
#include "CoA/Source/Modules/Appearance.h"
#include "CoA/Source/Runtime/Dbc.h"
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

struct lua_State;

namespace CoA::Appearance::detail {
    enum AppearanceType { TYPE_NONE, TYPE_ITEM, TYPE_ILLUSION, TYPE_INCARNATION, TYPE_SPELL_VISUAL, TYPE_COSMETIC_PET, TYPE_COSMETIC,
        TYPE_ITEM_SET, TYPE_OUTFIT, TYPE_MOUNT, TYPE_COMPANION, TYPE_TOY };

    extern const char* const kAppearanceTypes[12];   // 0x10B2A198
    extern const char* const kDisplayTypes[8];       // 0x10B3AEF0

    struct Collected {   // 16 bytes
        uint32_t id;
        uint32_t unk;
        uint64_t time;
    };

    struct Manager {
        std::vector<uint32_t> pending;                                   // +0x000
        uint32_t unk144 = 0;                                             // +0x144 (the browser's category)
        std::vector<uint32_t> current;                                   // +0x148
        std::vector<Collected> collected;                                // +0x154
        std::unordered_map<std::string, std::vector<uint32_t>> outfits;  // +0x160
        std::string pendingOutfitName;                                   // +0x180
        std::vector<uint32_t> pendingOutfit;                             // +0x198
        bool seeOwn = true;                                              // +0x1A4
        bool seeOthers = true;                                           // +0x1A5
    };

    Manager& M();
    bool& BrowserDirty();   // AppearancesEntryContainer (+0xC) reset flag, read by the browser

    inline uint32_t U32(const uint8_t* r, uint32_t off) {
        uint32_t v;
        memcpy(&v, r + off, 4);
        return v;
    }

    inline int32_t I32(const uint8_t* r, uint32_t off) {
        return static_cast<int32_t>(U32(r, off));
    }

    CoA::Dbc::Table& Categories();
    CoA::Dbc::Table& Appearances();
    CoA::Dbc::Table& Details();
    CoA::Dbc::Table& ItemAppearances();
    CoA::Dbc::Table& ItemSetAppearances();

    int CategoryType(const uint8_t* row);   // FUN_101b0ac0 -> FUN_101b07c0
    bool CategoryIs(uint32_t category, int type);
    bool CategoryAllowsClass(const uint8_t* category);   // FUN_101b0b00
    bool RealmAppropriate(const uint8_t* appearance);    // FUN_101b49f0
    bool HasCategoryOfType(const uint8_t* appearance, int type);
    bool IsItemDisplay(const uint8_t* appearance);       // FUN_101b28a0 == 1
    const uint8_t* DetailsFor(uint32_t appearance);
    const uint8_t* ItemAppearanceOf(uint32_t item);      // FUN_1020fae0
    const uint8_t* ItemSetAppearanceOf(uint32_t set);    // FUN_10210030
    const std::vector<const uint8_t*>* ItemsForAppearance(uint32_t appearance);
    std::vector<uint32_t> SetsWithAppearance(uint32_t appearance);   // FUN_102100b0
    bool IsCollected(uint32_t appearance);               // FUN_100c57a0
    uint64_t CollectedTime(uint32_t appearance);         // FUN_100c3ab0
    void ItemSetAppearanceList(const ItemSetRecord& set, std::vector<uint32_t>& out);   // FUN_100c3ba0
    std::string AppearanceName(const uint8_t* appearance);   // FUN_101b41e0
    uint32_t At(const std::vector<uint32_t>& v, uint32_t i);
    void PendingChanged();

    bool FetchItem(uint32_t id, ItemRecord& out);
    bool FetchItemSet(uint32_t id, ItemSetRecord& out);
    bool HasPlayer();
    uint32_t PlayerClass();

    void PushIntList(lua_State* L, const std::vector<uint32_t>& v);   // FUN_1009a460

    void InitBrowser();    // AppearanceBrowser.cpp
    void ResetBrowser();
}

#endif
