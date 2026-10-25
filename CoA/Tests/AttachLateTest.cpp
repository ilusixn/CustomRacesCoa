#include "CoA/Source/Modules/AttachLate.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <map>
#include <string>

namespace {
    // SMSG_REALM_INFO with rates {quest xp, rate1, rate2}.
    void Rates(float rate1, float rate2) {
        CoA::RealmInfo::Reset();
        CoATest::Bytes b;
        b.U32(1).U32(0).F32(1.0f).F32(rate1).F32(rate2).U32(0).F32(0).F32(0).U32(0);

        for (int i = 0; i < 8; ++i) {
            b.U8(0);
        }

        b.Str("").Str("Realm").U8(0);
        CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    }

    std::map<uint32_t, uint32_t> s_supercededBy;
    uint32_t s_known = 0;
    std::string s_fatal;

    bool Lookup(uint32_t spell, uint32_t& supercededBy) {
        auto it = s_supercededBy.find(spell);

        if (it == s_supercededBy.end()) {
            return false;
        }

        supercededBy = it->second;
        return true;
    }

    bool Known(uint32_t spell) {
        return spell == s_known;
    }
}

// 0x5E4B70: the client's value times the realm's first rate, truncated.
COA_TEST(AttachLate_ScaleByRealmRate) {
    Rates(2.0f, 3.0f);
    CHECK(CoA::AttachLate::ScaleByRealmRate(10.0f) == 20);
    CHECK(CoA::AttachLate::ScaleByRealmRate(2.75f) == 5);
    // Before SMSG_REALM_INFO the rate is 0, as the realm object starts.
    CoA::RealmInfo::Reset();
    CHECK(CoA::AttachLate::ScaleByRealmRate(10.0f) == 0);
}

// 0x54A370: win honor x rate1, win arena x rate2, loss honor x rate1, as unsigned numbers.
COA_TEST(AttachLate_RandomBgHonorScaled) {
    Rates(2.0f, 0.5f);
    double out[3];
    CoA::AttachLate::RandomBgHonor(100, 30, 25, out);
    CHECK(out[0] == 200.0 && out[1] == 15.0 && out[2] == 50.0);
    // A negative product reads back as its unsigned low half.
    CoA::AttachLate::RandomBgHonor(-1, 0, 0, out);
    CHECK(out[0] == 4294967294.0);
}

// 0x726160: true when the player knows any later rank along supercededBy.
COA_TEST(AttachLate_KnowsHigherRankWalksChain) {
    s_supercededBy = { { 1, 2 }, { 2, 3 }, { 3, 0 } };
    s_known = 3;
    CHECK(CoA::AttachLate::KnowsHigherRank(1, Lookup, Known));
    s_known = 1;
    CHECK(!CoA::AttachLate::KnowsHigherRank(1, Lookup, Known));
    CHECK(!CoA::AttachLate::KnowsHigherRank(9, Lookup, Known));
}

COA_TEST(AttachLate_SelfSupersedingSpellIsFatal) {
    s_supercededBy = { { 5, 5 } };
    s_known = 5;
    s_fatal.clear();
    CoA::AttachLate::SetFatalSink([](const char* text) { s_fatal = text; });
    CHECK(!CoA::AttachLate::KnowsHigherRank(5, Lookup, Known));
    CHECK(s_fatal == "Spell: 5 has recursive supercededBySpell in SkillLineAbility.dbc (dbc_skill_line_ability)");
    CoA::AttachLate::SetFatalSink(nullptr);
}
