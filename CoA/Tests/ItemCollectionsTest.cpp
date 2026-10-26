// AscItemCollections.cpp (ItemCollections.cpp): the collections attached per item slot (paths, link, scale, the HD
// columns), the gates (race, sex, the dress-up calls, SetModelApplyComponents), the model ids (counter, release), slot
// removal unless re-added, SMSG 0x56A and ReloadItemDisplayInfoCollections.
#include "CoA/Source/Modules/ItemCollections.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmData.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    struct FakeModel {
        int n;
        std::vector<FakeModel*> children;
    };

    struct FakeComponent {
        FakeModel* model;
        uint32_t race;
        uint32_t sex;
    };

    std::vector<std::unique_ptr<FakeModel>> s_models;
    std::vector<std::string> s_calls;
    const uint32_t kRec = 0x1234;   // stands for the ItemDisplayInfoRec pointer

    FakeModel* NewModel() {
        s_models.push_back(std::make_unique<FakeModel>(FakeModel{ static_cast<int>(s_models.size()), {} }));
        return s_models.back().get();
    }

    // Strings: 1 "Belt.mdx", 10 "Cape01.m2", 20 "CapeTex.blp", 32 "New.mdx".
    const std::string kStrings = std::string("Belt.mdx") + '\0' + "Cape01.m2" + '\0' + "CapeTex.blp" + '\0' + "New.mdx" + '\0';

    uint32_t Bits(float f) {
        uint32_t v;
        memcpy(&v, &f, 4);
        return v;
    }

    bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        if (!strstr(path, "ItemDisplayInfoCollections.dbc")) {
            return false;
        }

        out = CoATest::Wdbc({ { 1, 77, 15, 16, 1, 0, Bits(1.5f), Bits(2.5f) }, { 2, 77, 17, 18, 10, 20, 0, 0 },
            { 3, 88, 19, 19, 0, 0, 0, 0 } }, kStrings);
        return true;
    }

    lua_State* Start() {
        CoA::Dbc::SetFileReader(ReadTables);
        CoA::Dbc::Get("DBFilesClient\\ItemDisplayInfoCollections.dbc").Unload();
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::ItemCollections::Reset();
        CoA::ItemCollections::Init();
        CoA::RealmData::GetState().hdPatch = false;
        s_models.clear();
        s_calls.clear();
        CoA::ItemCollections::Deps& d = CoA::ItemCollections::Dependencies();
        d.componentModel = [](void* c) -> void* { return static_cast<FakeComponent*>(c)->model; };
        d.componentRace = [](void* c) { return static_cast<FakeComponent*>(c)->race; };
        d.componentSex = [](void* c) { return static_cast<FakeComponent*>(c)->sex; };
        d.children = [](void* m, std::vector<void*>& out) {
            for (FakeModel* c : static_cast<FakeModel*>(m)->children) {
                out.push_back(c);
            }
        };
        d.detach = [](void* m) {
            for (auto& p : s_models) {
                auto& v = p->children;
                v.erase(std::remove(v.begin(), v.end(), static_cast<FakeModel*>(m)), v.end());
            }

            s_calls.push_back("detach:" + std::to_string(static_cast<FakeModel*>(m)->n));
        };
        // CCharacterComponent::AddLink: CreateModel (the model hook), attach to the parent.
        d.addLink = [](void* parent, uint32_t link, const char* model, const char* texture, const void* rec) {
            s_calls.push_back("link:" + std::to_string(link) + ":" + model + ":" + texture + (rec == reinterpret_cast<const void*>(kRec) ? "" : ":?"));
            FakeModel* m = NewModel();
            CoA::ItemCollections::ModelCreated(m);
            static_cast<FakeModel*>(parent)->children.push_back(m);
        };
        d.racePrefix = [](uint32_t race) { return race == 1 ? "Human" : race == 2 ? "Orc" : static_cast<const char*>(nullptr); };
        d.scaleModel = [](void* m, float scale) { s_calls.push_back("scale:" + std::to_string(static_cast<FakeModel*>(m)->n) + ":" + std::to_string(scale)); };
        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        CoA::ItemCollections::Reset();
        CoA::RealmData::GetState().hdPatch = false;
        CoA::Dbc::Get("DBFilesClient\\ItemDisplayInfoCollections.dbc").Unload();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::Packets::Clear();
        CoA::LuaReg::Reset();
        s_models.clear();
    }

    void Attach(FakeComponent& c, uint32_t slot, uint32_t display, bool rec = true) {
        CoA::ItemCollections::Attach(&c, slot, rec ? reinterpret_cast<const void*>(kRec) : nullptr, display);
    }

    V Take() {
        V v = s_calls;
        s_calls.clear();
        return v;
    }
}

// FUN_102cbfe0: each collection of the display: "Item\ObjectComponents\<folder>\<model>" (the waist slot keeps the
// name), the texture from +0x14 or else from the model name, the link from +8, the scale (+0x18) on the created model,
// the model's id recorded for the slot. The pending scale starts at 1.0, so the very first model created gets it.
COA_TEST(ItemCollections_Attach) {
    lua_State* L = Start();
    CHECK(CoA::ItemCollections::PendingScale() == 1.0f);
    FakeModel* first = NewModel();
    CoA::ItemCollections::ModelCreated(first);
    CHECK(Take() == V{ "scale:0:1.000000" });
    CHECK(CoA::ItemCollections::ModelId(first) == 1 && CoA::ItemCollections::PendingScale() == 0.0f);
    FakeComponent c{ NewModel(), 1, 0 };
    Attach(c, 4, 77);
    CHECK(Take() == (V{ "link:15:Item\\ObjectComponents\\Waist\\Belt.mdx:Item\\ObjectComponents\\Waist\\Belt.blp", "scale:2:1.500000",
                         "link:17:Item\\ObjectComponents\\Waist\\Cape01.m2:Item\\ObjectComponents\\Waist\\CapeTex.blp" }));
    CHECK((CoA::ItemCollections::AttachedIds(&c, 4) == std::vector<uint64_t>{ 3, 5 }));
    CHECK(c.model->children.size() == 2);
    // A model created outside an attach gets an id but is not captured.
    FakeModel* other = NewModel();
    CoA::ItemCollections::ModelCreated(other);
    CHECK(CoA::ItemCollections::ModelId(other) == 7 && Take().empty());
    // Attaching the slot again first detaches its previous collection models.
    Attach(c, 4, 88);
    CHECK(Take() == (V{ "detach:2", "detach:3", "link:19::" }));   // an empty model name: empty paths
    CHECK((CoA::ItemCollections::AttachedIds(&c, 4) == std::vector<uint64_t>{ 9 }));
    // A display without collections only detaches.
    Attach(c, 4, 99);
    CHECK(Take() == V{ "detach:5" });
    CHECK(CoA::ItemCollections::AttachedIds(&c, 4).empty());
    Stop(L);
}

// FUN_102cc980: per-race models (the name loses its last four characters: "Cape01.m2" becomes "Cape0") (any slot but the waist, with a race, sex 0 / 1 / other) are
// "<name minus extension>_<ChrRaces prefix>_<M|F|>.mdx"; without the ChrRaces row the model path is empty.
COA_TEST(ItemCollections_RacePaths) {
    lua_State* L = Start();
    CoA::ItemCollections::ModelCreated(NewModel());   // the initial 1.0 scale
    FakeComponent c{ NewModel(), 1, 1 };
    Attach(c, 0, 77);
    Take();
    CHECK(s_calls.empty());
    c.sex = 0;
    Attach(c, 10, 88);
    CHECK(Take() == (V{ "link:19::" }));
    Attach(c, 1, 77);
    V calls = Take();
    CHECK(calls.size() == 3 && calls[0] == "link:15:Item\\ObjectComponents\\Shoulder\\Belt_Human_M.mdx:Item\\ObjectComponents\\Shoulder\\Belt.blp");
    CHECK(calls[2] == "link:17:Item\\ObjectComponents\\Shoulder\\Cape0_Human_M.mdx:Item\\ObjectComponents\\Shoulder\\CapeTex.blp");
    c.sex = 1;
    c.race = 2;
    Attach(c, 2, 77);
    calls = Take();
    CHECK(calls.size() == 3 && calls[0] == "link:15:Item\\ObjectComponents\\Collections\\Belt_Orc_F.mdx:Item\\ObjectComponents\\Collections\\Belt.blp");
    c.sex = 3;
    Attach(c, 10, 77);
    calls = Take();
    CHECK(calls.size() >= 2 && calls.back() == "link:17:Item\\ObjectComponents\\Cape\\Cape0_Orc_.mdx:Item\\ObjectComponents\\Cape\\CapeTex.blp");
    c.race = 5;   // no ChrRaces row
    Attach(c, 10, 77);
    calls = Take();
    CHECK(calls.size() >= 2 && calls.back() == "link:17::Item\\ObjectComponents\\Cape\\CapeTex.blp");
    // The tabard slot's folder is "".
    c.race = 1;
    c.sex = 0;
    Attach(c, 9, 88);
    Take();
    Attach(c, 9, 77);
    calls = Take();
    CHECK(calls.size() >= 2 && calls.back() == "link:17:Item\\ObjectComponents\\\\Cape0_Human_M.mdx:Item\\ObjectComponents\\\\CapeTex.blp");
    Stop(L);
}

// The gates: a display record, a race, sex not 2, not inside both dress-up calls, SetModelApplyComponents on. The HD patch
// picks the +0xC link and the +0x1C scale.
COA_TEST(ItemCollections_Gates) {
    lua_State* L = Start();
    CoA::ItemCollections::ModelCreated(NewModel());
    Take();
    FakeComponent c{ NewModel(), 0, 0 };
    Attach(c, 4, 77);
    CHECK(Take().empty());
    c.race = 1;
    c.sex = 2;
    Attach(c, 4, 77);
    CHECK(Take().empty());
    c.sex = 0;
    Attach(c, 4, 77, false);
    CHECK(Take().empty());
    CoA::ItemCollections::SetInDressUpSetModel(true);
    Attach(c, 4, 88);
    CHECK(Take().size() == 1);   // one of the two calls is not enough
    CoA::ItemCollections::SetInDressUpTryOn(true);
    Attach(c, 4, 77);
    CHECK(Take() == V{ "detach:2" });
    CoA::ItemCollections::SetInDressUpSetModel(false);
    CoA::ItemCollections::SetInDressUpTryOn(false);
    CHECK(CoATest::Call(L, "", "SetModelApplyComponents", { "nil" }).empty());
    CHECK(!CoA::ItemCollections::ApplyComponents());
    Attach(c, 4, 77);
    CHECK(Take().empty());
    CoATest::Call(L, "", "SetModelApplyComponents", { "n:0" });   // a number is true
    CHECK(CoA::ItemCollections::ApplyComponents());
    CoA::RealmData::GetState().hdPatch = true;
    Attach(c, 4, 77);
    V calls = Take();
    CHECK(calls.size() == 3 && calls[0].rfind("link:16:", 0) == 0 && calls[1] == "scale:3:2.500000" && calls[2].rfind("link:18:", 0) == 0);
    Stop(L);
}

// FUN_102caaa0: RemoveItemBySlot drops the slot's collection models (inventory slot -> item slot, FUN_102cb240) unless
// AddItem ran meanwhile; FUN_102caed0: the last Release drops a model's id (it then no longer matches).
COA_TEST(ItemCollections_RemoveAndRelease) {
    lua_State* L = Start();
    CoA::ItemCollections::ModelCreated(NewModel());
    FakeComponent c{ NewModel(), 1, 0 };
    Attach(c, 4, 77);
    Take();
    CoA::ItemCollections::BeforeRemoveItemBySlot();
    CoA::ItemCollections::BeforeAddItem();
    CoA::ItemCollections::AfterRemoveItemBySlot(&c, 5);
    CHECK(Take().empty());
    CoA::ItemCollections::BeforeRemoveItemBySlot();
    CoA::ItemCollections::AfterRemoveItemBySlot(&c, 1);   // the neck has no item slot
    CHECK(Take().empty());
    CoA::ItemCollections::ModelReleasing(s_models[2].get(), 2);
    CHECK(CoA::ItemCollections::ModelId(s_models[2].get()) == 3);
    CoA::ItemCollections::ModelReleasing(s_models[2].get(), 1);
    CHECK(CoA::ItemCollections::ModelId(s_models[2].get()) == 0);
    CoA::ItemCollections::BeforeRemoveItemBySlot();
    CoA::ItemCollections::AfterRemoveItemBySlot(&c, 5);
    CHECK(Take() == V{ "detach:3" });   // model 2 lost its id and stays
    CHECK((CoA::ItemCollections::AttachedIds(&c, 4) == std::vector<uint64_t>{ 3 }));
    // No model on the component: nothing.
    FakeComponent bare{ nullptr, 1, 0 };
    CoA::ItemCollections::AfterRemoveItemBySlot(&bare, 5);
    // The slot mapping.
    const int32_t expected[] = { 0, -1, 1, 2, 3, 4, 5, 6, 7, 8, -1, -1, -1, -1, 10, -1, -1, -1, 9, -1 };

    for (uint32_t s = 0; s < 20; ++s) {
        CHECK(CoA::ItemCollections::SlotIndex(s) == expected[s]);
    }

    Stop(L);
}

// SMSG 0x56A patches a row (an existing id in place, read through the table at once; a new one joins at
// ReloadItemDisplayInfoCollections).
COA_TEST(ItemCollections_PatchAndReload) {
    lua_State* L = Start();
    CoA::ItemCollections::ModelCreated(NewModel());
    FakeComponent c{ NewModel(), 1, 0 };
    Attach(c, 4, 88);   // builds the map
    Take();
    auto patch = [](uint32_t id, uint32_t display, uint32_t link, const char* model, const char* texture) {
        CoATest::Bytes b;
        b.U32(id).U32(display).U32(link).U32(link).U32(0).U32(0).F32(0).F32(0).Str(model).Str(texture);
        CoATest::Deliver(CoA::ItemCollections::OnPatchCollection, 0x56A, b);
    };
    patch(3, 88, 21, "New.mdx", "");
    Attach(c, 4, 88);
    CHECK(Take() == (V{ "detach:2", "link:21:Item\\ObjectComponents\\Waist\\New.mdx:Item\\ObjectComponents\\Waist\\New.blp" }));
    // A new id past the maximum grows the index without the old maximum's row (FUN_10133ac0, kept): row 3 is gone, and
    // row 9 is not in the map until the reload.
    patch(9, 88, 22, "Extra.m2", "Extra.blp");
    Attach(c, 4, 88);
    CHECK(Take() == V{ "detach:3" });
    CHECK(CoATest::Call(L, "", "ReloadItemDisplayInfoCollections").empty());
    Attach(c, 4, 88);
    CHECK(Take() == V{ "link:22:Item\\ObjectComponents\\Waist\\Extra.m2:Item\\ObjectComponents\\Waist\\Extra.blp" });
    Stop(L);
}
