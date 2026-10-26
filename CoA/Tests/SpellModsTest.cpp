#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Modules/SpellMods.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
    using CoA::SpellMods::Spell;
    std::map<uint32_t, Spell> s_spells;
    std::map<uint32_t, std::vector<std::array<uint32_t, 3>>> s_affects;
    std::map<uint32_t, uint32_t> s_attr18;
    std::vector<std::vector<uint32_t>> s_cooldowns;
    std::vector<std::string> s_calls;
    // (unit, aura type, spell) triples AffectedByAura answers true for; spells HasAura answers true for.
    std::vector<std::array<uint32_t, 3>> s_affected;
    std::vector<uint32_t> s_hasAura;
    std::vector<CoA::SpellMods::Aura> s_playerAuras;
    std::map<uint32_t, std::vector<uint8_t>> s_addons;
    int32_t s_amounts[3] = {};
    bool s_haveAmounts = false;
    bool s_activePlayer = true;
    uint32_t s_slots[20] = {};
    bool s_manastorm = false;
    uint32_t s_autoRepeat = 0;
    uint32_t s_basePower = 0;
    int32_t s_castTime = 0;
    bool s_errorSpeech = true;
    uint32_t s_castingSpell = 0;
    std::map<uint32_t, uint32_t> s_rangeFlags;
    bool s_modalSkip = false;
    std::string s_fatal;
    void* const kUnit = reinterpret_cast<void*>(0x1000);
    void* const kPlayer = reinterpret_cast<void*>(0x2000);
    constexpr uint64_t kUnitGuid = 0x51;
    constexpr uint64_t kPlayerGuid = 0x52;

    uint32_t UnitId(void* unit) {
        return unit == kUnit ? 1 : unit == kPlayer ? 2 : 0;
    }

    void SetAddonFloat(uint64_t guid, uint32_t index, float value) {
        uint32_t bits;
        memcpy(&bits, &value, 4);
        CoA::ObjectAddon::Unit(guid)[index].value = bits;
    }

    void Start() {
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::ObjectAddon::Reset();
        CoA::SpellMods::Reset();
        CoA::SpellMods::Init();
        s_spells.clear();
        s_affects.clear();
        s_attr18.clear();
        s_cooldowns.clear();
        s_calls.clear();
        s_affected.clear();
        s_hasAura.clear();
        s_playerAuras.clear();
        s_addons.clear();
        s_haveAmounts = false;
        s_activePlayer = true;
        memset(s_slots, 0, sizeof(s_slots));
        s_manastorm = false;
        s_autoRepeat = 0;
        s_basePower = 0;
        s_castTime = 0;
        s_errorSpeech = true;
        s_castingSpell = 0;
        s_rangeFlags.clear();
        s_modalSkip = false;
        s_fatal.clear();

        CoA::SpellMods::Deps& d = CoA::SpellMods::Dependencies();
        d.fetchSpell = [](uint32_t id, Spell& out) {
            auto it = s_spells.find(id);
            if (it == s_spells.end()) {
                return false;
            }
            out = it->second;
            return true;
        };
        d.spellAffects = [](uint32_t spell) { return s_affects[spell]; };
        d.rankChain = [](uint32_t first) { return first == 700 ? std::vector<uint32_t>{ 700, 701, 702 } : std::vector<uint32_t>{}; };
        d.customAttr18 = [](uint32_t spell, uint32_t& value) {
            auto it = s_attr18.find(spell);
            value = it != s_attr18.end() ? it->second : 0;
            return it != s_attr18.end();
        };
        d.setCooldownLeft = [](uint32_t a, uint32_t b, uint32_t c, uint32_t e, uint32_t f, uint32_t g, uint32_t h) {
            s_cooldowns.push_back({ a, b, c, e, f, g, h });
        };
        d.affectedByAura = [](void* unit, uint32_t type, uint32_t spell) {
            for (const auto& t : s_affected) {
                if (t[0] == UnitId(unit) && t[1] == type && t[2] == spell) {
                    return true;
                }
            }
            return false;
        };
        d.hasAura = [](void*, uint32_t spell) {
            for (uint32_t s : s_hasAura) {
                if (s == spell) {
                    return true;
                }
            }
            return false;
        };
        d.isActivePlayer = [](void*) { return s_activePlayer; };
        d.invSlotLow = [](void*, uint32_t slot) { return slot < 20 ? s_slots[slot] : 0u; };
        d.inManastorm = [] { return s_manastorm; };
        d.autoRepeatingSpell = [] { return s_autoRepeat; };
        d.basePower = [](void*, uint32_t) { return s_basePower; };
        d.unitGuid = [](void* unit) { return unit == kUnit ? kUnitGuid : unit == kPlayer ? kPlayerGuid : 0ull; };
        d.activePlayer = [] { return kPlayer; };
        d.auraCount = [](void*) { return static_cast<uint32_t>(s_playerAuras.size()); };
        d.auraAt = [](void*, uint32_t i, CoA::SpellMods::Aura& out) {
            if (i >= s_playerAuras.size()) {
                return false;
            }
            out = s_playerAuras[i];
            return true;
        };
        d.auraIndex = [](void*, uint32_t spell) {
            for (uint32_t i = 0; i < s_playerAuras.size(); ++i) {
                if (s_playerAuras[i].spell == spell) {
                    return i;
                }
            }
            return 0xFFFFFFFFu;
        };
        d.auraAmounts = [](uint64_t, uint64_t, uint32_t) { return s_haveAmounts ? static_cast<const int32_t*>(s_amounts) : nullptr; };
        d.spellAddon = [](uint32_t spell) {
            auto it = s_addons.find(spell);
            return it != s_addons.end() ? static_cast<const uint8_t*>(it->second.data()) : nullptr;
        };
        d.castTime = [](const void*) { return s_castTime; };
        d.errorSpeech = [] { return s_errorSpeech; };
        d.castingSpellId = [](void*) { return s_castingSpell; };
        d.uiCastingSpell = [](void*) { return 1234; };
        d.setSpellRangeFlags = [](uint32_t range, uint32_t flags) { s_rangeFlags[range] = flags; };
        d.setModalSkip = [](bool on) { s_modalSkip = on; };
        d.fatal = [](const char* message) { s_fatal = message; };
    }

    int32_t Dispatch(uint32_t opcode, CoATest::Bytes& bytes) {
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        return result;
    }

    // A SpellAddon row whose effect slots (+0x50) carry the given types.
    std::vector<uint8_t> Addon(uint32_t t0, uint32_t t1, uint32_t t2) {
        std::vector<uint8_t> row(0x60, 0);
        memcpy(row.data() + 0x50, &t0, 4);
        memcpy(row.data() + 0x54, &t1, 4);
        memcpy(row.data() + 0x58, &t2, 4);
        return row;
    }
}

COA_TEST(SpellMods_CooldownPacketCallsSetCooldownLeftPerRecord) {
    Start();
    CoATest::Bytes b;
    b.U32(2).U32(10).U32(11).U32(12).U32(13).U32(0x80000005).U32(20).U32(21).U32(22).U32(23).U32(7);
    Dispatch(0x64F, b);
    CHECK(s_cooldowns.size() == 2);
    CHECK((s_cooldowns[0] == std::vector<uint32_t>{ 10, 11, 12, 13, 5, 1, 0 }));
    CHECK((s_cooldowns[1] == std::vector<uint32_t>{ 20, 21, 22, 23, 7, 0, 0 }));
}

COA_TEST(SpellMods_CooldownPacketCountGuardedByBytesLeft) {
    Start();
    CoATest::Bytes b;
    b.U32(1000).U32(1).U32(2).U32(3).U32(4).U32(5).U32(6).U32(7);   // one whole record, one partial
    Dispatch(0x64F, b);
    CHECK(s_cooldowns.size() == 1);

    CoATest::Bytes empty;
    Dispatch(0x64F, empty);
    CHECK(s_cooldowns.size() == 1);
}

COA_TEST(SpellMods_ClassModifierPacketsWriteTheTables) {
    Start();
    CoATest::Bytes flat;
    flat.U8(0).U8(3).U8(10).U32(static_cast<uint32_t>(-15)).U32(4);   // bit 3, op 10, value -15, family 4
    CHECK(CoATest::Deliver(CoA::SpellMods::OnSpellModifier, 0x266, flat) == 1);
    const uint32_t index = 3 * 0x1F + 4 * 0x11A0 + 10;
    CHECK(CoA::SpellMods::FlatAt(index) == -15);
    CHECK(CoA::SpellMods::PctAt(index) == 0);

    CoATest::Bytes pct;
    pct.U8(0).U8(3).U8(10).U32(25).U32(4);
    CoATest::Deliver(CoA::SpellMods::OnSpellModifier, 0x267, pct);
    CHECK(CoA::SpellMods::PctAt(index) == 25);
    CHECK(CoA::SpellMods::FlatAt(index) == -15);
}

COA_TEST(SpellMods_ClassModifierOutOfTableOrShortIsIgnored) {
    Start();
    CoATest::Bytes far;
    far.U8(0).U8(3).U8(10).U32(99).U32(0x100);   // family 0x100: far past the 0x2AF60 table
    CHECK(CoATest::Deliver(CoA::SpellMods::OnSpellModifier, 0x266, far) == 1);

    CoATest::Bytes shortMsg;
    shortMsg.U8(0).U8(1).U8(2).U32(5);           // family missing
    CHECK(CoATest::Deliver(CoA::SpellMods::OnSpellModifier, 0x266, shortMsg) == 1);
    CHECK(CoA::SpellMods::FlatAt(1 * 0x1F + 2) == 0);

    CoATest::Bytes shortFamily;
    shortFamily.U8(1).U8(2).U32(5);              // spell missing
    CHECK(CoATest::Deliver(CoA::SpellMods::OnSpellModifier, 0x266, shortFamily) == 1);
}

COA_TEST(SpellMods_FamilyModifierAddsToEveryAffectedSpell) {
    Start();
    s_spells[500].id = 500;
    s_spells[600].id = 600;
    s_spells[700].id = 700;
    s_spells[701].id = 701;
    // 702 unknown: skipped.
    s_affects[500] = { { 1, 500, 600 }, { 2, 500, static_cast<uint32_t>(-700) } };

    CoATest::Bytes b;
    b.U8(1).U8(7).U32(5).U32(500);   // op 7, value 5, spell 500
    CoATest::Deliver(CoA::SpellMods::OnSpellModifier, 0x266, b);
    CHECK(CoA::SpellMods::SpellEntry(true, 600, 7) == 5);
    CHECK(CoA::SpellMods::SpellEntry(true, 700, 7) == 5);
    CHECK(CoA::SpellMods::SpellEntry(true, 701, 7) == 5);
    CHECK(CoA::SpellMods::SpellEntry(true, 702, 7) == 0);
    CHECK(CoA::SpellMods::SpellEntry(false, 600, 7) == 0);

    CoATest::Bytes again;
    again.U8(1).U8(7).U32(3).U32(500);
    CoATest::Deliver(CoA::SpellMods::OnSpellModifier, 0x266, again);
    CHECK(CoA::SpellMods::SpellEntry(true, 600, 7) == 8);

    // An unknown modifier spell does nothing.
    CoATest::Bytes unknown;
    unknown.U8(1).U8(7).U32(3).U32(999);
    CoATest::Deliver(CoA::SpellMods::OnSpellModifier, 0x267, unknown);
    CHECK(CoA::SpellMods::SpellEntry(false, 600, 7) == 0);
}

COA_TEST(SpellMods_ModifierSumOverTablesAndSpellMaps) {
    Start();
    const uint32_t mask[3] = { 0x9, 0, 0x1 };   // bits 0, 3 and 64
    int32_t flat = -1, pct = -1;

    // No class set / the opt-out bit: false, flat 0, pct 100.
    CHECK(!CoA::SpellMods::ModifierSum(1, 0, mask, 0, 10, flat, pct));
    CHECK(flat == 0 && pct == 100);
    CHECK(!CoA::SpellMods::ModifierSum(1, 4, mask, 0x20000000, 10, flat, pct));
    CHECK(flat == 0 && pct == 100);
    // Nothing set: false, both 0.
    CHECK(!CoA::SpellMods::ModifierSum(1, 4, mask, 0, 10, flat, pct));
    CHECK(flat == 0 && pct == 0);

    CoATest::Bytes a;
    a.U8(0).U8(3).U8(10).U32(4).U32(4);
    CoATest::Deliver(CoA::SpellMods::OnSpellModifier, 0x266, a);
    CoATest::Bytes b;
    b.U8(0).U8(64).U8(10).U32(static_cast<uint32_t>(-30)).U32(4);
    CoATest::Deliver(CoA::SpellMods::OnSpellModifier, 0x267, b);
    CoATest::Bytes c;
    c.U8(0).U8(1).U8(10).U32(1000).U32(4);   // bit 1 is not in the mask
    CoATest::Deliver(CoA::SpellMods::OnSpellModifier, 0x266, c);

    CHECK(CoA::SpellMods::ModifierSum(1, 4, mask, 0, 10, flat, pct));
    CHECK(flat == 4);
    CHECK(pct == 70);
    // Another op: nothing.
    CHECK(!CoA::SpellMods::ModifierSum(1, 4, mask, 0, 11, flat, pct));

    // The per-spell maps add on, and a pct below -100 clamps to 0.
    s_spells[500].id = 500;
    s_spells[1].id = 1;
    s_affects[500] = { { 0, 500, 1 } };
    CoATest::Bytes d;
    d.U8(1).U8(10).U32(static_cast<uint32_t>(-200)).U32(500);
    CoATest::Deliver(CoA::SpellMods::OnSpellModifier, 0x267, d);
    CHECK(CoA::SpellMods::ModifierSum(1, 4, mask, 0, 10, flat, pct));
    CHECK(flat == 4);
    CHECK(pct == 0);
}

COA_TEST(SpellMods_GlueScreenClearsTablesAndMapsButNotTheAuraFlag) {
    Start();
    s_spells[500].id = 500;
    s_spells[600].id = 600;
    s_affects[500] = { { 0, 500, 600 } };
    CoATest::Bytes a;
    a.U8(0).U8(2).U8(3).U32(9).U32(1);
    CoATest::Deliver(CoA::SpellMods::OnSpellModifier, 0x266, a);
    CoATest::Bytes b;
    b.U8(1).U8(3).U32(9).U32(500);
    CoATest::Deliver(CoA::SpellMods::OnSpellModifier, 0x267, b);
    Spell aura;
    aura.effectAura[1] = 0x138;
    aura.effectMisc[2] = 1;
    CoA::SpellMods::AuraFlagOn(aura);
    CHECK(CoA::SpellMods::AuraFlag());

    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoA::SpellMods::FlatAt(2 * 0x1F + 0x11A0 + 3) == 0);
    CHECK(CoA::SpellMods::SpellEntry(false, 600, 3) == 0);
    CHECK(CoA::SpellMods::AuraFlag());
}

COA_TEST(SpellMods_PowerPerSecondReplacedBySpellCustomAttr) {
    Start();
    Spell s;
    s.id = 42;
    s.manaPerSecond = 30;
    s.powerType = 0;
    s_basePower = 1001;
    int32_t out = -1;
    CHECK(!CoA::SpellMods::PowerPerSecondReplaced(s, kUnit, out));
    s_attr18[42] = 0x20000;
    CHECK(!CoA::SpellMods::PowerPerSecondReplaced(s, nullptr, out));
    CHECK(CoA::SpellMods::PowerPerSecondReplaced(s, kUnit, out));
    CHECK(out == 300);   // 30 * 1001 / 100, unsigned
}

COA_TEST(SpellMods_PowerCostScaledByUnitAddonFloat) {
    Start();
    Spell s;
    s.defenseType = 1;
    s.powerType = 2;
    SetAddonFloat(kUnitGuid, 1 * 7 + 7 + 2, 0.5f);
    CHECK(CoA::SpellMods::AfterPowerPerSecond(s, kUnit, 100) == 150);
    CHECK(CoA::SpellMods::AfterPowerPerSecond(s, kUnit, 0) == 0);
    CHECK(CoA::SpellMods::AfterPowerPerSecond(s, nullptr, 100) == 100);
    s.defenseType = 4;   // out of the scaled range
    CHECK(CoA::SpellMods::AfterPowerPerSecond(s, kUnit, 100) == 100);

    // GetPowerCost: the same scaling; a non-zero cast time or power type keeps it.
    s.defenseType = 1;
    s_castTime = 1500;
    CHECK(CoA::SpellMods::AfterPowerCost(s, nullptr, kUnit, 100) == 150);
    CHECK(CoA::SpellMods::AfterPowerCost(s, nullptr, nullptr, -1) == -1);
    CHECK(CoA::SpellMods::AfterPowerCost(s, nullptr, kUnit, 0) == 0);

    // Instant, mana: times (1 - field 6).
    Spell instant;
    instant.defenseType = 7;
    instant.powerType = 0;
    s_castTime = 0;
    SetAddonFloat(kUnitGuid, 6, 0.25f);
    CHECK(CoA::SpellMods::AfterPowerCost(instant, nullptr, kUnit, 200) == 150);
    instant.powerType = 1;
    CHECK(CoA::SpellMods::AfterPowerCost(instant, nullptr, kUnit, 200) == 200);
}

COA_TEST(SpellMods_CastTimeScaledBySpellAddonAuraAmounts) {
    Start();
    Spell cast;
    cast.schoolMask = 0x4;
    CHECK(CoA::SpellMods::AfterCastTime(&cast, 2000) == 2000);

    s_spells[900].id = 900;
    s_spells[900].effectMisc[1] = 0x6;
    s_addons[900] = Addon(0, 0x15D, 0);
    s_playerAuras.push_back({ 0x77, 900, 1 });
    s_amounts[0] = 0;
    s_amounts[1] = 20;
    s_amounts[2] = 0;
    CHECK(CoA::SpellMods::AfterCastTime(&cast, 2000) == 2000);   // no server amounts
    s_haveAmounts = true;
    CHECK(CoA::SpellMods::AfterCastTime(&cast, 2000) == 1600);
    cast.schoolMask = 0x1;   // not in the effect's misc mask
    CHECK(CoA::SpellMods::AfterCastTime(&cast, 2000) == 2000);
}

COA_TEST(SpellMods_SpellRangeMinZeroedUnderAuraType138) {
    Start();
    s_spells[50].id = 50;
    float minRange = 8.0f;
    CoA::SpellMods::AfterSpellRange(kUnit, 50, &minRange);
    CHECK(minRange == 8.0f);
    s_affected.push_back({ 1, 0x138, 50 });
    CoA::SpellMods::AfterSpellRange(kUnit, 50, &minRange);
    CHECK(minRange == 0.0f);

    // With the aura flag set any spell's min range goes.
    float other = 5.0f;
    Spell aura;
    aura.effectAura[0] = 0x138;
    aura.effectMisc[0] = 1;
    CoA::SpellMods::AuraFlagOn(aura);
    CoA::SpellMods::AfterSpellRange(kUnit, 51, &other);
    CHECK(other == 0.0f);
    CoA::SpellMods::AfterSpellRange(nullptr, 51, &other);
}

COA_TEST(SpellMods_AutoShotMeleeGates) {
    Start();
    s_slots[15] = 0;
    s_slots[17] = 0;
    s_activePlayer = false;
    CHECK(!CoA::SpellMods::InMeleeOverride(kUnit));
    CHECK(!CoA::SpellMods::MeleeRangeVeto(kUnit));

    s_autoRepeat = 0x4B;
    CHECK(!CoA::SpellMods::InMeleeOverride(kUnit));
    s_manastorm = true;
    CHECK(CoA::SpellMods::InMeleeOverride(kUnit));
    CHECK(CoA::SpellMods::MeleeRangeVeto(kUnit));
    s_manastorm = false;
    s_spells[0x4B].id = 0x4B;
    s_affected.push_back({ 1, 0x138, 0x4B });
    CHECK(CoA::SpellMods::InMeleeOverride(kUnit));
    s_affected.clear();

    // The player without a main-hand weapon.
    s_activePlayer = true;
    CHECK(CoA::SpellMods::InMeleeOverride(kUnit));
    s_slots[15] = 0x1234;
    CHECK(!CoA::SpellMods::InMeleeOverride(kUnit));
    CHECK(!CoA::SpellMods::MeleeRangeVeto(kUnit));

    // Without Auto Shot: no main hand and a ranged weapon still vetoes melee range.
    s_autoRepeat = 0;
    s_slots[15] = 0;
    s_slots[17] = 0x99;
    CHECK(CoA::SpellMods::MeleeRangeVeto(kUnit));
    CHECK(!CoA::SpellMods::InMeleeOverride(kUnit));
}

COA_TEST(SpellMods_CastSpellPatchesAroundTheCast) {
    Start();
    // Aura type 0x139 on the caster: cast while moving for the cast only.
    s_spells[10].id = 10;
    s_affected.push_back({ 1, 0x139, 10 });
    CoA::SpellMods::BeforeCastSpell(kUnit, 10);
    CHECK(CoA::SpellMods::CastWhileMoving());
    CHECK(!CoA::SpellMods::SheatheAnimates());
    CoA::SpellMods::AfterCastSpell();
    CHECK(!CoA::SpellMods::CastWhileMoving());

    // A mounted aura effect (0x4E): the sheathe jump for the cast only.
    s_spells[11].id = 11;
    s_spells[11].effectAura[2] = 0x4E;
    CoA::SpellMods::BeforeCastSpell(kUnit, 11);
    CHECK(CoA::SpellMods::SheatheAnimates());
    CHECK(!CoA::SpellMods::CastWhileMoving());
    CoA::SpellMods::AfterCastSpell();
    CHECK(!CoA::SpellMods::SheatheAnimates());

    // Family 5 with the masked bits and aura 0x7B9B; family 3 likewise.
    s_spells[12].id = 12;
    s_spells[12].classSet = 5;
    s_spells[12].classMask[1] = 0x40;   // byte +0x248 & 0xC0
    CoA::SpellMods::BeforeCastSpell(kUnit, 12);
    CHECK(!CoA::SpellMods::CastWhileMoving());
    s_hasAura.push_back(0x7B9B);
    CoA::SpellMods::BeforeCastSpell(kUnit, 12);
    CHECK(CoA::SpellMods::CastWhileMoving());
    CoA::SpellMods::AfterCastSpell();
    s_spells[13].id = 13;
    s_spells[13].classSet = 3;
    s_spells[13].classMask[0] = 0x400000;
    CoA::SpellMods::BeforeCastSpell(kUnit, 13);
    CHECK(CoA::SpellMods::CastWhileMoving());
    CoA::SpellMods::AfterCastSpell();
    s_spells[13].classMask[0] = 0x2;
    CoA::SpellMods::BeforeCastSpell(kUnit, 13);
    CHECK(!CoA::SpellMods::CastWhileMoving());
    CoA::SpellMods::AfterCastSpell();
}

COA_TEST(SpellMods_ErrorSpeechSwitchesFollowTheCVarWhileCasting) {
    Start();
    CHECK(CoA::SpellMods::ActivateSound());
    CHECK(CoA::SpellMods::FizzleSound());
    s_errorSpeech = false;
    CoA::SpellMods::BeforeCastSpell(kUnit, 1);   // not casting: untouched
    CHECK(CoA::SpellMods::ActivateSound());
    s_castingSpell = 77;
    CoA::SpellMods::BeforeCastSpell(kUnit, 1);
    CHECK(!CoA::SpellMods::ActivateSound());
    CHECK(!CoA::SpellMods::FizzleSound());
    CoA::SpellMods::AfterCastSpell();   // left as set
    CHECK(!CoA::SpellMods::ActivateSound());
    s_errorSpeech = true;
    CoA::SpellMods::BeforeCastSpell(kUnit, 1);
    CHECK(CoA::SpellMods::ActivateSound());
    CHECK(CoA::SpellMods::FizzleSound());
    CoA::SpellMods::BeforeCastSpell(nullptr, 1);   // no caster: nothing
}

COA_TEST(SpellMods_AuraFlagFollowsAura138Spells) {
    Start();
    Spell plain;
    plain.effectAura[0] = 0x138;
    plain.effectMisc[0] = 2;
    CHECK(CoA::SpellMods::AuraFlagOn(plain));
    CHECK(!CoA::SpellMods::AuraFlag());

    Spell masked;
    masked.effectAura[0] = 0x138;
    masked.effectMisc[0] = 1;
    masked.maskB[0] = 4;   // a mask: not the flag spell
    CHECK(CoA::SpellMods::AuraFlagOn(masked));
    CHECK(!CoA::SpellMods::AuraFlag());

    Spell flag;
    flag.effectAura[2] = 0x138;
    flag.effectMisc[1] = 1;
    CHECK(CoA::SpellMods::AuraFlagOn(flag));
    CHECK(CoA::SpellMods::AuraFlag());
    CHECK(s_modalSkip);
    CHECK(s_rangeFlags[0x72] == 0);
    CHECK(s_rangeFlags[0x1BD] == 0);
    CHECK(CoA::SpellMods::AuraFlagOff(flag));
    CHECK(!CoA::SpellMods::AuraFlag());
    CHECK(!s_modalSkip);
    CHECK(s_rangeFlags[0x72] == 2);
    CHECK(s_rangeFlags[0x1BD] == 2);
}

COA_TEST(SpellMods_UsableActionRequirements) {
    Start();
    Spell s;
    s.id = 300;
    CHECK(CoA::SpellMods::StackRequirement(kPlayer, s));
    CHECK(CoA::SpellMods::AuraRequirement(kPlayer, s));

    // 0x4000: needs aura 0xC3849.
    s_attr18[300] = 0x4000;
    CHECK(!CoA::SpellMods::AuraRequirement(kPlayer, s));
    CHECK(CoA::SpellMods::AuraRequirement(nullptr, s));
    s_hasAura.push_back(0xC3849);
    CHECK(CoA::SpellMods::AuraRequirement(kPlayer, s));

    // 0x2000 with a required caster aura: its stacks against the required spell's StackAmount.
    s_attr18[300] = 0x2000;
    s.casterAuraSpell = 310;
    CHECK(!CoA::SpellMods::StackRequirement(kPlayer, s));   // unknown required spell
    s_spells[310].id = 310;
    s_spells[310].cumulativeAura = 3;
    CHECK(!CoA::SpellMods::StackRequirement(kPlayer, s));   // no aura
    s_playerAuras.push_back({ 0, 310, 2 });
    CHECK(!CoA::SpellMods::StackRequirement(kPlayer, s));
    s_playerAuras.back().stack = 3;
    CHECK(CoA::SpellMods::StackRequirement(kPlayer, s));
    s_attr18[300] = 0;
    s_playerAuras.clear();
    CHECK(CoA::SpellMods::StackRequirement(kPlayer, s));
}

COA_TEST(SpellMods_CancelSpellPaths) {
    Start();
    CoA::SpellMods::NullCancelFromStopCasting();
    CHECK(s_fatal == "Spell_C_CancelSpell: spellCastLink = nullptr but GetUICastingSpellID = 1234.");

    s_spells[20].id = 20;
    CHECK(!CoA::SpellMods::KeepCastOnMove(20));
    s_affected.push_back({ 2, 0x139, 20 });
    CHECK(CoA::SpellMods::KeepCastOnMove(20));
}
