// The Build Creator link codec, and the CA natives and rules that now reach the Build Creator through CA::Deps.
#include "CoA/Source/Modules/CAFilter.h"
#include "CoA/Tests/BuildCreatorFixture.h"

using namespace BCFixture;
using namespace CoA::BuildCreator;

namespace {
    std::string Hex(const std::string& s) {
        static const char d[] = "0123456789abcdef";
        std::string out;

        for (char c : s) {
            out += d[(static_cast<uint8_t>(c) >> 4) & 0xF];
            out += d[static_cast<uint8_t>(c) & 0xF];
        }

        return out;
    }

    // The fixed vectors (raw deflate level 6 / window 15 / memLevel 8, standard base64, RFC 3986 percent-encoding),
    // computed independently with Perl's Compress::Raw::Zlib (classic zlib 1.3.2, the algorithm Boost.Beast ports) and
    // MIME::Base64. (Python's zlib-ng deflates kText1 to 26 bytes instead of 25.)
    const char* kText1 = ":5000:102t1::1001e2:1002e1:";
    const char* kDeflate1 = "b332353030b03234302a31b402520686a94620ca28d5d00a00";
    const char* kLink1 = "szI1MDCwMjQwKjG0AlIGhqlGIMoo1dAKAA%3D%3D";
    const char* kText2 = ":5000:160t2::1003e1:1001e2:";
    const char* kLink2 = "szI1MDCwMjQzKDGysjI0MDBONQRRhqlGVgA%3D";
    // ":5000:160t2::" deflates to 15 bytes; the original's 13-byte buffer keeps the first 13, which inflate to
    // ":5000:160t2:".
    const char* kLink3 = "szI1MDCwMjQzKDGysg%3D%3D";

    // Mystic enchant slots (SMSG 0x5FD: u32 n, n spells).
    void Slots(const std::vector<uint32_t>& slots) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(slots.size()));

        for (uint32_t s : slots) {
            b.U32(s);
        }

        Feed(0x5FD, b);
    }

    V PendingSpells(lua_State* L) {
        return R(L, "local b = C_BuildEditor.GetPendingBuild() local t = {} for i, s in ipairs(b.Spells) do t[#t + 1] = s.Spell .. '@' .. s.Level end "
                    "for i, c in ipairs(b.RandomEnchants) do t[#t + 1] = 'e' .. c.Enchant .. 'x' .. c.Stacks .. '@' .. c.Level end return unpack(t)");
    }
}

// Base64 (FUN_101094a0 / FUN_10109ba0): the standard alphabet with '=' padding; decoding stops at '=' or the first byte
// outside the alphabet and keeps a trailing partial group's whole bytes. URL encoding keeps RFC 3986's unreserved
// characters; decoding takes %XX when both hex characters are there (a cut "%X" at the end stays as it is).
COA_TEST(BuildCreator_Base64AndUrl) {
    CHECK(Base64Encode("") == "");
    CHECK(Base64Encode("f") == "Zg==");
    CHECK(Base64Encode("fo") == "Zm8=");
    CHECK(Base64Encode("foo") == "Zm9v");
    CHECK(Base64Encode(std::string("\xFF\xFE\x00", 3)) == "//4A");
    CHECK(Base64Decode("Zm9v") == "foo");
    CHECK(Base64Decode("Zm8=") == "fo");
    CHECK(Base64Decode("Zg==") == "f");
    CHECK(Base64Decode("Zm9vYg") == "foob");      // a partial group of 2 gives one byte
    CHECK(Base64Decode("Zm9vY") == "foo");        // of 1, none
    CHECK(Base64Decode("Zm9v!Zm9v") == "foo");    // stops at a byte outside the alphabet
    CHECK(Base64Decode("Zm=9v") == "f");          // and at '='
    CHECK(Base64Decode("//4A") == std::string("\xFF\xFE\x00", 3));
    CHECK(UrlEncode("aZ09-._~ +/=%") == "aZ09-._~%20%2B%2F%3D%25");
    CHECK(UrlDecode("a%2Bb%2fc") == "a+b/c");
    CHECK(UrlDecode("x+y") == "x+y");
    CHECK(UrlDecode("ab%3D") == "ab=");
    CHECK(UrlDecode("ab%3") == "ab%3");          // i + 2 < size fails
    CHECK(UrlDecode("ab%3Dc") == "ab=c");
    CHECK(UrlDecode("%G1z") == std::string("\0z", 2));   // >> hex reads nothing: 0
}

// The raw deflate matches the independently computed stream; a stream larger than its input is cut to the input's size
// (the original's buffer), and inflating the cut stream gives what it still holds.
COA_TEST(BuildCreator_DeflateInflate) {
    CHECK(Hex(Deflate(kText1)) == kDeflate1);
    CHECK(Inflate(Deflate(kText1)) == kText1);
    CHECK(Deflate("") == "");
    CHECK(Inflate("") == "");
    CHECK(Deflate(":5000:102t1::").size() == 13);
    CHECK(Hex(Deflate(":5000:102t1::")) == "b332353030b03234302a31b4b2");
    CHECK(Inflate(Deflate(":5000:102t1::")) == ":5000:102t1:");
    CHECK(Inflate(std::string("\xFF\xFF\xFF\xFF", 4)) == "");   // a stream error
    CHECK(UrlEncode(Base64Encode(Deflate(kText1))) == kLink1);
    CHECK(DecodeLink(kLink1) == kText1);
    CHECK(DecodeLink(kLink3) == ":5000:160t2:");
}

// C_BuildCreator.ExportBuild: ability chains as their first rank, talents as row t highest rank, "::", the enchants by
// level then quality, ":"; a known build only. ExportPendingBuild is the pending entry's link.
COA_TEST(BuildCreator_ExportLink) {
    lua_State* L = Start();
    E e = Build("b1", { { 5000, 10 }, { 5001, 20 }, { 5100, 30 } });
    e.enchants = { { 1002, 1, 2 }, { 1001, 2, 1 } };
    Deliver631(e);
    CHECK((R(L, "return C_BuildCreator.ExportBuild('b1')") == V{ kLink1 }));
    CHECK((R(L, "return C_BuildCreator.ExportBuild('nope')") == V{ "nil" }));
    CHECK((R(L, "return C_BuildCreator.ExportBuild(1)") == V{ "nil" }));   // read as "1": no such build
    CHECK((R(L, "return C_BuildCreator.ExportBuild()") == V{}));
    CHECK((R(L, "return C_BuildEditor.ExportPendingBuild()") == V{ "s7I%3D" }));   // "::" cut to 2 bytes
    R(L, "C_BuildEditor.EditBuild('b1')");
    CHECK((R(L, "return C_BuildEditor.ExportPendingBuild()") == V{ kLink1 }));
    Stop(L);
}

// C_BuildEditor.ImportBuild replaces the pending spells and enchants with the link's at the server's max level: a talent
// token adds every rank up to its count; a realm-gated enchant, a spell of another class, a talent token on an ability,
// an ability token on a talent and unknown rows are skipped; a malformed number ends the import.
COA_TEST(BuildCreator_ImportLink) {
    lua_State* L = Start();
    R(L, "C_BuildEditor.AddRandomEnchant({ Enchant = 1002, Stacks = 9 })");
    CHECK((R(L, "C_BuildEditor.ImportBuild('" + std::string(kLink1) + "')") == V{}));
    CHECK((PendingSpells(L) == V{ "5000@60", "5100@60", "e1001x2@60", "e1002x1@60" }));
    auto link = [](const std::string& text) { return UrlEncode(Base64Encode(Deflate(text))); };
    const std::string text = ":160t2:100t1:5100:999t1:12345:1003e1:1001e1:777e1:5000:0:abc:5200:";
    R(L, "C_BuildEditor.ImportBuild('" + link(text) + "')");
    // 160t2 -> 6000, 6001; 100t1: an ability; 5100: a talent's spell; 999 / 12345 / 777: unknown; 1003: no realm flag;
    // 0: no row; "abc" throws, so 5200 after it is never read.
    CHECK((PendingSpells(L) == V{ "6000@60", "6001@60", "5000@60", "e1001x1@60" }));
    CHECK((R(L, "C_BuildEditor.ImportBuild()") == V{}));
    CHECK((PendingSpells(L) == V{ "6000@60", "6001@60", "5000@60", "e1001x1@60" }));
    Stop(L);
}

// The import's class gate (FUN_101c6af0): a Mage row's spell is refused for the Warrior; without a player every row is.
COA_TEST(BuildCreator_ImportClassGate) {
    lua_State* L = Start();
    const std::string url = UrlEncode(Base64Encode(Deflate(":5000:5000:5000:102t1:102t1:102t1:")));
    CoA::CA::BuildSeed seed = SeedFromLink(url);
    CHECK(seed.spells.size() == 2);
    CHECK(seed.spells[0].first == 5000 && seed.spells[0].second == 60);
    CHECK(seed.spells[1].first == 5100);
    CAFixture::s_hasPlayer = false;
    CHECK(SeedFromLink(url).spells.empty());
    Stop(L);
}

// C_CharacterAdvancement.ExportBuild through Deps::exportBuild (FUN_100fc240): every rank of the active build's entries
// and, with includeREs, the enchant slots -- only slots 0 .. (number of non-empty slots - 1) are read, so an enchant past
// a gap is lost -- counted into stacks.
COA_TEST(BuildCreator_CAExportBuild) {
    lua_State* L = Start();
    CoA::CA::ActiveBuild()->AddEntry(100, 2);
    CoA::CA::ActiveBuild()->AddEntry(160, 2);
    std::vector<uint32_t> slots(17, 0);
    slots[0] = 1001;
    slots[1] = 1001;
    slots[2] = 1003;
    slots[16] = 1002;   // the 4th non-empty slot: slots 0..3 are read
    Slots(slots);
    CHECK((R(L, "return C_CharacterAdvancement.ExportBuild(true)") == V{ kLink2 }));
    CHECK((R(L, "return C_CharacterAdvancement.ExportBuild(false)") == V{ kLink3 }));
    CHECK(ExportBuildOf(*CoA::CA::ActiveBuild(), true) == kLink2);
    Stop(L);
}

// C_BuildEditor.ImportCurrentBuild: the active build exported and imported into the pending entry (realm-gated enchants
// dropped on the way back).
COA_TEST(BuildCreator_ImportCurrentBuild) {
    lua_State* L = Start();
    CoA::CA::ActiveBuild()->AddEntry(100, 2);
    CoA::CA::ActiveBuild()->AddEntry(160, 2);
    std::vector<uint32_t> slots(17, 0);
    slots[0] = 1001;
    slots[1] = 1001;
    slots[2] = 1003;
    Slots(slots);
    R(L, "C_BuildEditor.ImportCurrentBuild()");
    CHECK((PendingSpells(L) == V{ "5000@60", "6000@60", "6001@60", "e1001x2@60" }));
    Stop(L);
}

// C_CharacterAdvancement.ImportPendingBuild for a stock class reads a Build Creator link (Deps::seedFromLink) built at
// the player's level: the link's spells carry the server's max level, so a player below it gets none of them.
COA_TEST(BuildCreator_CAImportPendingBuild) {
    lua_State* L = Start(1, 60);
    const std::string url = UrlEncode(Base64Encode(Deflate(":5000:160t1:5000:160t1::")));
    CHECK(url == "szI1MDCwMjQzKDG0QmJaAQA%3D");
    CHECK(DecodeLink(url) == ":5000:160t1:5000:160t1::");
    CHECK((R(L, "return C_CharacterAdvancement.ImportPendingBuild('" + url + "')") == V{ "true", "nil", "nil", "nil" }));
    CHECK(CoA::CA::PendingBuild()->RankOf(100) == 1);
    CHECK(CoA::CA::PendingBuild()->RankOf(160) == 1);
    CAFixture::s_level = 59;
    CoA::CA::PendingBuild()->entries.clear();
    CHECK((R(L, "return C_CharacterAdvancement.ImportPendingBuild('" + url + "')") == V{ "true", "nil", "nil", "nil" }));
    CHECK(CoA::CA::PendingBuild()->entries.empty());
    Stop(L);
}

// C_CharacterAdvancement.ImportPendingBuildID reads a known Build Creator build (Deps::seedOfBuild) at the player's level.
COA_TEST(BuildCreator_CAImportPendingBuildID) {
    lua_State* L = Start(1, 60);
    Deliver631(Build("b1", { { 5000, 10 }, { 6000, 20 }, { 6001, 30 }, { 5100, 61 } }));
    CHECK((R(L, "return C_CharacterAdvancement.ImportPendingBuildID('b1')") == V{ "true", "nil", "nil", "nil" }));
    CHECK(CoA::CA::PendingBuild()->RankOf(100) == 1);
    CHECK(CoA::CA::PendingBuild()->RankOf(160) == 2);
    CHECK(!CoA::CA::PendingBuild()->Has(102));   // level 61 is above the player's
    CHECK((R(L, "return C_CharacterAdvancement.ImportPendingBuildID('b2')") == V{ "<error> ImportPendingBuildID: invalid buildID" }));
    CoA::CA::BuildSeed seed;
    CHECK(SeedOfBuild("b1", seed));
    CHECK(seed.id == "b1" && seed.classKey == 1 && seed.spells.size() == 4 && seed.spells[3].first == 5100 && seed.spells[3].second == 61);
    CHECK(!SeedOfBuild("b2", seed));
    Stop(L);
}

// The purge confirmations add CA_CONFIRM_UNLEARN_ALL_*_DEACTIVATE_BUILD when the active spec has a Build Creator build
// (Deps::specHasCreatorBuild, SMSG 0x632).
COA_TEST(BuildCreator_CAPurgeConfirm) {
    lua_State* L = Start();
    const std::string q = "local ok, t = C_CharacterAdvancement.ShouldConfirmUnlearnAllTalents() if not t then return ok end "
                          "local r = { ok } for i, c in ipairs(t) do r[#r + 1] = c.Error end return unpack(r)";
    const V before = R(L, q);
    CHECK((before == V{ "false" }));
    CHECK((R(L, "return C_CharacterAdvancement.ShouldConfirmUnlearnAllSpells()") == V{ "false" }));
    CHECK(SpecHasActiveBuild(0) == false);
    Feed(0x632, CoATest::Bytes().U32(0).Str("b1").U8(1).U8(0));
    CHECK(SpecHasActiveBuild(0));
    CHECK(!SpecHasActiveBuild(1));
    CHECK(!SpecHasActiveBuild(-1));
    CHECK((R(L, q) == V{ "true", "CA_CONFIRM_UNLEARN_ALL_TALENTS_DEACTIVATE_BUILD" }));
    CHECK((R(L, "local ok, t = C_CharacterAdvancement.ShouldConfirmUnlearnAllSpells() return ok, t[1].Error") ==
        V{ "true", "CA_CONFIRM_UNLEARN_ALL_ABILITIES_DEACTIVATE_BUILD" }));
    Feed(0x632, CoATest::Bytes().U32(0).Str("").U8(0).U8(0));   // an empty id is no build
    CHECK(R(L, q) == before);
    Stop(L);
}

// The C_BuildEditor entry filters ask the pending entry per spell of the row (Deps::creatorCanAddSpell / CanRemoveSpell /
// HasSpell).
COA_TEST(BuildCreator_CAEditorFilters) {
    lua_State* L = Start();
    const std::string ids = "local t = {} for i = 1, C_BuildEditor.GetNumFilteredEntries() do t[#t + 1] = C_BuildEditor.GetFilteredEntryAtIndex(i).ID end "
                            "return unpack(t)";
    R(L, "C_BuildEditor.AddSpell(" + SpellArg(5000, 10) + ")");
    R(L, "C_BuildEditor.SetFilteredEntries('', { FILTER_KNOWN = true })");
    CHECK((R(L, ids) == V{ "100" }));
    R(L, "C_BuildEditor.SetFilteredEntries('', { FILTER_CAN_REMOVE = true })");
    CHECK((R(L, ids) == V{ "100" }));
    CHECK(PendingCanRemoveSpell(5000));
    CHECK(!PendingCanRemoveSpell(5100));
    CHECK(PendingHasSpell(5000));
    CHECK(!PendingHasSpell(5001));
    CHECK(PendingCanAddSpell(5001));    // the next rank
    CHECK(!PendingCanAddSpell(5000));   // already there
    CHECK(!PendingCanAddSpell(9999));   // no row
    R(L, "C_BuildEditor.SetFilteredEntries('', { FILTER_CAN_ADD = true })");
    const V canAdd = R(L, ids);
    CHECK(std::find(canAdd.begin(), canAdd.end(), "100") != canAdd.end());   // 5001 can be added
    CHECK(std::find(canAdd.begin(), canAdd.end(), "102") != canAdd.end());
    R(L, "C_BuildEditor.SetFilteredEntries('', { FILTER_UNKNOWN = true })");
    const V unknown = R(L, ids);
    CHECK(std::find(unknown.begin(), unknown.end(), "100") == unknown.end());
    CHECK(std::find(unknown.begin(), unknown.end(), "102") != unknown.end());
    Stop(L);
}

// The BUILD_CREATOR learn (18) and unlearn (16) slots of a draft / named build: the Build Creator build of the build's name
// at the build's level must hold more ranks than the build would after learning, or fewer than it holds after unlearning;
// no such build fails both.
COA_TEST(BuildCreator_CARulesTemplate) {
    lua_State* L = Start(1, 30);
    CoA::CA::Build* b = CAFixture::NewBuild(30);
    b->u65 = 1;
    b->name = "tmpl";
    CHECK(b->ValidateLearn(0, 100, { 18 }, {}) == 18);   // no template
    Deliver631(Build("tmpl", { { 5000, 10 }, { 5001, 20 }, { 6000, 40 } }));
    CHECK(b->ValidateLearn(0, 100, { 18 }, {}) == 0);    // template rank 2 >= 0 + 1
    CHECK(b->ValidateLearn(0, 160, { 18 }, {}) == 18);   // 6000 is above level 30: rank 0
    b->AddEntry(100, 2);
    CHECK(b->ValidateLearn(0, 100, { 18 }, {}) == 18);   // 2 >= 2 + 1 fails
    CHECK(b->ValidateUnlearn(0, 100, { 16 }, {}) == 16); // template 2 < 2 fails
    b->SetRank(100, 3);
    CHECK(b->ValidateUnlearn(0, 100, { 16 }, {}) == 0);  // 2 < 3
    b->name = "other";
    CHECK(b->ValidateUnlearn(0, 100, { 16 }, {}) == 16);
    b->u65 = 0;
    CHECK(b->ValidateLearn(0, 100, { 18 }, {}) == 0);    // a plain build has no template rule
    delete b;
    Stop(L);
}
