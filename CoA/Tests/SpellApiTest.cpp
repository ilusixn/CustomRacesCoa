#include "CoA/Source/Modules/SpellApi.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <map>

namespace {
    using V = std::vector<std::string>;
    using CoA::SpellApi::SpellFields;

    std::map<uint32_t, SpellFields> s_spells;
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> s_sla;   // id -> {spell, skill line}
    std::map<uint32_t, uint32_t> s_lineCategory;
    bool s_player = true;

    SpellFields& Add(uint32_t id) {
        SpellFields& s = s_spells[id];
        s = SpellFields();
        s.id = id;
        return s;
    }

    // Effect i applies aura `aura` to the caster (implicit target 1).
    SpellFields& Aura(uint32_t id, uint32_t aura, uint32_t trigger = 0) {
        SpellFields& s = Add(id);
        s.effect[0] = 6;
        s.aura[0] = aura;
        s.targetA[0] = 1;
        s.triggerSpell[0] = trigger;
        return s;
    }

    const std::vector<uint32_t>& Chain(uint32_t first) {
        static const std::vector<uint32_t> chain = { 10, 11, 12 };
        static const std::vector<uint32_t> none;
        return first == 10 ? chain : none;
    }

    bool ReadTrainer(const char* path, std::vector<uint8_t>& out) {
        if (!strstr(path, "NPCTrainer.dbc")) {
            return false;
        }

        out = CoATest::Wdbc({ { 1, 777, 0, 0, 0, 0, 0 } }, std::string(1, '\0'));
        return true;
    }

    lua_State* Start() {
        CoA::LuaReg::Reset();
        CoA::SpellApi::Reset();
        s_spells.clear();
        s_sla.clear();
        s_lineCategory.clear();
        s_player = true;
        CoA::Dbc::SetFileReader(ReadTrainer);
        CoA::SpellApi::Deps& d = CoA::SpellApi::Dependencies();
        d.spell = [](uint32_t id, SpellFields& out) {
            auto it = s_spells.find(id);

            if (it == s_spells.end()) {
                return false;
            }

            out = it->second;
            return true;
        };
        d.firstRank = [](uint32_t spell) { return spell >= 10 && spell <= 12 ? 10u : spell; };
        d.chain = Chain;
        d.hasPlayer = [] { return s_player; };
        d.skillLineAbilityRange = [](uint32_t& minId, uint32_t& maxId) {
            minId = 1;
            maxId = 5;
        };
        d.skillLineAbility = [](uint32_t id, uint32_t& spell, uint32_t& line) {
            auto it = s_sla.find(id);

            if (it == s_sla.end()) {
                return false;
            }

            spell = it->second.first;
            line = it->second.second;
            return true;
        };
        d.skillLineCategory = [](uint32_t line, uint32_t& category) {
            auto it = s_lineCategory.find(line);
            category = it == s_lineCategory.end() ? 0 : it->second;
            return it != s_lineCategory.end();
        };
        CoA::SpellApi::Init();
        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        CoA::Dbc::SetFileReader(nullptr);
    }

    V Call(lua_State* L, const char* name, uint32_t spell) {
        return CoATest::Call(L, "C_Spell", name, { "n:" + std::to_string(spell) });
    }
}

// A friendly aura is a buff and helpful; a stun is a debuff and harmful; attribute 0x4000000 is never positive.
COA_TEST(SpellApi_BuffAndDebuff) {
    lua_State* L = Start();
    Aura(1, 8);
    Aura(2, 0x1A);
    Aura(3, 8).attributes = 0x4000000;
    CHECK((Call(L, "IsHelpfulSpell", 1) == V{ "true" }));
    CHECK((Call(L, "IsHarmfulSpell", 1) == V{ "false" }));
    CHECK((Call(L, "AppliesBuff", 1) == V{ "true" }));
    CHECK((Call(L, "AppliesDebuff", 1) == V{ "false" }));
    CHECK((Call(L, "IsHarmfulSpell", 2) == V{ "true" }));
    CHECK((Call(L, "AppliesDebuff", 2) == V{ "true" }));
    CHECK((Call(L, "IsHelpfulSpell", 3) == V{ "false" }));
    CHECK((Call(L, "IsHelpfulSpell", 999) == V{ "false" }));
    CHECK((Call(L, "IsHarmfulSpell", 999) == V{ "false" }));
    CHECK((CoATest::Call(L, "C_Spell", "IsHelpfulSpell", { "s:x" }) == V{}));
    Stop(L);
}

// An enemy-targeted effect is not positive; a periodic trigger (aura 0x17) asks its trigger spell one level deep,
// and a spell triggering itself does not recurse.
COA_TEST(SpellApi_TargetsAndTriggers) {
    lua_State* L = Start();
    SpellFields& bolt = Add(4);
    bolt.effect[0] = 2;
    bolt.targetA[0] = 6;
    CHECK((Call(L, "IsHarmfulSpell", 4) == V{ "true" }));
    Aura(9, 0x1A);                           // a stun on the caster: friendly-targeted and negative
    Aura(5, 0x17, 9);
    CHECK((Call(L, "IsHelpfulSpell", 5) == V{ "true" }));             // AllPositive asks nested: no trigger lookup
    CHECK(!CoA::SpellApi::IsPositiveEffect(s_spells[5], 0, false));   // unnested: the trigger spell decides
    Aura(15, 0x17, 4);                       // a trigger hitting an enemy does not count against it
    CHECK(CoA::SpellApi::IsPositiveEffect(s_spells[15], 0, false));
    SpellFields& self = Add(6);
    self.effect[0] = 0x40;                   // trigger spell, no aura
    self.targetA[0] = 1;
    self.triggerSpell[0] = 6;
    CHECK(CoA::SpellApi::IsPositiveEffect(self, 0, false));
    Stop(L);
}

// The family lists and the aura amount rules.
COA_TEST(SpellApi_FamilyAndAmounts) {
    lua_State* L = Start();
    Add(0xD634);                              // family 0 negative list
    CHECK((Call(L, "IsHelpfulSpell", 0xD634) == V{ "false" }));
    SpellFields& dmgDown = Aura(7, 0x1D);     // a stat modifier with a negative amount
    dmgDown.basePoints[0] = static_cast<uint32_t>(-10);
    dmgDown.dieSides[0] = 1;
    CHECK((Call(L, "IsHelpfulSpell", 7) == V{ "false" }));
    dmgDown.basePoints[0] = 10;
    CHECK((Call(L, "IsHelpfulSpell", 7) == V{ "true" }));
    SpellFields& shield = Add(8);
    shield.mechanic = 0x1D;                   // invulnerability
    shield.effect[0] = 2;
    shield.targetA[0] = 6;
    CHECK((Call(L, "IsHelpfulSpell", 8) == V{ "true" }));
    Stop(L);
}

COA_TEST(SpellApi_ImmunityTrainerFirstRank) {
    lua_State* L = Start();
    CHECK((Call(L, "IsImmunitySpell", 0x282) == V{ "true" }));
    CHECK((Call(L, "IsImmunitySpell", 1) == V{ "false" }));
    CHECK((Call(L, "IsTrainerSpell", 777) == V{ "true" }));
    CHECK((Call(L, "IsTrainerSpell", 778) == V{ "false" }));
    CHECK((Call(L, "GetFirstRank", 12) == V{ "10" }));
    Stop(L);
}

// The highest rank the level reaches; a rank other than the spell itself only for a class skill (category 7).
COA_TEST(SpellApi_MaxLearnableRank) {
    lua_State* L = Start();
    Add(10).spellLevel = 1;
    Add(11).spellLevel = 20;
    Add(12).spellLevel = 40;
    s_sla[1] = { 10, 50 };
    s_lineCategory[50] = 7;
    CHECK((CoATest::Call(L, "C_Spell", "GetMaxLearnableRank", { "n:10", "n:25" }) == V{ "11" }));
    CHECK((CoATest::Call(L, "C_Spell", "GetMaxLearnableRank", { "n:10", "n:5" }) == V{ "10" }));
    CHECK((CoATest::Call(L, "C_Spell", "GetMaxLearnableRank", { "n:10", "n:0" }) == V{ "nil" }));
    CoA::SpellApi::SkillLineAbilityErase(10);
    CHECK((CoATest::Call(L, "C_Spell", "GetMaxLearnableRank", { "n:10", "n:25" }) == V{ "nil" }));
    CoA::SpellApi::SkillLineAbilityUpdate(10);
    CHECK((CoATest::Call(L, "C_Spell", "GetMaxLearnableRank", { "n:10", "n:25" }) == V{ "11" }));
    s_lineCategory[50] = 6;
    CHECK((CoATest::Call(L, "C_Spell", "GetMaxLearnableRank", { "n:10", "n:25" }) == V{ "nil" }));
    s_player = false;
    CHECK((CoATest::Call(L, "C_Spell", "GetMaxLearnableRank", { "n:10", "n:25" }) == V{ "nil" }));
    Stop(L);
}
