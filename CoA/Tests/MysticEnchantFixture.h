#ifndef COA_TESTS_MYSTICENCHANTFIXTURE_H
#define COA_TESTS_MYSTICENCHANTFIXTURE_H

// The shared setup of the mystic enchant tests: a MysticEnchant.dbc with three rows, a realm with the live gate, and a
// fake client behind CoA::MysticEnchant::Deps.

#include "CoA/Source/Modules/MysticEnchant.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace MysticFixture {
    using V = std::vector<std::string>;

    const uint64_t kPlayer = 0x11;
    const uint64_t kTarget = 0x22;
    const uint32_t kScroll = 0xF25D0;
    const uint32_t kMark = 0x5B9D2;
    const uint32_t kExtract = 0x1809F;

    // String block offsets (CoATest::Wdbc puts "" at 0).
    const uint32_t kRare = 1, kUncommon = 17, kEpic = 37, kFire = 53, kNone = 58;

    inline uint8_t s_class = 12;
    inline uint32_t s_level = 60;
    inline uint32_t s_money = 0;
    inline bool s_casting = false;
    inline bool s_inWorld = true;
    inline bool s_altar = true;
    inline bool s_hasPlayer = true;
    inline int32_t s_auraPoints = -1;
    inline uint32_t s_investment = 0;
    inline uint32_t s_free = 5;
    inline uint32_t s_scrollPrice = 1000;
    inline uint8_t s_targetClass = 3;
    inline std::set<uint32_t> s_patchedSpells;   // patched rows whose spell has a record in this test
    inline std::vector<CoA::MysticEnchant::Item> s_items;
    inline std::map<uint32_t, uint32_t> s_tokens;

    // A 0x7C-byte row: id, spell, +8, quality, reborn quality, level, item, worldforged, five realm flags, class mask,
    // four tags, three classes, three tabs, three AE, three TE.
    inline std::vector<uint32_t> Row(uint32_t id, uint32_t spell, uint32_t quality, uint32_t reborn, uint32_t level, uint32_t item,
        uint32_t worldforged, uint32_t realm) {
        std::vector<uint32_t> r(31, 0);
        r[0] = id;
        r[1] = spell;
        r[3] = quality;
        r[4] = reborn;
        r[5] = level;
        r[6] = item;
        r[7] = worldforged;
        r[8] = realm;
        r[13] = 0xFFFFFFFF;
        r[19] = r[20] = r[21] = 1;
        r[22] = r[23] = r[24] = 1;
        return r;
    }

    inline bool ReadDbc(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "MysticEnchant.dbc") == nullptr) {
            return false;
        }

        std::vector<uint32_t> one = Row(1, 1001, kRare, kUncommon, 10, 5001, 0, 1);
        one[15] = kFire;
        one[16] = kNone;
        std::vector<uint32_t> two = Row(2, 1002, kEpic, kEpic, 60, 5002, 1, 1);
        two[13] = 1u << 2;   // warriors only
        two[19] = 3;
        two[20] = 3;
        two[22] = 7;
        two[23] = 6;
        two[25] = 5;
        two[29] = 2;
        std::vector<uint32_t> three = Row(3, 1003, kUncommon, kUncommon, 1, 5003, 0, 0);
        const std::string strings = std::string("RE_QUALITY_RARE") + '\0' + "RE_QUALITY_UNCOMMON" + '\0' + "RE_QUALITY_EPIC" + '\0' +
            "FIRE" + '\0' + "NONE";
        out = CoATest::Wdbc({ one, two, three }, strings);
        return true;
    }

    inline bool SpellStacks(uint32_t spell, uint32_t& stacks) {
        if (spell == 1001 || s_patchedSpells.count(spell) != 0) {
            stacks = 1;
            return true;
        }

        if (spell == 1002) {
            stacks = 2;
            return true;
        }

        return false;
    }

    inline void Realm(uint8_t live) {
        CoATest::Bytes b;
        b.U32(1).U32(0).F32(0).F32(0).F32(0).U32(0).F32(0).F32(0).U32(0);
        b.U8(live);

        for (int i = 1; i < 8; ++i) {
            b.U8(0);
        }

        b.Str("").Str("Realm").U8(0);
        CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    }

    inline lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::Dbc::SetFileReader(ReadDbc);
        Realm(1);
        s_class = 12;
        s_level = 60;
        s_money = 0;
        s_casting = false;
        s_inWorld = true;
        s_altar = true;
        s_hasPlayer = true;
        s_auraPoints = -1;
        s_investment = 0;
        s_free = 5;
        s_scrollPrice = 1000;
        s_targetClass = 3;
        s_patchedSpells.clear();
        s_items.clear();
        s_tokens.clear();
        CoA::ClientState::SetInWorldProbe([] { return s_inWorld; });
        CoA::MysticEnchant::Reset();
        CoA::MysticEnchant::Init();

        CoA::MysticEnchant::Deps& d = CoA::MysticEnchant::Dependencies();
        d.hasPlayer = [] { return s_hasPlayer; };
        d.playerClass = [] { return s_class; };
        d.playerLevel = [] { return s_level; };
        d.coinage = [] { return s_money; };
        d.casting = [] { return s_casting; };
        d.item = [](uint32_t low, CoA::MysticEnchant::Item& out) {
            for (const auto& i : s_items) {
                if (i.low == low) {
                    out = i;
                    return true;
                }
            }

            return false;
        };
        d.playerItems = [] { return s_items; };
        d.tokenCount = [](uint32_t entry) {
            auto it = s_tokens.find(entry);
            return it == s_tokens.end() ? 0u : it->second;
        };
        d.freeBagSlots = [] { return s_free; };
        d.itemName = [](uint32_t low) { return "Item" + std::to_string(low); };
        d.itemBuyPrice = [](uint32_t entry, uint32_t& price) {
            price = s_scrollPrice;
            return entry == kScroll && s_scrollPrice != 0;
        };
        d.spellStacks = SpellStacks;
        d.spellName = [](uint32_t spell, std::string& name) {
            uint32_t n;
            name = "Spell" + std::to_string(spell);
            return SpellStacks(spell, n);
        };
        d.auraPoints = [](uint32_t, int32_t& points) {
            points = s_auraPoints;
            return s_auraPoints >= 0;
        };
        d.investment = [](uint32_t, uint32_t, bool) { return s_investment; };
        d.tokenGuid = [](const char* token, uint64_t& guid) {
            guid = strcmp(token, "player") == 0 ? kPlayer : strcmp(token, "target") == 0 ? kTarget : 0;
            return guid != 0;
        };
        d.activePlayerGuid = [] { return kPlayer; };
        d.playerClassByGuid = [](uint64_t guid, uint8_t& cls) {
            cls = guid == kPlayer ? s_class : s_targetClass;
            return guid == kPlayer || guid == kTarget;
        };
        d.altarNearby = [] { return s_altar; };
        d.spellTextName = [](uint32_t spell) { return "Spell" + std::to_string(spell); };
        d.spellTextDescription = [](uint32_t) { return std::string(); };
        d.cacheItem = [](uint32_t) {};
        return CoATest::NewState(false);
    }

    inline void Teardown(lua_State* L) {
        lua_close(L);
        CoA::ClientState::SetInWorldProbe(nullptr);
        CoA::Dbc::SetFileReader(nullptr);
    }

    inline V Feed(uint32_t opcode, CoATest::Bytes bytes) {
        CoATest::RecordSignals();
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    inline V Run(lua_State* L, const char* chunk) {
        V out;
        const int base = lua_gettop(L);

        if (luaL_loadstring(L, chunk) != 0 || lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
            out.push_back(std::string("<error> ") + lua_tostring(L, -1));
            lua_settop(L, base);
            return out;
        }

        for (int i = base + 1; i <= lua_gettop(L); ++i) {
            out.push_back(lua_isnil(L, i) ? "nil" : lua_isboolean(L, i) ? (lua_toboolean(L, i) ? "true" : "false") : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }

    inline std::vector<std::vector<uint8_t>> Sent(lua_State* L, const char* chunk, V* results = nullptr) {
        CoATest::RecordPackets();
        V r = Run(L, chunk);
        CoATest::StopRecordingPackets();

        if (results) {
            *results = r;
        }

        return CoATest::SentPackets();
    }
}

#endif
