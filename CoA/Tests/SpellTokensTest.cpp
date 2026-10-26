#include "CoA/Source/Modules/AttachFixes.h"
#include "CoA/Source/Modules/AuraAmounts.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Modules/SpellTokens.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
    using CoA::SpellTokens::Equipped;
    using CoA::SpellTokens::ItemSet;
    using CoA::SpellTokens::Spell;

    bool s_player = true;
    std::map<uint32_t, uint32_t> s_desc;
    std::map<uint32_t, float> s_descF;
    std::vector<Equipped> s_equipped;
    std::map<uint32_t, ItemSet> s_sets;
    std::map<std::string, std::string> s_globals;
    int32_t s_lastModifyOp = -1;

    bool HasPlayer() { return s_player; }
    uint32_t PlayerU32(uint32_t off) { return s_desc.count(off) ? s_desc[off] : 0; }
    float PlayerF32(uint32_t off) { return s_descF.count(off) ? s_descF[off] : 0.0f; }
    int32_t Modify(const void*, int32_t value, int32_t op) {
        s_lastModifyOp = op;
        return value + 100;
    }
    std::vector<Equipped> EquippedItems() { return s_equipped; }
    bool Set(uint32_t id, ItemSet& out) {
        auto it = s_sets.find(id);

        if (it == s_sets.end()) {
            return false;
        }

        out = it->second;
        return true;
    }
    const char* Global(const char* key) {
        auto it = s_globals.find(key);
        return it == s_globals.end() ? nullptr : it->second.c_str();
    }

    void Start() {
        CoA::SpellTokens::Reset();
        CoA::SpellTokens::Deps& d = CoA::SpellTokens::Dependencies();
        d.hasPlayer = HasPlayer;
        d.playerU32 = PlayerU32;
        d.playerF32 = PlayerF32;
        d.modifyValue = Modify;
        d.equipped = EquippedItems;
        d.itemSet = Set;
        d.globalText = Global;
        CoA::SpellTokens::Init();
        s_player = true;
        s_desc.clear();
        s_descF.clear();
        s_equipped.clear();
        s_sets.clear();
        s_globals.clear();
        s_lastModifyOp = -1;
    }

    float V(uint32_t op, const Spell& spell, bool* handled = nullptr) {
        float out = -1.0f;
        const bool h = CoA::SpellTokens::Value(op, spell, &spell, out);

        if (handled) {
            *handled = h;
        }

        return out;
    }
}

COA_TEST(SpellTokens_VariableTable) {
    const char* want[] = {
        "w1", "w2", "w3", "W1", "W2", "W3", "ppl1", "ppl2", "ppl3", "PPL1", "PPL2", "PPL3", "ppl1", "ppl2", "ppl3",
        "power1", "power2", "power3", "power4", "power5", "power6", "power7",
        "POWER1", "POWER2", "POWER3", "POWER4", "POWER5", "POWER6", "POWER7",
        "br", "BR", "r1", "r2", "r3", "health", "HEALTH",
    };
    static_assert(sizeof(want) / sizeof(want[0]) == CoA::SpellTokens::kVariableCount - CoA::SpellTokens::kClientVariables, "0x24 names");

    for (uint32_t i = 0; i < sizeof(want) / sizeof(want[0]); ++i) {
        CHECK(strcmp(CoA::SpellTokens::ExtraVariable(i), want[i]) == 0);
    }

    CHECK(CoA::SpellTokens::ExtraVariable(0x24) == nullptr);
    CHECK(strcmp(CoA::SpellTokens::ConditionLetters(), "sSaApP") == 0);
}

COA_TEST(SpellTokens_OpsTheClientKeeps) {
    Start();
    Spell spell;
    bool handled = true;
    V(0x16, spell, &handled);   // STR
    CHECK(!handled);
    V(0x54, spell, &handled);
    CHECK(!handled);
    V(0xC3, spell, &handled);   // r3, health, HEALTH: ops past the evaluator's cases
    CHECK(!handled);
    V(0xC5, spell, &handled);
    CHECK(!handled);
}

COA_TEST(SpellTokens_SpellAndPlayerValues) {
    Start();
    Spell spell;
    spell.id = 500;
    spell.maxTargets = 3;
    spell.baseLevel = 10;
    spell.maxLevel = 0;
    spell.realPointsPerLevel[0] = 1.5f;
    spell.realPointsPerLevel[1] = 2.0f;
    spell.realPointsPerLevel[2] = 0.5f;
    spell.miscValueB[0] = -4;
    spell.miscValueB[1] = 5;
    spell.miscValueB[2] = 6;
    s_desc[0xD8] = 30;   // level

    // i / I: max targets through spell modifier 0x22.
    CHECK(V(0x52, spell) == 103.0f && s_lastModifyOp == 0x22);
    CHECK(V(0x53, spell) == 103.0f);

    // ppl / PPL: (level - base) times the per-level value, the level capped at the max level.
    CHECK(V(0xA8, spell) == 30.0f);
    CHECK(V(0xAB, spell) == 30.0f);
    CHECK(V(0xA9, spell) == 40.0f);
    CHECK(V(0xAD, spell) == 10.0f);
    spell.maxLevel = 25;
    CHECK(V(0xA8, spell) == 22.5f);
    spell.baseLevel = 31;
    bool handled = false;
    CHECK(V(0xA8, spell, &handled) == 0.0f && handled);

    // power / POWER (shifted by the duplicate names, as shipped): descriptor dwords.
    s_desc[0x64] = 77;          // 0xB1 * 4 - 0x260: UNIT_FIELD_POWER1
    s_desc[0x78] = 0xFFFFFFFF;  // 0xB5 * 4 - 0x25C, unsigned
    s_desc[0x90] = 9;           // 0xBB
    CHECK(V(0xB1, spell) == 77.0f);
    CHECK(V(0xB5, spell) == 4294967295.0f);
    CHECK(V(0xBB, spell) == 9.0f);
    s_descF[0x1000] = 12.5f;
    CHECK(V(0xBC, spell) == 12.5f && V(0xBD, spell) == 12.5f);
    CHECK(V(0xBE, spell) == -4.0f && V(0xBF, spell) == 5.0f && V(0xC0, spell) == 6.0f);
    s_desc[0x60] = 1000;
    s_desc[0x80] = 2000;
    CHECK(V(0xC1, spell) == 1000.0f && V(0xC2, spell) == 2000.0f);

    // Without a player the player values are 0 (still pushed); the spell's own are not affected.
    s_player = false;
    CHECK(V(0xB1, spell, &handled) == 0.0f && handled);
    CHECK(V(0xA9, spell) == 0.0f && V(0xBC, spell) == 0.0f && V(0xC1, spell) == 0.0f);
    CHECK(V(0xBF, spell) == 5.0f);
}

COA_TEST(SpellTokens_AuraAmountTokens) {
    Start();
    CoA::LuaReg::Reset();
    CoA::Packets::Clear();
    CoA::Lifecycle::Reset();
    CoA::AuraAmounts::Init();
    CoA::AuraAmounts::Deps& a = CoA::AuraAmounts::Dependencies();
    a.spellEffects = [](uint32_t, CoA::AuraAmounts::SpellEffects& out) { out = {}; out.aura[0] = 3; return true; };
    a.acceptsSpell = [](uint32_t, uint64_t) { return true; };
    a.tokens = [](uint64_t) { return std::vector<std::string>(); };
    a.activePlayer = [] { return uint64_t(0x11); };
    CoA::AuraAmounts::Forget(0x33);
    CoATest::Bytes b;
    b.U64(0x33).U32(1).U64(0x99).U32(700).U32(11).U32(22).U32(33);
    CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
    int32_t result = 0;
    CoA::Packets::Dispatch(0x673, 0, &msg, result);

    Spell spell;
    spell.id = 700;
    CoA::SpellTokens::SetAuraContext(0x33);
    CHECK(V(0xA2, spell) == 11.0f && V(0xA5, spell) == 11.0f);
    CHECK(V(0xA3, spell) == 22.0f && V(0xA6, spell) == 22.0f);
    CHECK(V(0xA4, spell) == 33.0f && V(0xA7, spell) == 33.0f);
    spell.id = 701;
    bool handled = false;
    CHECK(V(0xA2, spell, &handled) == 0.0f && handled);
    CoA::SpellTokens::SetAuraContext(0x34);
    spell.id = 700;
    CHECK(V(0xA4, spell) == 0.0f);
    CoA::AuraAmounts::Forget(0x33);
}

COA_TEST(SpellTokens_Conditions) {
    Start();
    using CoA::SpellTokens::Condition;
    CHECK(Condition("C12") == 0);
    CHECK(Condition("c") == 0);
    CHECK(Condition("s100") == -1);
    CHECK(Condition("a100") == -1);
    CHECK(Condition("(p1)") == -1);

    // An equipped item's on-equip spell (trigger 1); another trigger does not count.
    Equipped ring;
    ring.entry = 40;
    ring.hasProto = true;
    ring.spells[0] = 100;
    ring.triggers[0] = 0;
    ring.spells[1] = 101;
    ring.triggers[1] = 1;
    s_equipped.push_back(ring);
    CHECK(Condition("p101") == 1);
    CHECK(Condition("P101]") == 1);
    CHECK(Condition("p100") == 0);
    CHECK(Condition("p") == 0);

    // The spell list stops at the first zero.
    s_equipped[0].spells[0] = 0;
    CHECK(Condition("p101") == 0);

    // Item-set bonuses: enough pieces equipped (any slot), the list stops at the first zero spell.
    Equipped a;
    a.entry = 50;
    a.hasProto = true;
    a.itemSet = 9;
    Equipped bItem;
    bItem.entry = 51;
    bItem.hasProto = false;   // no record: its set is not considered from it, but it counts as a piece
    s_equipped = { a, bItem };
    ItemSet set;
    set.items[0] = 50;
    set.items[1] = 51;
    set.items[2] = 52;
    set.spells[0] = 300;
    set.thresholds[0] = 2;
    set.spells[1] = 301;
    set.thresholds[1] = 3;
    s_sets[9] = set;
    CHECK(Condition("p300") == 1);
    CHECK(Condition("p301") == 0);

    // Without a player: false (the original dereferences it).
    s_player = false;
    CHECK(Condition("p300") == 0);
}

COA_TEST(SpellTokens_SchoolNames) {
    Start();
    using CoA::SpellTokens::SchoolName;
    CHECK(SchoolName(0).empty());
    CHECK(SchoolName(4) == "Fire");
    CHECK(SchoolName(0x7F) == "Chaos");
    CHECK(SchoolName(7) == "Unknown");
    s_globals["STRING_SCHOOL_FIRE"] = "Feu";
    CHECK(SchoolName(4) == "Feu");

    char text[64];
    strcpy(text, "Deals $sc damage, $sc.");
    CoA::SpellTokens::ReplaceSchool(text, sizeof(text), 0x20);
    CHECK(strcmp(text, "Deals Shadow damage, Shadow.") == 0);
    strcpy(text, "no token");
    CoA::SpellTokens::ReplaceSchool(text, sizeof(text), 0x20);
    CHECK(strcmp(text, "no token") == 0);
    // Truncated to the buffer.
    char small[12];
    strcpy(small, "a $sc b");
    CoA::SpellTokens::ReplaceSchool(small, sizeof(small), 0x40);
    CHECK(strcmp(small, "a Arcane b") == 0);
    strcpy(small, "$sc $sc");
    CoA::SpellTokens::ReplaceSchool(small, sizeof(small), 0x40);
    CHECK(strcmp(small, "Arcane Arca") == 0);
    // A mask of 0 names nothing: the token is removed.
    strcpy(text, "x$scy");
    CoA::SpellTokens::ReplaceSchool(text, sizeof(text), 0);
    CHECK(strcmp(text, "xy") == 0);
    CoA::SpellTokens::ReplaceSchool(nullptr, 0, 4);
}

COA_TEST(SpellTokens_HidesCastTime) {
    Start();
    CoA::AttachFixes::Deps& d = CoA::AttachFixes::Dependencies();
    auto saved = d.spellCustomAttr;
    d.spellCustomAttr = [](uint32_t spell, uint32_t& a10, uint32_t& a14, uint32_t& a18) {
        a10 = 0;
        a18 = 0;
        a14 = spell == 1 ? 0x40 : spell == 2 ? 0xBF : 0;
        return spell == 1 || spell == 2;
    };
    CHECK(CoA::SpellTokens::HidesCastTime(1));
    CHECK(!CoA::SpellTokens::HidesCastTime(2));
    CHECK(!CoA::SpellTokens::HidesCastTime(3));
    d.spellCustomAttr = saved;
}
