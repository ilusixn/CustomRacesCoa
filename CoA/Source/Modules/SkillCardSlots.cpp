// AscSkillCardSlots.cpp (see SkillCard.h): C_SkillCard, the skill cards slotted per specialization, and the bare
// GetHandOfFateQuests / GetScrollOfFortuneQuests (FUN_102f7c30 registers them after the C_SkillCard table).
#include "CoA/Source/Modules/SkillCard.h"
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/Wildcard.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <vector>

namespace {
    using CoA::SkillCard::Card;
    using CoA::Script::CheckNumber;
    using CoA::Script::CheckString;
    using CoA::Script::PushBool;
    using CoA::Script::PushStr;
    using CoA::Script::ToInt;

    struct Slot {
        uint32_t cardId = 0;
        uint32_t rank = 0;
        uint8_t blocked = 0;
    };

    std::vector<std::vector<std::vector<Slot>>> s_specs;   // 0x10BE3AC4

    const uint32_t kTypeCount = 9;                    // FUN_102f41f0 / 0x10B5752C: types 0..8
    const uint32_t kLevels[3] = { 0, 25, 40 };        // 0x10B57550: the level each default slot opens at

    // The 7a convention: a slot record is read only while its 9 bytes are left; the per-type count while 4 are left.
    const uint32_t kSlotSize = 9;
    // SMSG 0x624's spec count and SMSG 0x625's spec / slot index size vectors; past these the original's allocation
    // fails.
    const uint32_t kMaxIndex = 0x10000;

    uint32_t Spec() {   // FUN_1016f580, unchecked: -1 reads as 0xFFFFFFFF
        const int32_t s = CoA::GameMode::ActiveSpecIndex();
        return s < 0 ? 0xFFFFFFFF : static_cast<uint32_t>(s);
    }

    const Slot* SlotAt(uint32_t spec, uint32_t type, uint32_t i) {   // FUN_102f43c0
        if (spec >= s_specs.size() || type >= s_specs[spec].size() || i >= s_specs[spec][type].size()) {
            return nullptr;
        }

        return &s_specs[spec][type][i];
    }

    uint32_t Count(uint32_t spec, uint32_t type) {   // FUN_102f4440
        return (spec < s_specs.size() && type < s_specs[spec].size()) ? static_cast<uint32_t>(s_specs[spec][type].size()) : 0;
    }

    // FUN_102f51a0: a slot is blocked when its starter-ness disagrees with the build's starting phase, when the server
    // blocked it, or (default slots 2 and 3) below their level.
    bool Blocked(uint32_t spec, uint32_t type, uint32_t i) {
        const bool starter = type == 2 || type == 3;
        bool phase = false;

        if (const CoA::CA::Build* b = CoA::CA::ActiveBuild()) {   // FUN_1016f390
            if (b->wildcard) {
                phase = b->u44 < CoA::WildcardRolls::StartingCount();   // FUN_10159210
            } else if (b->draft) {
                phase = b->GlobalAE(0) < (((CoA::GameMode::Mode() >> 7) & 1) ? 0x12u : 8u);
            }
        }

        const Slot* s = SlotAt(spec, type, i);

        if (starter != phase || (s && s->blocked)) {
            return true;
        }

        if ((type == 0 || type == 1) && i < 3 && kLevels[i] != 0) {
            const CoA::SkillCard::Deps& d = CoA::SkillCard::Dependencies();

            if (!d.hasPlayer || !d.hasPlayer()) {
                return true;
            }

            if ((d.playerLevel ? d.playerLevel() : 0) < kLevels[i]) {
                return true;
            }
        }

        return false;
    }

    int32_t ConfigInt(const char* name, int32_t fallback) {
        const int32_t* v = CoA::Config::Int(name);
        return v ? *v : fallback;
    }

    // (type name, 1-based slot) -- FUN_100e96d0 then FUN_102f3f50.
    bool ReadTypeSlot(lua_State* L, int& type, uint32_t& slot) {
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
            return false;
        }

        type = CoA::SkillCard::TypeIndex(CheckString(L, 1));
        slot = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        return type >= 0 && slot != 0;
    }

    int GetCardCount(lua_State* L) {   // FUN_102f53e0
        std::string s;

        if (!CoA::Script::ReadString(L, s)) {
            return 0;
        }

        const int t = CoA::SkillCard::TypeIndex(s);

        if (t < 0) {
            return 0;
        }

        lua_pushinteger(L, static_cast<int32_t>(Count(Spec(), static_cast<uint32_t>(t))));
        return 1;
    }

    int GetCardAtIndex(lua_State* L) {   // FUN_102f5290: card id, rank
        int t;
        uint32_t i;

        if (!ReadTypeSlot(L, t, i)) {
            return 0;
        }

        const Slot* s = SlotAt(Spec(), static_cast<uint32_t>(t), i - 1);
        lua_pushinteger(L, static_cast<int32_t>(s ? s->cardId : 0));
        lua_pushinteger(L, static_cast<int32_t>(s ? s->rank : 0));
        return 2;
    }

    int GetSkillCardInfoAtIndex(lua_State* L) {   // FUN_102f6970
        int t;
        uint32_t i;

        if (!ReadTypeSlot(L, t, i)) {
            return 0;
        }

        const Slot* s = SlotAt(Spec(), static_cast<uint32_t>(t), i - 1);
        const Card* c = CoA::SkillCard::ByCardRank(s ? s->cardId : 0, s ? s->rank : 0);

        if (c) {
            CoA::SkillCard::PushCard(L, *c);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetCardRankAtIndex(lua_State* L) {   // FUN_102f5550
        int t;
        uint32_t i;

        if (!ReadTypeSlot(L, t, i)) {
            return 0;
        }

        const Slot* s = SlotAt(Spec(), static_cast<uint32_t>(t), i - 1);
        lua_pushinteger(L, static_cast<int32_t>(s ? s->rank : 0));
        return 1;
    }

    int IsCardAtIndexActive(lua_State* L) {   // FUN_102f6b90: slotted and not blocked
        int t;
        uint32_t i;

        if (!ReadTypeSlot(L, t, i)) {
            return 0;
        }

        const Slot* s = SlotAt(Spec(), static_cast<uint32_t>(t), i - 1);
        PushBool(L, s && s->cardId != 0 && !Blocked(Spec(), static_cast<uint32_t>(t), i - 1));
        return 1;
    }

    int IsCardAtIndexBlocked(lua_State* L) {   // FUN_102f6cd0: the server's flag only
        int t;
        uint32_t i;

        if (!ReadTypeSlot(L, t, i)) {
            return 0;
        }

        const Slot* s = SlotAt(Spec(), static_cast<uint32_t>(t), i - 1);
        PushBool(L, s && s->blocked);
        return 1;
    }

    // FUN_102f5c80: the realm's slot count for the type (felforged mode: mode bit 7).
    int GetMaxCardCount(lua_State* L) {
        std::string s;

        if (!CoA::Script::ReadString(L, s)) {
            return 0;
        }

        const int t = CoA::SkillCard::TypeIndex(s);

        if (t < 0) {
            return 0;
        }

        const bool fel = (CoA::GameMode::Mode() >> 7) & 1;
        int32_t n = 0;

        switch (t) {
            case 0:
            case 1:
                n = fel ? ConfigInt("CONFIG_RANDOM_MODE_MAX_DEFAULT_FELFORGED_SKILL_CARDS", 3)
                        : ConfigInt("CONFIG_RANDOM_MODE_MAX_DEFAULT_NORMAL_SKILL_CARDS", 2);
                break;
            case 2:
            case 3:
                n = 2;
                break;
            case 4:
            case 5:
                n = fel ? ConfigInt("CONFIG_RANDOM_MODE_MAX_LUCKY_FELFORGED_SKILL_CARDS", 3)
                        : ConfigInt("CONFIG_RANDOM_MODE_MAX_LUCKY_NORMAL_SKILL_CARDS", 2);
                break;
            case 6:
            case 7:
                n = fel ? ConfigInt("CONFIG_RANDOM_MODE_MAX_TALENT_FELFORGED_SKILL_CARDS", 3)
                        : ConfigInt("CONFIG_RANDOM_MODE_MAX_TALENT_NORMAL_SKILL_CARDS", 2);
                break;
            default:
                n = 0;
                break;
        }

        lua_pushinteger(L, n);
        return 1;
    }

    void SendSet(const char* type, uint32_t slot, uint32_t card) {   // CMSG_SET_SKILL_CARD 0x622
        CoA::Packet(0x622).U32(Spec()).Str(type).U32(slot).U32(card).Send();
    }

    int SetCardAtIndex(lua_State* L) {   // FUN_102f79a0: (type, slot, card)
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER, LUA_TNUMBER })) {
            return 0;
        }

        const std::string type = CheckString(L, 1);
        const int32_t slot = ToInt(CheckNumber(L, 2));
        const int32_t card = ToInt(CheckNumber(L, 3));

        if (slot == 0) {
            return 0;
        }

        SendSet(type.c_str(), static_cast<uint32_t>(slot - 1), static_cast<uint32_t>(card));
        PushBool(L, true);
        return 1;
    }

    int RemoveCardAtIndex(lua_State* L) {   // FUN_102f77f0: (type, slot)
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
            return 0;
        }

        const std::string type = CheckString(L, 1);
        const int32_t slot = ToInt(CheckNumber(L, 2));

        if (slot == 0) {
            return 0;
        }

        SendSet(type.c_str(), static_cast<uint32_t>(slot - 1), 0);
        PushBool(L, true);
        return 1;
    }

    // (card [, rank]) with rank 1 by default -- FUN_100b88c0, else FUN_1008ba00.
    bool ReadCardRank(lua_State* L, uint32_t& card, uint32_t& rank) {
        int32_t a;
        int32_t b;

        if (CoA::Script::ReadInt2(L, a, b)) {
            card = static_cast<uint32_t>(a);
            rank = static_cast<uint32_t>(b);
            return true;
        }

        rank = 1;
        return CoA::Script::ReadNumber(L, card);
    }

    int GetSkillCardInfo(lua_State* L) {   // FUN_102f68f0
        uint32_t card;
        uint32_t rank;

        if (!ReadCardRank(L, card, rank)) {
            return 0;
        }

        const Card* c = CoA::SkillCard::ByCardRank(card, rank);

        if (!c) {
            return 0;
        }

        CoA::SkillCard::PushCard(L, *c);
        return 1;
    }

    int GetSkillCardQuality(lua_State* L) {   // FUN_102f6af0
        uint32_t card;
        uint32_t rank;

        if (!ReadCardRank(L, card, rank)) {
            return 0;
        }

        const Card* c = CoA::SkillCard::ByCardRank(card, rank);

        if (c) {
            PushStr(L, c->quality.c_str());
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // FUN_102f6e00 / FUN_102f7300: an unblocked slot of the active spec whose card matches. Returns (found, the slot's
    // type name -- SKILL_CARD_MAX when none, found).
    template <class Match>
    int FindCarded(lua_State* L, Match match) {
        const uint32_t spec = Spec();
        bool found = false;
        uint32_t type = 8;

        for (uint32_t t = 0; t < 8 && !found; ++t) {   // FUN_102f41f0: types 0..7
            for (uint32_t i = 0; i < Count(spec, t); ++i) {
                const Slot* s = SlotAt(spec, t, i);
                const Card* c = s ? CoA::SkillCard::ByCardRank(s->cardId, s->rank) : nullptr;

                if (Blocked(spec, t, i) || !c || !match(*c)) {
                    continue;
                }

                found = true;
                type = t;
                break;
            }
        }

        PushBool(L, found);
        const char* name = CoA::SkillCard::TypeName(type);
        PushStr(L, name ? name : "");
        PushBool(L, found);
        return 3;
    }

    int IsCardedID(lua_State* L) {   // an entry id (the CharacterAdvancement row listing the card's spell)
        uint32_t entry;

        if (!CoA::Script::ReadNumber(L, entry)) {
            return 0;
        }

        return FindCarded(L, [entry](const Card& c) {
            CoA::CA::Row r = CoA::CA::RowBySpell(c.spell);   // FUN_101498d0
            return r && CoA::Dbc::Table::U32(r, 0) == entry;
        });
    }

    int IsCardedSpellID(lua_State* L) {
        uint32_t spell;

        if (!CoA::Script::ReadNumber(L, spell)) {
            return 0;
        }

        return FindCarded(L, [spell](const Card& c) { return c.spell == spell; });
    }

    int GetCardID(lua_State* L) {   // handler_GetCardID: (item) -> card id, rank
        uint32_t item;

        if (!CoA::Script::ReadNumber(L, item)) {
            return 0;
        }

        const Card* c = CoA::SkillCard::ByItem(item);

        if (!c) {
            lua_pushnil(L);
            lua_pushnil(L);
            return 2;
        }

        lua_pushinteger(L, static_cast<int32_t>(c->cardId));
        lua_pushinteger(L, static_cast<int32_t>(c->rank));
        return 2;
    }

    int GetCardSpellID(lua_State* L) {   // FUN_102f5670: (card, rank)
        int32_t card;
        int32_t rank;

        if (!CoA::Script::ReadInt2(L, card, rank)) {
            return 0;
        }

        const Card* c = CoA::SkillCard::ByCardRank(static_cast<uint32_t>(card), static_cast<uint32_t>(rank));

        if (!c) {
            lua_pushnil(L);
        } else {
            lua_pushinteger(L, static_cast<int32_t>(c->spell));
        }

        return 1;
    }

    // ---- Hand of Fate / Scroll of Fortune quests ------------------------------------------------------------------
    void PushIntList(lua_State* L, const char* key, const uint8_t* r, uint32_t off, uint32_t n) {   // FUN_100b9d20 + array
        lua_pushstring(L, key);
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < n; ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_pushinteger(L, static_cast<int32_t>(CoA::Dbc::Table::U32(r, off + i * 4)));
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
    }

    void SetInt(lua_State* L, const char* k, uint32_t v) {
        lua_pushstring(L, k);
        lua_pushinteger(L, static_cast<int32_t>(v));
        lua_settable(L, -3);
    }

    // One array of every quest of each spec's group (DAT_10be3a64: 20 groups, first + spec), each {ID, MinLevel,
    // RequiredItemCount, RequiredItemId, RewardAmount, RewardItem, Specialization (0-based), Leveling: one of the
    // quest's groups (+8 / +0xC / +0x10) is a leveling group (DAT_10be3a4c: levelingFirst + 0..19)}.
    int PushQuestGroups(lua_State* L, uint32_t first, uint32_t levelingFirst) {
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        int32_t n = 0;

        for (uint32_t spec = 0; spec < 20; ++spec) {
            for (const uint8_t* r : CoA::BridgeStore::QuestsInGroup(first + spec)) {
                lua_pushinteger(L, ++n);
                lua_createtable(L, 0, 0);
                lua_checkstack(L, 2);
                SetInt(L, "ID", CoA::Dbc::Table::U32(r, 0));
                SetInt(L, "MinLevel", CoA::Dbc::Table::U32(r, 0x14));
                PushIntList(L, "RequiredItemCount", r, 0x50, 6);
                PushIntList(L, "RequiredItemId", r, 0x38, 6);
                PushIntList(L, "RewardAmount", r, 0x28, 4);
                PushIntList(L, "RewardItem", r, 0x18, 4);
                SetInt(L, "Specialization", spec);
                bool leveling = false;

                for (uint32_t off : { 8u, 12u, 16u }) {
                    const uint32_t g = CoA::Dbc::Table::U32(r, off);

                    if (g != 0 && g >= levelingFirst && g < levelingFirst + 20) {
                        leveling = true;
                        break;
                    }
                }

                lua_pushstring(L, "Leveling");
                PushBool(L, leveling);
                lua_settable(L, -3);
                lua_settable(L, -3);
            }
        }

        return 1;
    }

    // 0x102F56F0: groups 0x65 + spec (FUN_1007a0b0), leveling groups 0xC9.. (FUN_1007a2b0).
    int GetHandOfFateQuests(lua_State* L) {
        return PushQuestGroups(L, 0x65, 0xC9);
    }

    // 0x102F6360: groups 0x191 + spec (FUN_1007a1b0), leveling groups 0x1F5.. (FUN_1007a3b0).
    int GetScrollOfFortuneQuests(lua_State* L) {
        return PushQuestGroups(L, 0x191, 0x1F5);
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_SkillCard", "GetCardCount", GetCardCount),
        COA_NATIVE("C_SkillCard", "GetCardAtIndex", GetCardAtIndex),
        COA_NATIVE("C_SkillCard", "GetSkillCardInfoAtIndex", GetSkillCardInfoAtIndex),
        COA_NATIVE("C_SkillCard", "GetCardRankAtIndex", GetCardRankAtIndex),
        COA_NATIVE("C_SkillCard", "IsCardAtIndexActive", IsCardAtIndexActive),
        COA_NATIVE("C_SkillCard", "IsCardAtIndexBlocked", IsCardAtIndexBlocked),
        COA_NATIVE("C_SkillCard", "GetMaxCardCount", GetMaxCardCount),
        COA_NATIVE("C_SkillCard", "SetCardAtIndex", SetCardAtIndex),
        COA_NATIVE("C_SkillCard", "RemoveCardAtIndex", RemoveCardAtIndex),
        COA_NATIVE("C_SkillCard", "GetSkillCardInfo", GetSkillCardInfo),
        COA_NATIVE("C_SkillCard", "GetSkillCardQuality", GetSkillCardQuality),
        COA_NATIVE("C_SkillCard", "IsCardedID", IsCardedID),
        COA_NATIVE("C_SkillCard", "IsCardedSpellID", IsCardedSpellID),
        COA_NATIVE("C_SkillCard", "GetCardID", GetCardID),
        COA_NATIVE("C_SkillCard", "GetCardSpellID", GetCardSpellID),
        COA_NATIVE("", "GetHandOfFateQuests", GetHandOfFateQuests),
        COA_NATIVE("", "GetScrollOfFortuneQuests", GetScrollOfFortuneQuests),
    };
}

// FUN_102f4f70: any slotted, unblocked card in the spec (types 0..8).
bool CoA::SkillCard::AnyUnblocked(uint32_t spec) {
    for (uint32_t t = 0; t < kTypeCount; ++t) {
        for (uint32_t i = 0; i < Count(spec, t); ++i) {
            const Slot* s = SlotAt(spec, t, i);

            if (s && s->cardId != 0 && !Blocked(spec, t, i)) {
                return true;
            }
        }
    }

    return false;
}

int32_t CoA::SkillCardSlots::OnSetResult(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {   // 0x623
    const std::string r = CoA::Read::Str(msg);
    CoA::Events::Signal("SET_SKILL_CARD_RESULT", "%s", r.c_str());
    return 1;
}

// 0x624 (FUN_102f4ab0): u32 specs, each 9 types x {u32 n, n x {u32 card, u32 rank, u8 blocked}}.
int32_t CoA::SkillCardSlots::OnList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    s_specs.clear();
    uint32_t specs = CoA::Read::U32(msg);

    if (specs > kMaxIndex) {
        specs = 0;
    }

    s_specs.resize(specs);

    for (uint32_t s = 0; s < specs; ++s) {
        s_specs[s].resize(kTypeCount);

        for (uint32_t t = 0; t < kTypeCount; ++t) {
            const uint32_t n = CoA::Read::U32(msg);

            for (uint32_t i = 0; i < n && CoA::Read::Remaining(msg) >= kSlotSize; ++i) {
                Slot e;
                e.cardId = CoA::Read::U32(msg);
                e.rank = CoA::Read::U32(msg);
                e.blocked = CoA::Read::U8(msg);
                s_specs[s][t].push_back(e);
            }
        }
    }

    return 1;
}

// 0x625 (FUN_102f4660): u32 spec, str type, u32 slot, u32 card, u32 rank, u8 blocked.
int32_t CoA::SkillCardSlots::OnUpdate(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    const uint32_t spec = CoA::Read::U32(msg);
    const std::string typeName = CoA::Read::Str(msg);
    const uint32_t slot = CoA::Read::U32(msg);
    const uint32_t card = CoA::Read::U32(msg);
    const uint32_t rank = CoA::Read::U32(msg);
    const uint8_t blocked = CoA::Read::U8(msg);
    const int t = CoA::SkillCard::TypeIndex(typeName);

    if (t < 0 || spec >= kMaxIndex || slot >= kMaxIndex) {
        return 1;
    }

    if (s_specs.size() <= spec) {
        s_specs.resize(spec + 1);
    }

    if (s_specs[spec].size() <= static_cast<uint32_t>(t)) {
        s_specs[spec].resize(static_cast<uint32_t>(t) + 1);
    }

    std::vector<Slot>& v = s_specs[spec][static_cast<uint32_t>(t)];

    if (v.size() <= slot) {
        v.resize(slot + 1);
    }

    const Slot old = v[slot];
    v[slot] = { card, rank, blocked };

    if (old.cardId != card) {
        CoA::Events::Signal("SKILL_CARD_UPDATED", "%u%s%u%u%u%u%u", spec + 1, typeName.c_str(), slot + 1, old.cardId, old.rank, card, rank);
    }

    if (old.blocked != blocked) {
        CoA::Events::Signal(blocked ? "SKILL_CARD_BLOCKED" : "SKILL_CARD_UNBLOCKED", "%u%s%u", spec + 1, typeName.c_str(), slot + 1);
    }

    return 1;
}

void CoA::SkillCardSlots::OnGlueScreen() {   // 0x102F41C0
    s_specs.clear();
}

void CoA::SkillCardSlots::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_PACKET(0x623, CoA::SkillCardSlots::OnSetResult);
    COA_PACKET(0x624, CoA::SkillCardSlots::OnList);
    COA_PACKET(0x625, CoA::SkillCardSlots::OnUpdate);
    CoA::Lifecycle::OnGlueScreen(CoA::SkillCardSlots::OnGlueScreen);
    COA_EVENT("SET_SKILL_CARD_RESULT");
    COA_EVENT("SKILL_CARD_UPDATED");
    COA_EVENT("SKILL_CARD_BLOCKED");
    COA_EVENT("SKILL_CARD_UNBLOCKED");
}

void CoA::SkillCardSlots::Reset() {
    s_specs.clear();
}
