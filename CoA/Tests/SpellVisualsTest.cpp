#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/SpellVisuals.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
    using CoA::SpellVisuals::ObjectInfo;

    struct Obj {
        ObjectInfo info;
    };

    const uint64_t kMe = 0x0000000000000042ull;
    const uint64_t kOther = 0x0000000000000077ull;
    const uint64_t kCreature = 0xF130000501ABCDEFull;   // kind 3, counter 0xABCDEF

    Obj s_me{ { 4, kMe } };
    std::map<uint64_t, Obj*> s_objects;
    std::set<uint32_t> s_rows;
    std::map<const void*, uint32_t> s_recordVisuals;
    bool s_appearance = false;

    // SpellCustomAttr.dbc (first row skipped): +4 spell, +0x14 flag 8, +0x18 flag 0x40.
    bool ReadAttrs(const char* path, std::vector<uint8_t>& out) {
        if (!strstr(path, "SpellCustomAttr.dbc")) {
            return false;
        }

        out = CoATest::Wdbc({ { 1, 9999, 0, 0, 0, 0, 0 }, { 2, 801, 0, 0, 0, 8, 0 }, { 3, 802, 0, 0, 0, 0, 0x40 } }, std::string(1, '\0'));
        return true;
    }

    CoA::SpellApi::SpellFields Harmful(uint32_t id) {
        CoA::SpellApi::SpellFields f;
        f.id = id;
        f.attributes = 0x4000000;   // every effect negative
        return f;
    }

    CoA::SpellApi::SpellFields Helpful() {
        CoA::SpellApi::SpellFields f;
        f.id = 0x789D;              // family 0: positive
        return f;
    }

    void SetOption(const char* value);

    void Start() {
        CoA::ClientHooks::Reset();
        CoA::CVars::Reset();
        CoA::Packets::Clear();
        CoA::SpellVisuals::Reset();
        CoA::Dbc::SetFileReader(ReadAttrs);
        CoA::Dbc::Get("DBFilesClient\\SpellCustomAttr.dbc").Unload();
        CoA::CA::RebuildSpellCustomAttr();
        s_objects.clear();
        s_objects[kMe] = &s_me;
        s_rows = { 5000, 5001 };
        s_recordVisuals.clear();
        s_appearance = false;
        CoA::SpellVisuals::Init();
        CoA::CVars::RegisterWorld();
        SetOption("0");   // a registered CVar keeps its value across tests
        CoA::SpellVisuals::Deps& d = CoA::SpellVisuals::Dependencies();
        d.objectInfo = [](void* object, ObjectInfo& out) {
            out = static_cast<Obj*>(object)->info;
            return true;
        };
        d.objectByGuid = [](uint64_t guid, uint32_t mask) -> void* {
            auto it = s_objects.find(guid);

            if (it == s_objects.end()) {
                return nullptr;
            }

            const uint32_t type = it->second->info.type;
            const uint32_t bits = type == 4 ? 0x19 : type == 3 ? 0x9 : (1u << type) | 1;
            return (bits & mask) ? it->second : nullptr;
        };
        d.activePlayer = []() -> void* { return &s_me; };
        d.activeGuid = [] { return kMe; };
        d.visualRowExists = [](uint32_t id) { return s_rows.count(id) != 0; };
        d.effectSpell = [](void* effect) { return *static_cast<uint32_t*>(effect); };
        d.setRecordVisual = [](const void* rec, uint32_t visual) { s_recordVisuals[rec] = visual; };
        d.spell = [](uint32_t id, CoA::SpellApi::SpellFields& out) {
            out = id == 900 ? Helpful() : Harmful(id);
            return true;
        };
        d.playerSpellVisual = [](uint32_t spell, uint32_t& visual) {
            visual = 7000 + spell;
            return s_appearance;
        };
    }

    void Stop() {
        CoATest::StopRecordingPackets();
        CoA::Dbc::Get("DBFilesClient\\SpellCustomAttr.dbc").Unload();
        CoA::CA::RebuildSpellCustomAttr();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::SpellVisuals::Reset();
        CoA::Packets::Clear();
        CoA::ClientHooks::Reset();
        CoA::CVars::Reset();
    }

    void Override(uint64_t guid, uint32_t spell, uint32_t visual) {
        CoATest::Bytes b;
        b.U64(guid).U32(spell).U32(visual);
        CoATest::Deliver(CoA::SpellVisuals::OnSpellVisualOverride, 0x662, b);
    }

    void SetOption(const char* value) {
        CVar* var = CVar::Lookup("hideOtherPlayerHarmfulSpellVisuals");
        CHECK(var != nullptr);

        if (var) {
            var->Set(value, true, false, false, false);
        }
    }
}

// FUN_10322fb0: the CVar (flags 1, "0", category 1, no callback; checked against the DLL), SMSG 0x662 and the three lists
// at their sites.
COA_TEST(SpellVisuals_Install) {
    Start();
    size_t count = 0;
    const CoA::CVarEntry* world = CoA::CVars::World(count);
    CHECK(count == 1);

    if (count == 1) {
        CHECK(strcmp(world[0].name, "hideOtherPlayerHarmfulSpellVisuals") == 0);
        CHECK(strcmp(world[0].defaultValue, "0") == 0);
        CHECK(world[0].flags == 1 && world[0].category == 1);
        CHECK(world[0].callback == nullptr && world[0].help == nullptr && !world[0].setCommand && world[0].slot != nullptr);
    }

    CHECK(CoA::Packets::Has(0x662));
    CHECK(!CoA::ClientHooks::removeEffect.Empty());
    CHECK(!CoA::ClientHooks::visualHide.Empty());
    CHECK(!CoA::ClientHooks::addAuraEffect.Empty());
    CHECK(!CoA::SpellVisuals::OptionOn());
    SetOption("1");
    CHECK(CoA::SpellVisuals::OptionOn());
    Stop();
}

// FUN_102cdc10 / FUN_100a03b0.
COA_TEST(SpellVisuals_GuidKindAndCounter) {
    CHECK(CoA::SpellVisuals::GuidKind(0x0000000000000001ull) == 4);
    CHECK(CoA::SpellVisuals::GuidKind(0xF100000000000001ull) == 6);
    CHECK(CoA::SpellVisuals::GuidKind(0x1FC0000000000001ull) == 5);
    CHECK(CoA::SpellVisuals::GuidKind(0x4000000000000001ull) == 1);
    CHECK(CoA::SpellVisuals::GuidKind(0xF101000000000001ull) == 7);
    CHECK(CoA::SpellVisuals::GuidKind(0xF110000000000001ull) == 5);
    CHECK(CoA::SpellVisuals::GuidKind(0xF130000000000001ull) == 3);
    CHECK(CoA::SpellVisuals::GuidKind(0xF140000000000001ull) == 3);
    CHECK(CoA::SpellVisuals::GuidKind(0xF150000000000001ull) == 3);
    CHECK(CoA::SpellVisuals::GuidKind(0xF120000000000001ull) == 0);
    CHECK(CoA::SpellVisuals::GuidCounter(kCreature) == 0xABCDEF);
    CHECK(CoA::SpellVisuals::GuidCounter(0xF100000001ABCDEFull) == 0x01ABCDEF);
    CHECK(CoA::SpellVisuals::GuidCounter(0x1F40000001ABCDEFull) == 0x01ABCDEF);
    CHECK(CoA::SpellVisuals::GuidCounter(0x0000000701ABCDEFull) == 0x01ABCDEF);
}

// SMSG 0x662 fills the map by kind / counter / spell; FUN_10322c70 reads it, the hide list first, then the appearance.
COA_TEST(SpellVisuals_ResolveAndPacket) {
    Start();
    CHECK(CoA::SpellVisuals::Resolve(3, kCreature, 100) == 0);
    Override(kCreature, 100, 5000);
    CHECK(CoA::SpellVisuals::Resolve(3, kCreature, 100) == 5000);
    CHECK(CoA::SpellVisuals::Resolve(3, 0xF130000999ABCDEFull, 100) == 5000);   // same counter
    CHECK(CoA::SpellVisuals::Resolve(4, kCreature, 100) == 0);                  // another kind
    CHECK(CoA::SpellVisuals::Resolve(3, kCreature, 101) == 0);
    Override(kCreature, 100, 5001);
    CHECK(CoA::SpellVisuals::Resolve(3, kCreature, 100) == 5001);
    // Our own kind-4 counter asks the appearance first.
    Override(kMe, 100, 5000);
    CHECK(CoA::SpellVisuals::Resolve(4, kMe, 100) == 5000);
    s_appearance = true;
    CHECK(CoA::SpellVisuals::Resolve(4, kMe, 100) == 7100);
    CHECK(CoA::SpellVisuals::Resolve(3, kMe, 100) == 0);   // kind 3: no appearance
    // A hide list entry wins over everything.
    CoA::ClientHooks::visualHide.Add([](uint32_t, uint32_t, uint32_t, uint32_t spell) { return spell == 100; }, 1);
    CHECK(CoA::SpellVisuals::Resolve(4, kMe, 100) == 0xFFFFFFFF);
    CHECK(CoA::SpellVisuals::Resolve(3, kCreature, 100) == 0xFFFFFFFF);
    Stop();
}

// 0x10322B50 before: only from OnSpellGo, another player, the option on and a harmful spell.
COA_TEST(SpellVisuals_SkipAppropriateVisual) {
    Start();
    Obj other{ { 4, kOther } };
    Obj creature{ { 3, kCreature } };
    CHECK(!CoA::SpellVisuals::SkipAppropriateVisual(&other, true, Harmful(5)));   // option off (default)
    SetOption("1");
    CHECK(CoA::SpellVisuals::SkipAppropriateVisual(&other, true, Harmful(5)));
    CHECK(!CoA::SpellVisuals::SkipAppropriateVisual(&other, false, Harmful(5)));
    CHECK(!CoA::SpellVisuals::SkipAppropriateVisual(&other, true, Helpful()));
    CHECK(!CoA::SpellVisuals::SkipAppropriateVisual(&s_me, true, Harmful(5)));
    CHECK(!CoA::SpellVisuals::SkipAppropriateVisual(&creature, true, Harmful(5)));
    Stop();
}

// 0x10322B50 after: the object's type is the kind.
COA_TEST(SpellVisuals_AppropriateVisual) {
    Start();
    Obj creature{ { 3, kCreature } };
    CHECK(CoA::SpellVisuals::AppropriateVisual(&creature, 100) == 0);
    Override(kCreature, 100, 5000);
    CHECK(CoA::SpellVisuals::AppropriateVisual(&creature, 100) == 5000);
    Obj asPlayer{ { 4, kCreature } };
    CHECK(CoA::SpellVisuals::AppropriateVisual(&asPlayer, 100) == 0);
    Stop();
}

// 0x10322AD0 (kind from the caster GUID) and 0x10322760.
COA_TEST(SpellVisuals_MissileAndDynamicObject) {
    Start();
    Override(kCreature, 100, 5000);
    CHECK(CoA::SpellVisuals::MissileHitVisual(kCreature, 100) == 5000);
    CHECK(CoA::SpellVisuals::MissileHitVisual(kCreature, 101) == 0);

    Obj other{ { 4, kOther } };
    s_objects[kOther] = &other;
    CHECK(!CoA::SpellVisuals::SkipDynamicObject(kOther, 5));   // option off
    SetOption("1");
    CHECK(CoA::SpellVisuals::SkipDynamicObject(kOther, 5));
    CHECK(!CoA::SpellVisuals::SkipDynamicObject(kOther, 900));      // helpful
    CHECK(!CoA::SpellVisuals::SkipDynamicObject(kMe, 5));           // ours
    CHECK(!CoA::SpellVisuals::SkipDynamicObject(0, 5));
    CHECK(!CoA::SpellVisuals::SkipDynamicObject(kCreature, 5));     // not a player object
    s_objects.erase(kOther);
    CHECK(!CoA::SpellVisuals::SkipDynamicObject(kOther, 5));        // not around
    Stop();
}

// 0x10322810: CMSG 0x9C7 for our missiles and those of units no other player owns.
COA_TEST(SpellVisuals_FireMissileReport) {
    Start();
    CoATest::RecordPackets();
    CoA::SpellVisuals::Missile m;
    m.caster = kMe;
    m.target = kCreature;
    m.spell = 133;
    m.castId = 9;
    m.launch[0] = 1.0f;
    m.launch[1] = 2.0f;
    m.launch[2] = 3.0f;
    m.impact[0] = 4.0f;
    m.impact[1] = 5.0f;
    m.impact[2] = 6.0f;
    CoA::SpellVisuals::AfterFireMissiles(m);
    CoATest::Bytes b;
    b.U64(kMe).U64(kCreature).U32(133).U8(9).F32(1.0f).F32(2.0f).F32(3.0f).F32(4.0f).F32(5.0f).F32(6.0f);
    CHECK(CoATest::SentPackets().size() == 1);
    CHECK(!CoATest::SentPackets().empty() && CoATest::SentPackets()[0] == CoATest::Expected(0x9C7, b));

    m.caster = kOther;   // another player
    s_objects[kOther] = new Obj{ { 4, kOther } };
    CoA::SpellVisuals::AfterFireMissiles(m);
    m.caster = kCreature;   // not around
    CoA::SpellVisuals::AfterFireMissiles(m);
    CHECK(CoATest::SentPackets().size() == 1);

    Obj creature{ { 3, kCreature } };
    s_objects[kCreature] = &creature;
    CoA::SpellVisuals::AfterFireMissiles(m);   // a wild creature
    CHECK(CoATest::SentPackets().size() == 2);
    creature.info.summonedBy = kOther;
    CoA::SpellVisuals::AfterFireMissiles(m);
    creature.info.summonedBy = kMe;
    creature.info.charmedBy = kOther;
    CoA::SpellVisuals::AfterFireMissiles(m);
    creature.info.charmedBy = 0;
    creature.info.createdBy = kOther;
    CoA::SpellVisuals::AfterFireMissiles(m);
    CHECK(CoATest::SentPackets().size() == 2);
    creature.info.createdBy = 0xF130000000000001ull;   // not a player: fine
    CoA::SpellVisuals::AfterFireMissiles(m);
    CHECK(CoATest::SentPackets().size() == 3);
    delete s_objects[kOther];
    Stop();
}

// 0x10323410 / 0x10323480: flag 8 hides on dead players / units and corpses; flag 0x40 on objects that are not ours.
COA_TEST(SpellVisuals_HideAndRemove) {
    Start();
    Obj other{ { 4, kOther } };
    other.info.health = 10;
    s_objects[kOther] = &other;
    const uint32_t lo = static_cast<uint32_t>(kOther);
    CHECK(!CoA::SpellVisuals::HideVisual(4, lo, 0, 801));   // alive
    other.info.health = 0;
    CHECK(CoA::SpellVisuals::HideVisual(4, lo, 0, 801));
    CHECK(!CoA::SpellVisuals::HideVisual(4, lo, 0, 803));   // no row
    CHECK(CoA::SpellVisuals::HideVisual(4, lo, 0, 802));    // flag 0x40 on another player
    CHECK(!CoA::SpellVisuals::HideVisual(4, static_cast<uint32_t>(kMe), 0, 802));
    CHECK(!CoA::SpellVisuals::HideVisual(4, 0x99, 0, 802)); // no such player

    uint32_t effect801 = 801;
    uint32_t effect802 = 802;
    Obj corpse{ { 7, 0xF101000000000001ull } };
    corpse.info.health = 5;
    CHECK(CoA::SpellVisuals::RemoveEffect(&corpse, &effect801));
    Obj unit{ { 3, kCreature } };
    unit.info.health = 5;
    CHECK(!CoA::SpellVisuals::RemoveEffect(&unit, &effect801));
    CHECK(CoA::SpellVisuals::RemoveEffect(&unit, &effect802));
    unit.info.summonedBy = kMe;
    CHECK(!CoA::SpellVisuals::RemoveEffect(&unit, &effect802));
    unit.info.summonedBy = 0;
    unit.info.charmedBy = kMe;
    CHECK(!CoA::SpellVisuals::RemoveEffect(&unit, &effect802));
    CHECK(!CoA::SpellVisuals::RemoveEffect(&s_me, &effect802));
    // Through the lists.
    CHECK(CoA::ClientHooks::removeEffect.AnyTrue(&corpse, &effect801));
    CHECK(CoA::ClientHooks::visualHide.AnyTrue(4, lo, 0, 802));
    Stop();
}

// 0x10323300: the record's visual follows the resolve; spell 0x31029 stays off us and our summons.
COA_TEST(SpellVisuals_BeforeAuraApply) {
    Start();
    Obj creature{ { 3, kCreature } };
    uint32_t rec100 = 100;
    CHECK(CoA::SpellVisuals::BeforeAuraApply(&creature, 0, &rec100));
    CHECK(s_recordVisuals.empty());
    Override(kCreature, 100, 5000);
    CHECK(CoA::SpellVisuals::BeforeAuraApply(&creature, 0, &rec100));
    CHECK(s_recordVisuals[&rec100] == 5000);
    s_recordVisuals.clear();
    Override(kCreature, 100, 6000);   // no such row: unchanged
    CHECK(CoA::SpellVisuals::BeforeAuraApply(&creature, 0, &rec100));
    CHECK(s_recordVisuals.empty());
    CoA::ClientHooks::visualHide.Add([](uint32_t, uint32_t, uint32_t, uint32_t spell) { return spell == 100; }, 1);
    CHECK(CoA::SpellVisuals::BeforeAuraApply(&creature, 0, &rec100));
    CHECK(s_recordVisuals.count(&rec100) && s_recordVisuals[&rec100] == 0);

    uint32_t rec31029 = 0x31029;
    CHECK(!CoA::SpellVisuals::BeforeAuraApply(&s_me, 0, &rec31029));
    CHECK(CoA::SpellVisuals::BeforeAuraApply(&creature, 0, &rec31029));
    creature.info.summonedBy = kMe;
    CHECK(!CoA::SpellVisuals::BeforeAuraApply(&creature, 0, &rec31029));
    // Through the list at 0x10323151.
    CHECK(!CoA::ClientHooks::addAuraEffect.AllPass(&s_me, 0, &rec31029));
    Stop();
}
