#include "CoA/Source/Modules/AuraAmounts.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    constexpr uint64_t kPlayer = 0x11;
    constexpr uint64_t kOther = 0x22;
    bool s_otherAccepted = false;

    // Spells: 100 an absorb shield (aura 0x45, misc 0x7F), 200 a heal absorb (0x12D / 0x7F), 300 a school absorb
    // (0x45, misc 4) on effect 1 only, 400 an unrelated aura.
    bool Effects(uint32_t spell, CoA::AuraAmounts::SpellEffects& out) {
        out = {};

        switch (spell) {
            case 100: out.aura[0] = 0x45; out.misc[0] = 0x7F; return true;
            case 200: out.aura[0] = 0x12D; out.misc[0] = 0x7F; return true;
            case 300: out.aura[1] = 0x45; out.misc[1] = 4; return true;
            case 400: out.aura[0] = 3; return true;
            default: return false;
        }
    }

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::AuraAmounts::Init();
        CoA::AuraAmounts::Deps& deps = CoA::AuraAmounts::Dependencies();
        deps.spellEffects = Effects;
        deps.acceptsSpell = [](uint32_t, uint64_t) { return s_otherAccepted; };
        deps.tokens = [](uint64_t unit) { return unit == kPlayer ? V{ "player" } : V{ "target", "nameplate1" }; };
        deps.tokenGuid = [](const char* token, uint64_t& guid) {
            const std::string t = token;
            guid = t == "player" ? kPlayer : t == "target" ? kOther : 0;
            return t == "player" || t == "target" || t == "focus";
        };
        deps.unitExists = [](uint64_t guid) { return guid == kPlayer || guid == kOther; };
        deps.activePlayer = [] { return kPlayer; };
        s_otherAccepted = false;
        return CoATest::NewState(false);
    }

    CoATest::Bytes Record(uint32_t slot, uint64_t caster, uint32_t spell, int32_t a0, int32_t a1, int32_t a2) {
        CoATest::Bytes b;
        b.U32(slot).U64(caster).U32(spell);

        if (slot != 0xFFFFFFFF) {
            b.U32(static_cast<uint32_t>(a0)).U32(static_cast<uint32_t>(a1)).U32(static_cast<uint32_t>(a2));
        }

        return b;
    }

    std::vector<std::string> Feed(uint32_t opcode, CoATest::Bytes bytes) {
        CoATest::RecordSignals();
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    CoATest::Bytes Unit(uint64_t unit, const CoATest::Bytes& rec) {
        CoATest::Bytes b;
        b.U64(unit);
        b.Raw(rec.data.data(), static_cast<uint32_t>(rec.data.size()));
        return b;
    }
}

// SMSG 0x673: one record; the totals sum the matching effects; events per unit token.
COA_TEST(AuraAmounts_UpdateAndTotals) {
    lua_State* L = Setup();
    CHECK(Feed(0x673, Unit(kPlayer, Record(1, 0x99, 100, 500, 0, 0))) == V{ "UNIT_ABSORB_AMOUNT_CHANGED:player" });
    CHECK(Feed(0x673, Unit(kPlayer, Record(2, 0x99, 200, 70, 0, 0))) == V{ "UNIT_HEAL_ABSORB_AMOUNT_CHANGED:player" });
    CHECK(Feed(0x673, Unit(kPlayer, Record(3, 0x99, 300, 1, 40, 2))).empty());
    CHECK(CoATest::Call(L, "", "UnitGetTotalAbsorbs", { "s:player" })[0] == "500");
    CHECK(CoATest::Call(L, "", "UnitGetTotalHealAbsorbs", { "s:player" })[0] == "70");
    CHECK(CoATest::Call(L, "", "UnitGetTotalSchoolAbsorbs", { "s:player" })[0] == "540");
    // An update of the same (caster, spell) replaces the amounts.
    Feed(0x673, Unit(kPlayer, Record(1, 0x99, 100, 650, 0, 0)));
    CHECK(CoATest::Call(L, "", "UnitGetTotalAbsorbs", { "s:player" })[0] == "650");
    // A bad token gives nil; a token naming no unit gives 0.
    CHECK(CoATest::Call(L, "", "UnitGetTotalAbsorbs", { "s:bogus" })[0] == "nil");
    CHECK(CoATest::Call(L, "", "UnitGetTotalAbsorbs", { "s:focus" })[0] == "0");
    const int32_t* found = CoA::AuraAmounts::Find(kPlayer, 0x99, 300);
    CHECK(found && found[1] == 40);
}

// A removal (slot 0xFFFFFFFF) of an entry that is not there still fires the change event, as the original.
COA_TEST(AuraAmounts_RemoveMissing) {
    Setup();
    CHECK(Feed(0x673, Unit(kPlayer, Record(0xFFFFFFFF, 0x99, 100, 0, 0, 0))) == V{ "UNIT_ABSORB_AMOUNT_CHANGED:player" });
    // An unknown spell stops the record: nothing changes, nothing fires.
    CHECK(Feed(0x673, Unit(kPlayer, Record(1, 0x99, 999, 5, 0, 0))).empty());
}

// Another unit's new entry is only kept when the client accepts the spell for it.
COA_TEST(AuraAmounts_OtherUnitFiltered) {
    lua_State* L = Setup();
    CHECK(Feed(0x673, Unit(kOther, Record(1, 0x99, 100, 30, 0, 0))).empty());
    CHECK(CoATest::Call(L, "", "UnitGetTotalAbsorbs", { "s:target" })[0] == "0");
    s_otherAccepted = true;
    CHECK((Feed(0x673, Unit(kOther, Record(1, 0x99, 100, 30, 0, 0))) == V{ "UNIT_ABSORB_AMOUNT_CHANGED:target", "UNIT_ABSORB_AMOUNT_CHANGED:nameplate1" }));
    CHECK(CoATest::Call(L, "", "UnitGetTotalAbsorbs", { "s:target" })[0] == "30");
}

// SMSG 0x674: records to the packet's end; an unknown spell stops the run.
COA_TEST(AuraAmounts_UpdateAll) {
    lua_State* L = Setup();
    CoATest::Bytes b = Unit(kPlayer, Record(1, 0x99, 100, 10, 0, 0));
    const CoATest::Bytes second = Record(2, 0x98, 100, 20, 0, 0);
    const CoATest::Bytes unknown = Record(3, 0x97, 999, 1, 1, 1);
    const CoATest::Bytes third = Record(4, 0x96, 100, 40, 0, 0);
    b.Raw(second.data.data(), static_cast<uint32_t>(second.data.size()));
    b.Raw(unknown.data.data(), static_cast<uint32_t>(unknown.data.size()));
    b.Raw(third.data.data(), static_cast<uint32_t>(third.data.size()));
    Feed(0x674, b);
    CHECK(CoATest::Call(L, "", "UnitGetTotalAbsorbs", { "s:player" })[0] == "30");
}

// The glue screen empties the map; a freed object's entries go.
COA_TEST(AuraAmounts_GlueResets) {
    lua_State* L = Setup();
    Feed(0x673, Unit(kPlayer, Record(1, 0x99, 100, 10, 0, 0)));
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoATest::Call(L, "", "UnitGetTotalAbsorbs", { "s:player" })[0] == "0");
    Feed(0x673, Unit(kPlayer, Record(1, 0x99, 100, 10, 0, 0)));
    CoA::AuraAmounts::Forget(kPlayer);
    CHECK(CoA::AuraAmounts::Find(kPlayer, 0x99, 100) == nullptr);
}
