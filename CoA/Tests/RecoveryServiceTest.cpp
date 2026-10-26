// C_RecoveryService (RecoveryService.cpp): every query / recover CMSG byte-exact, the six response formats and their
// records, the view filled only by UpdateFilter, the item / character searches, the recover position (all list) and
// the result's view-index bug, the refilter's token-stripped text, short packets.
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/RecoveryService.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;

    const char* const kDisenchanted = "RECOVERY_SERVICE_CATEGORY_DISENCHANTED_ITEM";
    const char* const kCharacter = "RECOVERY_SERVICE_CATEGORY_DELETED_CHARACTER";
    const char* const kWorldforged = "RECOVERY_SERVICE_CATEGORY_WORLDFORGED_ITEM";

    // Strings: 1 "Sharp Axe", 11 "Cuts deep", 21 "Plain Hat".
    const std::string kStrings = std::string("Sharp Axe") + '\0' + "Cuts deep" + '\0' + "Plain Hat" + '\0';

    std::vector<uint32_t> ItemAddonRow(uint32_t rowId, uint32_t item, uint32_t name, uint32_t description, uint32_t quality) {
        std::vector<uint32_t> row = { rowId, item };
        std::vector<uint32_t> n = CoATest::Loc(name);
        std::vector<uint32_t> d = CoATest::Loc(description);
        row.insert(row.end(), n.begin(), n.end());
        row.insert(row.end(), d.begin(), d.end());
        std::vector<uint32_t> fields(12, 0);
        fields[0] = quality;
        row.insert(row.end(), fields.begin(), fields.end());
        return row;
    }

    bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "ItemAddon.dbc")) {
            out = CoATest::Wdbc({ ItemAddonRow(1, 100, 1, 11, 4), ItemAddonRow(2, 200, 21, 0, 1), ItemAddonRow(3, 400, 21, 0, 2) }, kStrings);
            return true;
        }

        return false;
    }

    // Item.dbc rows: 100 an epic two-handed axe, 200 a cloth head piece, 300 (no ItemAddon record) a potion; 400 none.
    bool ItemRow(uint32_t entry, CoA::RecoveryService::ItemRow& out) {
        switch (entry) {
        case 100: out = { 2, 1, 17 }; return true;
        case 200: out = { 4, 1, 1 }; return true;
        case 300: out = { 0, 1, 0 }; return true;
        default: return false;
        }
    }

    const char* RaceName(uint32_t id) {
        return id == 10 ? "Blood Elf" : id == 2 ? "Orc" : nullptr;
    }

    const char* ClassName(uint32_t id) {
        return id == 8 ? "Mage" : id == 1 ? "Warrior" : nullptr;
    }

    lua_State* Start(bool deps = true) {
        CoA::Dbc::SetFileReader(ReadTables);
        CoA::BridgeStore::Reset();
        CoA::BridgeStore::LoadTables();
        CoA::BridgeStore::BuildIndexes();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::RecoveryService::Reset();
        CoA::RecoveryService::Init();

        if (deps) {
            CoA::RecoveryService::Deps& d = CoA::RecoveryService::Dependencies();
            d.itemRow = ItemRow;
            d.raceName = RaceName;
            d.className = ClassName;
        }

        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        CoATest::StopRecordingPackets();
        CoATest::StopRecordingSignals();
        CoA::RecoveryService::Reset();
        CoA::Packets::Clear();
        CoA::BridgeStore::Reset();
    }

    V Dispatch(uint32_t opcode, CoATest::Bytes& b) {
        CoATest::RecordSignals();
        CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    V Run(lua_State* L, const std::string& chunk) {
        V out;
        const int base = lua_gettop(L);

        if (luaL_loadstring(L, chunk.c_str()) != 0 || lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
            out.push_back(std::string("<error> ") + lua_tostring(L, -1));
            lua_settop(L, base);
            return out;
        }

        for (int i = base + 1; i <= lua_gettop(L); ++i) {
            out.push_back(lua_isnil(L, i) ? "nil" : lua_isboolean(L, i) ? (lua_toboolean(L, i) ? "true" : "false")
                : lua_istable(L, i) ? "table" : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }

    Packets Sent(lua_State* L, const std::string& chunk, V* results = nullptr) {
        CoATest::RecordPackets();
        V r = Run(L, chunk);
        CoATest::StopRecordingPackets();

        if (results) {
            *results = r;
        }

        return CoATest::SentPackets();
    }

    // A table as "k=v" pairs in key order, nested tables as "k={...}" (no table library in the test state).
    const std::string kDump =
        "local function dump(t) local keys = {} for k in pairs(t) do local i = #keys + 1 keys[i] = k "
        "while i > 1 and tostring(keys[i - 1]) > tostring(keys[i]) do keys[i - 1], keys[i] = keys[i], keys[i - 1] i = i - 1 end end "
        "local out = '' for i, k in ipairs(keys) do local v = t[k] if i > 1 then out = out .. ' ' end "
        "out = out .. tostring(k) .. '=' .. (type(v) == 'table' and ('{' .. dump(v) .. '}') or tostring(v)) end "
        "return out end ";

    // The Ids of a category's view, comma-joined.
    std::string Ids(lua_State* L, const char* category) {
        const V r = Run(L, std::string("local c = '") + category + "' local s = '' for i = 1, C_RecoveryService.GetNumItemsInCategory(c) do "
            "s = s .. (i > 1 and ',' or '') .. C_RecoveryService.GetCategoryItemAtIndex(c, i).Id end return s");
        return r.empty() ? "<none>" : r[0];
    }

    struct Cost {
        std::vector<std::pair<uint32_t, uint32_t>> items;
        std::vector<std::pair<std::string, std::pair<uint32_t, uint32_t>>> tokens;
    };

    void Tail(CoATest::Bytes& b, uint8_t recovered, uint64_t date, uint64_t money, const Cost& c) {
        b.U8(recovered).U64(date).U64(money).U32(static_cast<uint32_t>(c.items.size()));

        for (const auto& i : c.items) {
            b.U32(i.first).U32(i.second);
        }

        b.U32(static_cast<uint32_t>(c.tokens.size()));

        for (const auto& t : c.tokens) {
            b.Str(t.first.c_str()).U32(t.second.first).U32(t.second.second);
        }
    }

    void Item(CoATest::Bytes& b, const char* id, uint32_t entry, uint32_t count, uint64_t money = 0, const Cost& c = {},
        uint8_t recovered = 0, uint64_t date = 1700000000) {
        b.Str(id).U32(entry).U32(count).Str("extra").U32(9);
        Tail(b, recovered, date, money, c);
    }

    void Character(CoATest::Bytes& b, const char* id, const char* name, const char* race, const char* sex, const char* cls,
        uint32_t level, uint64_t money = 0) {
        b.Str(id).Str(name).Str(race).Str(sex).Str(cls).U32(level);
        Tail(b, 0, 5, money, {});
    }

    V Items(uint32_t opcode, const std::vector<std::string>& ids, const std::vector<uint32_t>& entries) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(ids.size()));

        for (size_t i = 0; i < ids.size(); ++i) {
            Item(b, ids[i].c_str(), entries[i], 1, 10);
        }

        return Dispatch(opcode, b);
    }

    std::vector<uint8_t> Recover(uint32_t opcode, uint32_t position, const char* text) {
        CoATest::Bytes b;
        b.U32(position).Str(text);
        return CoATest::Expected(opcode, b);
    }

    V Result(uint32_t opcode, const char* result) {
        CoATest::Bytes b;
        b.Str(result);
        return Dispatch(opcode, b);
    }
}

// QueryCategory: CMSG with no body per category, true; MAX false and nothing sent; NONE / unknown / a non-string nothing.
COA_TEST(RecoveryService_QueryCategory) {
    lua_State* L = Start();
    const char* names[6] = { "DISENCHANTED_ITEM", "DELETED_CHARACTER", "VENDORED_ITEM", "DELETED_ITEM", "MYTHIC_RECYCLING",
        "WORLDFORGED_ITEM" };
    const uint32_t opcodes[6] = { 0x5D2, 0x5DA, 0x5DE, 0x5E2, 0x5E6, 0x682 };

    for (int i = 0; i < 6; ++i) {
        V r;
        const Packets p = Sent(L, std::string("return C_RecoveryService.QueryCategory('RECOVERY_SERVICE_CATEGORY_") + names[i] + "')", &r);
        CHECK(r == V{ "true" });
        CHECK(p == Packets{ CoATest::Expected(opcodes[i], CoATest::Bytes()) });
    }

    V r;
    CHECK(Sent(L, "return C_RecoveryService.QueryCategory('RECOVERY_SERVICE_CATEGORY_MAX')", &r).empty());
    CHECK(r == V{ "false" });
    CHECK(Sent(L, "return select('#', C_RecoveryService.QueryCategory('RECOVERY_SERVICE_CATEGORY_NONE'))", &r).empty());
    CHECK(r == V{ "0" });
    CHECK(Sent(L, "return select('#', C_RecoveryService.QueryCategory('BOGUS'))", &r).empty());
    CHECK(r == V{ "0" });
    CHECK(Sent(L, "return select('#', C_RecoveryService.QueryCategory(5))", &r).empty());
    CHECK(r == V{ "0" });
    CHECK(Run(L, "return C_RecoveryService.GetNumItemsInCategory('RECOVERY_SERVICE_CATEGORY_MAX'), "
        "select('#', C_RecoveryService.GetNumItemsInCategory('RECOVERY_SERVICE_CATEGORY_NONE'))") == (V{ "0", "0" }));
    Stop(L);
}

// Each response opcode loads its own category and fires RECOVERY_QUERY_RESULT; the view stays empty until UpdateFilter.
COA_TEST(RecoveryService_ResponsesAndView) {
    lua_State* L = Start();
    const uint32_t responses[5] = { 0x5D3, 0x5DF, 0x5E3, 0x5E7, 0x683 };
    const char* cats[5] = { "DISENCHANTED_ITEM", "VENDORED_ITEM", "DELETED_ITEM", "MYTHIC_RECYCLING", "WORLDFORGED_ITEM" };

    for (int i = 0; i < 4; ++i) {
        CHECK(Items(responses[i], { "A", "B" }, { 100, 200 }) == V{ std::string("RECOVERY_QUERY_RESULT:RECOVERY_SERVICE_CATEGORY_") + cats[i] });
        const std::string c = std::string("RECOVERY_SERVICE_CATEGORY_") + cats[i];
        CHECK(Ids(L, c.c_str()) == "");
        Run(L, "C_RecoveryService.UpdateFilter('" + c + "', '')");
        CHECK(Ids(L, c.c_str()) == "A,B");
    }

    CoATest::Bytes ch;
    ch.U32(1);
    Character(ch, "C1", "Bob", "RACE_ORC", "GENDER_MALE", "CLASS_WARRIOR", 60);
    CHECK(Dispatch(0x5DB, ch) == V{ "RECOVERY_QUERY_RESULT:RECOVERY_SERVICE_CATEGORY_DELETED_CHARACTER" });
    Run(L, std::string("C_RecoveryService.UpdateFilter('") + kCharacter + "', '')");
    CHECK(Ids(L, kCharacter) == "C1");
    CHECK(Ids(L, kDisenchanted) == "A,B");   // the other categories are untouched

    // A new response resets the category: view empty again until the next UpdateFilter.
    Items(0x5D3, { "Z" }, { 100 });
    CHECK(Ids(L, kDisenchanted) == "");
    Run(L, std::string("C_RecoveryService.UpdateFilter('") + kDisenchanted + "', '')");
    CHECK(Ids(L, kDisenchanted) == "Z");

    // An empty response: UpdateFilter leaves the (reset) view empty.
    Items(0x5D3, {}, {});
    Run(L, std::string("C_RecoveryService.UpdateFilter('") + kDisenchanted + "', '')");
    CHECK(Ids(L, kDisenchanted) == "");
    Stop(L);
}

// The records' tables: item / mythic / character / worldforged keys, the cost priority (money, items, tokens, nil).
COA_TEST(RecoveryService_RecordTables) {
    lua_State* L = Start();
    CoATest::Bytes b;
    b.U32(5);
    Item(b, "M", 100, 3, 0x1234, { { { 7, 2 } }, {} });
    Item(b, "I", 100, 1, 0, { { { 7, 2 }, { 0, 5 }, { 8, 0 }, { 7, 4 } }, { { "TOKEN_TYPE_CALLBOARD_CACHE_POINTS", { 3, 0 } } } });
    Item(b, "T", 200, 1, 0, { {}, { { "TOKEN_TYPE_CALLBOARD_CACHE_POINTS", { 6, 1 } }, { "TOKEN_TYPE_NOPE", { 1, 1 } },
        { "TOKEN_TYPE_SCROLL_OF_FORTUNE_I", { 0, 0 } } } });
    Item(b, "N", 200, 1, 0, { { { 0, 1 } }, { { "TOKEN_TYPE_SCROLL_OF_FORTUNE_I", { 0, 0 } } } });
    Item(b, "X", 100, 1, 0x8000000000000000ull, {}, 0, 0x123456789ull);   // a negative cost is not money
    Dispatch(0x5D3, b);
    Run(L, std::string("C_RecoveryService.UpdateFilter('") + kDisenchanted + "', '')");
    const std::string get = kDump + "return dump(C_RecoveryService.GetCategoryItemAtIndex('" + kDisenchanted + "', ";
    CHECK(Run(L, get + "1))") == V{ "Date=1700000000 Id=M ItemCount=3 ItemEntry=100 RecoveryCost=4660" });
    CHECK(Run(L, get + "2))") == V{ "Date=1700000000 Id=I ItemCount=1 ItemEntry=100 RecoveryCost={7=4}" });
    CHECK(Run(L, get + "3))") == V{ "Date=1700000000 Id=T ItemCount=1 ItemEntry=200 RecoveryCost={TOKEN_TYPE_CALLBOARD_CACHE_POINTS=6}" });
    CHECK(Run(L, get + "4))") == V{ "Date=1700000000 Id=N ItemCount=1 ItemEntry=200" });
    CHECK(Run(L, get + "5))") == V{ "Date=591751049 Id=X ItemCount=1 ItemEntry=100" });
    CHECK(Run(L, std::string("return C_RecoveryService.GetCategoryItemAtIndex('") + kDisenchanted + "', 6), "
        "select('#', C_RecoveryService.GetCategoryItemAtIndex('" + kDisenchanted + "', 0)), "
        "select('#', C_RecoveryService.GetCategoryItemAtIndex('BOGUS', 1)), "
        "C_RecoveryService.GetCategoryItemAtIndex('RECOVERY_SERVICE_CATEGORY_MAX', 1), "
        "select('#', C_RecoveryService.GetCategoryItemAtIndex(1, 1))") == (V{ "nil", "0", "0", "nil", "0" }));

    CoATest::Bytes m;
    m.U32(1);
    Item(m, "Y", 100, 2, 0, { { { 9, 1 } }, {} });
    Dispatch(0x5E7, m);
    Run(L, "C_RecoveryService.UpdateFilter('RECOVERY_SERVICE_CATEGORY_MYTHIC_RECYCLING', '')");
    CHECK(Run(L, kDump + "return dump(C_RecoveryService.GetCategoryItemAtIndex('RECOVERY_SERVICE_CATEGORY_MYTHIC_RECYCLING', 1))") ==
        V{ "Date=1700000000 Id=Y ItemCount=2 ItemEntry=100 RecoveryCosts={9=1}" });

    CoATest::Bytes c;
    c.U32(1);
    Character(c, "C1", "Bob", "RACE_ORC", "GENDER_MALE", "CLASS_WARRIOR", 60, 500);
    Dispatch(0x5DB, c);
    Run(L, std::string("C_RecoveryService.UpdateFilter('") + kCharacter + "', '')");
    CHECK(Run(L, kDump + "return dump(C_RecoveryService.GetCategoryItemAtIndex('" + kCharacter + "', 1))") ==
        V{ "Class=CLASS_WARRIOR Date=5 Id=C1 Level=60 Name=Bob Race=RACE_ORC RecoveryCost=500 Sex=GENDER_MALE" });

    CoATest::Bytes w;
    w.U32(2);
    w.Str("W1").U32(100).U32(200).U32(3).Str("x").U32(1).U8(0).U64(77).U32(4);
    w.Str("W2").U32(200).U32(100).U32(1).Str("x").U32(1).U8(0).U64(78).U32(0);
    Dispatch(0x683, w);
    Run(L, std::string("C_RecoveryService.UpdateFilter('") + kWorldforged + "', '')");
    CHECK(Run(L, kDump + "return dump(C_RecoveryService.GetCategoryItemAtIndex('" + kWorldforged + "', 1))") ==
        V{ "AdditionalRefund={375250=4} Date=77 Id=W1 ItemCount=3 RecoveryCosts={100=3} RefundedItemId=200 RequiredItemId=100" });
    CHECK(Run(L, kDump + "return dump(C_RecoveryService.GetCategoryItemAtIndex('" + kWorldforged + "', 2))") ==
        V{ "AdditionalRefund={} Date=78 Id=W2 ItemCount=1 RecoveryCosts={200=1} RefundedItemId=100 RequiredItemId=200" });
    Stop(L);
}

// The item search: name, description, quality, class, subclass, inventory type; no Item.dbc row or no ItemAddon record
// never matches (even with no text); a $token rejects everything; recovered records never show.
COA_TEST(RecoveryService_ItemSearch) {
    lua_State* L = Start();
    CoATest::Bytes b;
    b.U32(5);
    Item(b, "AXE", 100, 1);
    Item(b, "HAT", 200, 1);
    Item(b, "POT", 300, 1);    // Item.dbc row, no ItemAddon record
    Item(b, "GONE", 400, 1);   // ItemAddon record, no Item.dbc row
    Item(b, "OLD", 100, 1, 0, {}, 1);   // recovered on the wire
    Dispatch(0x5D3, b);
    auto filter = [&](const std::string& s) {
        Run(L, std::string("C_RecoveryService.UpdateFilter('") + kDisenchanted + "', '" + s + "')");
        return Ids(L, kDisenchanted);
    };
    CHECK(filter("") == "AXE,HAT");
    CHECK(filter("SHARP") == "AXE");
    CHECK(filter("deep") == "AXE");
    CHECK(filter("epic") == "AXE");
    CHECK(filter("normal") == "HAT");
    CHECK(filter("weapon") == "AXE");
    CHECK(filter("axe2") == "AXE");   // ITEM_SUBCLASS_WEAPON_AXE2 -> "weapon axe2"
    CHECK(filter("weapon_axe2") == "AXE");   // '_' in the text is a space too
    CHECK(filter("2hweapon") == "AXE");
    CHECK(filter("cloth") == "HAT");
    CHECK(filter("head") == "HAT");
    CHECK(filter("armor") == "HAT");
    CHECK(filter("nothing") == "");
    CHECK(filter("item") == "");     // every token's prefix is dropped
    CHECK(filter("  plain   hat ") == "HAT");   // the engine trims and collapses spaces
    CHECK(filter("$q4") == "");                 // any token empties the view
    CHECK(filter("$q4 axe") == "");
    CHECK(filter("$q4x axe") == "AXE");         // not a token: the word is removed

    // No Item.dbc dependency: nothing matches.
    CoA::RecoveryService::Dependencies().itemRow = nullptr;
    CHECK(filter("") == "");
    Stop(L);
}

// The character search: name, race name (ChrRaces), sex without GENDER_, class name (ChrClasses), level; unknown race /
// class tokens read row 0, missing rows read "none".
COA_TEST(RecoveryService_CharacterSearch) {
    lua_State* L = Start();
    CoATest::Bytes b;
    b.U32(3);
    Character(b, "1", "Arthas", "RACE_BLOODELF", "GENDER_MALE", "CLASS_MAGE", 80);
    Character(b, "2", "Thrall", "RACE_ORC", "GENDER_FEMALE", "CLASS_WARRIOR", 12);
    Character(b, "3", "Nobody", "RACE_GOBLIN", "SOMETHING_ELSE", "CLASS_PYROMANCER", 5);
    Dispatch(0x5DB, b);
    auto filter = [&](const std::string& s) {
        Run(L, std::string("C_RecoveryService.UpdateFilter('") + kCharacter + "', '" + s + "')");
        return Ids(L, kCharacter);
    };
    CHECK(filter("") == "1,2,3");
    CHECK(filter("ARTH") == "1");
    CHECK(filter("blood elf") == "1");
    CHECK(filter("orc") == "2");
    CHECK(filter("female") == "2");
    CHECK(filter("male") == "1,2");
    CHECK(filter("something else") == "3");
    CHECK(filter("gender") == "");   // GENDER_ is dropped from the sex
    CHECK(filter("mage") == "1");
    CHECK(filter("warrior") == "2");
    CHECK(filter("none") == "3");    // no ChrRaces / ChrClasses row for ids 0 / 24
    CHECK(filter("8") == "1");       // level 80
    CHECK(filter("12") == "2");
    CHECK(filter("pyromancer") == "");
    Stop(L);
}

// Recover: {u32 position in the all list, str text}, true; the view index -> all-list position; index 0, a bad
// category, a missing entry, a wrong argument set.
COA_TEST(RecoveryService_RecoverPacket) {
    lua_State* L = Start();
    Items(0x5D3, { "A", "B", "C" }, { 100, 200, 100 });
    const std::string cat = std::string("'") + kDisenchanted + "'";
    V r;
    CHECK(Sent(L, "return C_RecoveryService.RecoverCategoryItemAtIndex(" + cat + ", 1, 'x')", &r).empty());   // view empty
    CHECK(r == V{ "false" });
    Run(L, "C_RecoveryService.UpdateFilter(" + cat + ", 'hat')");   // view: B (position 1)
    CHECK(Sent(L, "return C_RecoveryService.RecoverCategoryItemAtIndex(" + cat + ", 1, 'please')", &r) ==
        Packets{ Recover(0x5D4, 1, "please") });
    CHECK(r == V{ "true" });
    Run(L, "C_RecoveryService.UpdateFilter(" + cat + ", 'axe')");   // view: A, C (positions 0, 2)
    CHECK(Sent(L, "return C_RecoveryService.RecoverCategoryItemAtIndex(" + cat + ", 2, '')", &r) == Packets{ Recover(0x5D4, 2, "") });
    CHECK(Sent(L, "return C_RecoveryService.RecoverCategoryItemAtIndex(" + cat + ", 3, 'x')", &r).empty());
    CHECK(r == V{ "false" });
    CHECK(Sent(L, "return select('#', C_RecoveryService.RecoverCategoryItemAtIndex(" + cat + ", 0, 'x'))", &r).empty());
    CHECK(r == V{ "0" });
    CHECK(Sent(L, "return C_RecoveryService.RecoverCategoryItemAtIndex('RECOVERY_SERVICE_CATEGORY_MAX', 1, 'x')", &r).empty());
    CHECK(r == V{ "false" });
    CHECK(Sent(L, "return select('#', C_RecoveryService.RecoverCategoryItemAtIndex('BOGUS', 1, 'x'))", &r).empty());
    CHECK(r == V{ "0" });
    // A number passes a string slot (ValidateInput) and is sent as its text.
    CHECK(Sent(L, "return C_RecoveryService.RecoverCategoryItemAtIndex(" + cat + ", 1, 2)", &r) == Packets{ Recover(0x5D4, 0, "2") });
    CHECK(r == V{ "true" });
    CHECK(Sent(L, "return select('#', C_RecoveryService.RecoverCategoryItemAtIndex(" + cat + ", true, 'x'))", &r).empty());
    CHECK(r == V{ "0" });

    // Every category's recover opcode.
    const uint32_t responses[6] = { 0x5D3, 0x5DB, 0x5DF, 0x5E3, 0x5E7, 0x683 };
    const uint32_t recovers[6] = { 0x5D4, 0x5DC, 0x5E0, 0x5E4, 0x5E8, 0x684 };
    const char* cats[6] = { "DISENCHANTED_ITEM", "DELETED_CHARACTER", "VENDORED_ITEM", "DELETED_ITEM", "MYTHIC_RECYCLING", "WORLDFORGED_ITEM" };

    for (int i = 0; i < 6; ++i) {
        CoATest::Bytes b;
        b.U32(1);

        if (i == 1) {
            Character(b, "C", "Bob", "RACE_ORC", "GENDER_MALE", "CLASS_WARRIOR", 1);
        } else if (i == 5) {
            b.Str("W").U32(100).U32(0).U32(1).Str("").U32(0).U8(0).U64(0).U32(0);
        } else {
            Item(b, "I", 100, 1);
        }

        Dispatch(responses[i], b);
        const std::string c = std::string("'RECOVERY_SERVICE_CATEGORY_") + cats[i] + "'";
        Run(L, "C_RecoveryService.UpdateFilter(" + c + ", '')");
        CHECK(Sent(L, "return C_RecoveryService.RecoverCategoryItemAtIndex(" + c + ", 1, 'go')") == Packets{ Recover(recovers[i], 0, "go") });
    }

    Stop(L);
}

// The result: <TYPE>_RECOVERY_OK marks the pending position of the VIEW recovered (the original indexes the view with
// the all-list position) and refilters; any result fires RECOVERY_RESULT and clears the pending pair.
COA_TEST(RecoveryService_RecoverResult) {
    lua_State* L = Start();
    const std::string cat = std::string("'") + kDisenchanted + "'";
    Items(0x5D3, { "A", "B", "C" }, { 200, 100, 100 });
    Run(L, "C_RecoveryService.UpdateFilter(" + cat + ", 'axe')");   // view B, C
    Sent(L, "C_RecoveryService.RecoverCategoryItemAtIndex(" + cat + ", 1, 'x')");   // B: position 1
    CHECK(Result(0x5D5, "DISENCHANTED_ITEM_RECOVERY_OK") ==
        V{ "RECOVERY_RESULT:RECOVERY_SERVICE_CATEGORY_DISENCHANTED_ITEM,DISENCHANTED_ITEM_RECOVERY_OK" });
    CHECK(Ids(L, kDisenchanted) == "B");   // view[1] -- C -- was marked, not B
    Run(L, "C_RecoveryService.UpdateFilter(" + cat + ", '')");
    CHECK(Ids(L, kDisenchanted) == "A,B");

    // The pending pair is cleared: a second OK marks nothing.
    CHECK(Result(0x5D5, "DISENCHANTED_ITEM_RECOVERY_OK").size() == 1);
    CHECK(Ids(L, kDisenchanted) == "A,B");

    // A failure marks nothing and still fires.
    Sent(L, "C_RecoveryService.RecoverCategoryItemAtIndex(" + cat + ", 1, 'x')");
    CHECK(Result(0x5D5, "DISENCHANTED_ITEM_RECOVERY_NOT_ENOUGH_MONEY") ==
        V{ "RECOVERY_RESULT:RECOVERY_SERVICE_CATEGORY_DISENCHANTED_ITEM,DISENCHANTED_ITEM_RECOVERY_NOT_ENOUGH_MONEY" });
    CHECK(Ids(L, kDisenchanted) == "A,B");

    // Another category's OK string on this category's result does nothing; this category's OK on another category's
    // result marks the pending category.
    Sent(L, "C_RecoveryService.RecoverCategoryItemAtIndex(" + cat + ", 1, 'x')");   // A: position 0
    Result(0x5D5, "DELETED_CHARACTER_RECOVERY_OK");
    CHECK(Ids(L, kDisenchanted) == "A,B");
    Sent(L, "C_RecoveryService.RecoverCategoryItemAtIndex(" + cat + ", 1, 'x')");
    CHECK(Result(0x5DD, "DELETED_CHARACTER_RECOVERY_OK") ==
        V{ "RECOVERY_RESULT:RECOVERY_SERVICE_CATEGORY_DELETED_CHARACTER,DELETED_CHARACTER_RECOVERY_OK" });
    CHECK(Ids(L, kDisenchanted) == "B");

    // The refilter passes the last text after token extraction: "$q1 " left "" behind.
    Items(0x5D3, { "A", "B" }, { 200, 100 });
    Run(L, "C_RecoveryService.UpdateFilter(" + cat + ", 'hat')");   // view A
    Sent(L, "C_RecoveryService.RecoverCategoryItemAtIndex(" + cat + ", 1, 'x')");   // A: position 0
    Run(L, "C_RecoveryService.UpdateFilter(" + cat + ", '$q1')");   // view empty
    CHECK(Ids(L, kDisenchanted) == "");
    Result(0x5D5, "DISENCHANTED_ITEM_RECOVERY_OK");   // nothing at view[0]; refilter with ""
    CHECK(Ids(L, kDisenchanted) == "A,B");

    // A result with nothing pending only fires.
    CHECK(Result(0x685, "WORLDFORGED_ITEM_RECOVERY_OK") ==
        V{ "RECOVERY_RESULT:RECOVERY_SERVICE_CATEGORY_WORLDFORGED_ITEM,WORLDFORGED_ITEM_RECOVERY_OK" });
    const uint32_t results[6] = { 0x5D5, 0x5DD, 0x5E1, 0x5E5, 0x5E9, 0x685 };

    for (uint32_t op : results) {
        CHECK(Result(op, "R").size() == 1);
    }

    Stop(L);
}

// Short packets: counts stop at the packet's end; missing fields read 0 / "".
COA_TEST(RecoveryService_ShortPackets) {
    lua_State* L = Start();
    CoATest::Bytes b;
    b.U32(1000);
    Item(b, "A", 100, 1);
    CHECK(Dispatch(0x5D3, b).size() == 1);
    Run(L, std::string("C_RecoveryService.UpdateFilter('") + kDisenchanted + "', '')");
    CHECK(Ids(L, kDisenchanted) == "A");

    CoATest::Bytes c;
    c.U32(2).Str("A").U32(100);   // cut inside the first record
    Dispatch(0x5D3, c);
    Run(L, std::string("C_RecoveryService.UpdateFilter('") + kDisenchanted + "', '')");
    CHECK(Run(L, kDump + "return dump(C_RecoveryService.GetCategoryItemAtIndex('" + kDisenchanted + "', 1))") ==
        V{ "Date=0 Id=A ItemCount=0 ItemEntry=100" });
    CHECK(Run(L, std::string("return C_RecoveryService.GetNumItemsInCategory('") + kDisenchanted + "')") == V{ "1" });

    // A character always matches an empty search: the count stops at the end, no zero records follow.
    CoATest::Bytes ch;
    ch.U32(1000);
    Character(ch, "C", "Bob", "RACE_ORC", "GENDER_MALE", "CLASS_WARRIOR", 1);
    Dispatch(0x5DB, ch);
    Run(L, std::string("C_RecoveryService.UpdateFilter('") + kCharacter + "', '')");
    CHECK(Run(L, std::string("return C_RecoveryService.GetNumItemsInCategory('") + kCharacter + "')") == V{ "1" });

    CoATest::Bytes e;
    CHECK(Dispatch(0x5D3, e).size() == 1);
    CoATest::Bytes r;
    CHECK(Dispatch(0x5D5, r) == V{ "RECOVERY_RESULT:RECOVERY_SERVICE_CATEGORY_DISENCHANTED_ITEM," });
    Stop(L);
}
