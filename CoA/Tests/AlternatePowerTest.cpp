// AscAlternatePower.cpp (AlternatePower.cpp): the alternate power event on the player's addon fields 0 / 1 after
// PostInitActivePlayer, the forced death of kind-3 objects (field 4 == 1), the jump-cast spell remembered after a
// spellbook add (ClientHooks::afterAddKnownSpell, SpellCustomAttr +0x14) and cast / cancelled on key 0x20.
#include "CoA/Source/Modules/AlternatePower.h"
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    const uint64_t kPlayer = 0x0000000000000042ull;
    const uint64_t kCreature = 0xF130000123000077ull;

    struct FakeObject {
        uint64_t guid;
    };

    std::vector<std::string> s_calls;
    bool s_hasPlayer = true;
    bool s_hasAura = false;
    bool s_falling = false;

    bool ReadAttrs(const char* path, std::vector<uint8_t>& out) {
        if (!strstr(path, "SpellCustomAttr.dbc")) {
            return false;
        }

        // FUN_1020b080 skips the first row (its loop runs MinId..MaxId-1 from MinId + 1); +4 spell, +0x14 the mask.
        out = CoATest::Wdbc({ { 1, 9999, 0, 0, 0, 0x3000, 0 }, { 2, 501, 0, 0, 0, 0x1000, 0 }, { 3, 502, 0, 0, 0, 0x2000, 0 },
            { 4, 503, 0, 0, 0, 0x3000, 0 }, { 5, 0, 0, 0, 0, 0, 0 } }, std::string(1, '\0'));
        return true;
    }

    void ClearHooks() {
        CoA::ClientHooks::Reset();
        CoA::ClientHooks::afterActivePlayerInit.Clear();
        CoA::ClientHooks::beforeKeyDown.Clear();
    }

    void Start() {
        ClearHooks();
        CoA::Packets::Clear();
        CoA::ObjectAddon::Reset();
        CoA::ObjectAddon::Init();
        CoA::AlternatePower::Reset();
        CoA::AlternatePower::Init();
        CoA::Dbc::SetFileReader(ReadAttrs);
        CoA::Dbc::Get("DBFilesClient\\SpellCustomAttr.dbc").Unload();
        CoA::CA::RebuildSpellCustomAttr();
        s_calls.clear();
        s_hasPlayer = true;
        s_hasAura = false;
        s_falling = false;
        CoA::AlternatePower::Deps& d = CoA::AlternatePower::Dependencies();
        d.objectGuid = [](void* object) { return static_cast<FakeObject*>(object)->guid; };
        d.forceDeath = [](uint64_t guid) { s_calls.push_back("death:" + std::to_string(guid & 0xFFFF)); };
        d.hasPlayer = [] { return s_hasPlayer; };
        d.hasAura = [](uint32_t spell) { s_calls.push_back("aura?" + std::to_string(spell)); return s_hasAura; };
        d.spellExists = [](uint32_t spell) { return spell != 503; };
        d.cancelAura = [](uint32_t spell) { s_calls.push_back("cancel:" + std::to_string(spell)); };
        d.falling = [] { return s_falling; };
        d.castOnSelf = [](uint32_t spell) { s_calls.push_back("cast:" + std::to_string(spell)); };
    }

    void Stop() {
        CoATest::StopRecordingSignals();
        CoA::Dbc::Get("DBFilesClient\\SpellCustomAttr.dbc").Unload();
        CoA::CA::RebuildSpellCustomAttr();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::AlternatePower::Reset();
        CoA::ObjectAddon::Reset();
        ClearHooks();
        CoA::Packets::Clear();
    }

    // SMSG_UPDATE_OBJECT_ADDON 0x578: u64 guid, {u32 index, u32 value}...
    V Update(uint64_t guid, std::vector<std::pair<uint32_t, uint32_t>> fields) {
        CoATest::Bytes b;
        b.U64(guid);

        for (const auto& f : fields) {
            b.U32(f.first).U32(f.second);
        }

        CoATest::RecordSignals();
        CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(0x578, 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    void Key(uint32_t key) {
        uint32_t event[4] = { key, 0, 0, 0 };
        CoA::ClientHooks::beforeKeyDown.Run(event);
    }
}

// FUN_10321b60 after 0x6E7F50: fields 0 and 1 of the player's record fire UNIT_POWER_ALTERNATIVE_UPDATE with field 0.
COA_TEST(AlternatePower_Event) {
    Start();
    CHECK(Update(kPlayer, { { 0, 3 } }).empty());   // not watched yet
    FakeObject player{ kPlayer };
    CoA::ClientHooks::afterActivePlayerInit.Run(&player);
    CHECK(CoA::ObjectAddon::Unit(kPlayer)[0].arg == reinterpret_cast<void*>(4));
    CHECK(Update(kPlayer, { { 0, 7 } }) == V{ "UNIT_POWER_ALTERNATIVE_UPDATE:7" });
    CHECK(Update(kPlayer, { { 1, 50 } }) == V{ "UNIT_POWER_ALTERNATIVE_UPDATE:7" });
    CHECK(Update(kPlayer, { { 2, 50 } }).empty());
    CHECK(Update(kPlayer, { { 1, 2 }, { 0, 9 } }) == (V{ "UNIT_POWER_ALTERNATIVE_UPDATE:9", "UNIT_POWER_ALTERNATIVE_UPDATE:9" }));
    // Another unit's record is not watched.
    CHECK(Update(0x43, { { 0, 1 } }).empty());
    Stop();
}

// 0x10A4BAF0 (registered after 0x6E7F50): field 4 of a creature (kind 3) set to 1 forces its death; other values and
// other kinds do not.
COA_TEST(AlternatePower_ForcedDeath) {
    Start();
    CHECK(Update(kCreature, { { 4, 1 } }).empty() && s_calls.empty());   // not registered yet
    FakeObject player{ kPlayer };
    CoA::ClientHooks::afterActivePlayerInit.Run(&player);
    Update(kCreature, { { 4, 2 } });
    CHECK(s_calls.empty());
    Update(kCreature, { { 4, 1 } });
    CHECK(s_calls == V{ "death:119" });
    Update(kPlayer, { { 4, 1 } });   // a player is kind 4
    CHECK(s_calls.size() == 1);
    Stop();
}

// FUN_10322270 on afterAddKnownSpell: +0x14 & 0x1000 remembers the flag spell, & 0x2000 the jump spell. FUN_103222b0 on
// key 0x20: with the aura, cancel it; without, cast while falling.
COA_TEST(AlternatePower_JumpCast) {
    Start();
    Key(0x20);
    CHECK(s_calls.empty());   // no jump spell
    CoA::ClientHooks::afterAddKnownSpell.Run(9999);   // the skipped first row
    CHECK(CoA::AlternatePower::FlagSpell() == 0 && CoA::AlternatePower::JumpSpell() == 0);
    CoA::ClientHooks::afterAddKnownSpell.Run(501);
    CHECK(CoA::AlternatePower::FlagSpell() == 501 && CoA::AlternatePower::JumpSpell() == 0);
    CoA::ClientHooks::afterAddKnownSpell.Run(502);
    CHECK(CoA::AlternatePower::FlagSpell() == 501 && CoA::AlternatePower::JumpSpell() == 502);
    CoA::ClientHooks::afterAddKnownSpell.Run(777);   // no row: nothing changes
    CHECK(CoA::AlternatePower::JumpSpell() == 502);
    Key(0x20);
    CHECK(s_calls == V{ "aura?502" });   // not falling
    s_falling = true;
    s_calls.clear();
    Key(0x20);
    CHECK(s_calls == (V{ "aura?502", "cast:502" }));
    s_hasAura = true;
    s_calls.clear();
    Key(0x20);
    CHECK(s_calls == (V{ "aura?502", "cancel:502" }));
    s_calls.clear();
    Key(0x21);
    s_hasPlayer = false;
    Key(0x20);
    CHECK(s_calls.empty());
    s_hasPlayer = true;
    // Both bits: both remembered; a spell the client lacks is neither cast nor cancelled.
    CoA::ClientHooks::afterAddKnownSpell.Run(503);
    CHECK(CoA::AlternatePower::FlagSpell() == 503 && CoA::AlternatePower::JumpSpell() == 503);
    Key(0x20);
    s_hasAura = false;
    Key(0x20);
    CHECK(s_calls == (V{ "aura?503", "aura?503" }));
    Stop();
}
