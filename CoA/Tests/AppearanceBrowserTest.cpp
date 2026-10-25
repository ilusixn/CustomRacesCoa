#include "CoA/Tests/AppearanceFixture.h"

using namespace AppearanceFixture;

namespace {
    V Page(lua_State* L, const char* apply, uint32_t page = 1) {
        return Run(L, (std::string(apply) + " local t = C_AppearanceCollection.GetCategoryAppearances(" + std::to_string(page) +
                          ") return unpack(t)").c_str());
    }

    CoATest::Bytes Outfits(const std::vector<std::pair<std::string, std::vector<uint32_t>>>& outfits) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(outfits.size()));

        for (const auto& o : outfits) {
            b.Str(o.first.c_str()).U32(static_cast<uint32_t>(o.second.size()));

            for (uint32_t a : o.second) {
                b.U32(a);
            }
        }

        return b;
    }
}

// The appearance types and categories the player's class may use; category info.
COA_TEST(AppearanceBrowser_TypesAndCategories) {
    lua_State* L = Setup();
    CHECK((Run(L, "return unpack(C_AppearanceCollection.GetAppearanceTypes())") ==
        V{ "APPEARANCE_TYPE_ITEM", "APPEARANCE_TYPE_SPELL_VISUAL", "APPEARANCE_TYPE_COSMETIC_PET", "APPEARANCE_TYPE_ITEM_SET", "APPEARANCE_TYPE_MOUNT",
            "APPEARANCE_TYPE_OUTFIT" }));
    CHECK((Run(L, "return unpack(C_AppearanceCollection.GetCategoriesForType('APPEARANCE_TYPE_ITEM'))") == V{ "1", "2" }));
    s_class = 8;
    CHECK((Run(L, "return unpack(C_AppearanceCollection.GetCategoriesForType('APPEARANCE_TYPE_ITEM'))") == V{ "7" }));
    CHECK((Run(L, "return unpack(C_AppearanceCollection.GetCategoriesForType('APPEARANCE_TYPE_OUTFIT'))") == V{ "-2" }));
    CHECK((Run(L, "return C_AppearanceCollection.GetCategoryInfo(1)") == V{ "Head", "icon_head", "APPEARANCE_TYPE_ITEM" }));
    CHECK((Run(L, "return C_AppearanceCollection.GetCategoryInfo(99)") == V{ "nil", "nil", "nil" }));
    Teardown(L);
}

// The browser lists the realm-appropriate appearances of the selected category (no thrown weapons), matches the text
// against its items' names (alternates included), filters by collected state and pages by 18.
COA_TEST(AppearanceBrowser_Filter) {
    lua_State* L = Setup();
    CHECK((Page(L, "C_AppearanceCollection.ApplyCategoryFilter(1, '', {}, {})") == V{ "100" }));
    CHECK((Run(L, "return C_AppearanceCollection.GetCategoryMaxPages()") == V{ "1" }));
    CHECK((Page(L, "C_AppearanceCollection.ApplyCategoryFilter(1, 'copper', {}, {})") == V{ "100" }));
    CHECK((Page(L, "C_AppearanceCollection.ApplyCategoryFilter(1, 'zzz', {}, {})") == V{}));
    CHECK((Page(L, "C_AppearanceCollection.ApplyCategoryFilter(1, '', { FILTER_COLLECTED = true }, {})") == V{}));
    Collect({ 100 });
    CHECK((Page(L, "C_AppearanceCollection.ApplyCategoryFilter(1, '', { FILTER_COLLECTED = true }, {})") == V{ "100" }));
    CHECK((Page(L, "C_AppearanceCollection.ApplyCategoryFilter(3, 'tier', {}, {})") == V{ "103" }));
    CHECK((Page(L, "C_AppearanceCollection.ApplyCategoryFilter(1, '', {}, {})", 2) == V{}));
    CHECK((Run(L, "return C_AppearanceCollection.GetCategoryAppearances(0)") == V{}));
    Teardown(L);
}

// Collected counts per category.
COA_TEST(AppearanceBrowser_CollectedCount) {
    lua_State* L = Setup();
    Collect({ 100, 101 });
    CHECK((Run(L, "return C_AppearanceCollection.GetCollectedCount(1)") == V{ "1", "3" }));   // 100, 102, 106 are in 1
    // Category 0 matches every empty category slot in the collected count; the total is only counted above 0.
    CHECK((Run(L, "return C_AppearanceCollection.GetCollectedCount(0)") == V{ "2", "0" }));
    Teardown(L);
}

// Outfits: the outfit view lists NEW_OUTFIT first; saving keeps the pending name until the server answers; the
// NEW_OUTFIT name is refused; deleting erases on the server's OK.
COA_TEST(AppearanceBrowser_Outfits) {
    lua_State* L = Setup();
    Current({ 0, 100, 0, 0, 0, 0, 0, 0 });
    Feed(0x69D, Outfits({ { "Tank", { 0, 101, 0 } } }));
    CHECK((Page(L, "C_AppearanceCollection.ApplyCategoryFilter(-2, '', {}, {})") == V{ "New Outfit", "Tank" }));
    CHECK((Run(L, "return unpack(C_AppearanceOutfit.GetOutfitInfo('Tank'))") == V{ "101", "0" }));
    CHECK((Run(L, "return C_AppearanceOutfit.SaveOutfit('New Outfit')") == V{ "false" }));
    CoATest::Bytes save;
    save.Str("PvP").U32(8);

    for (uint32_t a : { 0, 100, 0, 0, 0, 0, 0, 0 }) {
        save.U32(a);
    }

    CHECK((Sent(L, "return C_AppearanceOutfit.SaveOutfit('PvP')") == Packets { CoATest::Expected(0x69E, save) }));
    CHECK((Feed(0x69F, CoATest::Bytes().Str("SAVE_APPEARANCE_OUTFIT_OK")) == V{ "APPEARANCE_OUTFIT_SAVED_RESULT:true,SAVE_APPEARANCE_OUTFIT_OK" }));
    CHECK((Run(L, "return C_AppearanceOutfit.GetCurrentOutfitName()") == V{ "PvP" }));
    Run(L, "C_AppearanceOutfit.SetPendingOutfit('Tank')");
    CHECK((Run(L, "return C_AppearanceOutfit.GetCurrentOutfitName(), C_Appearance.GetPendingAppearance(1)") == V{ "Tank", "101" }));
    CHECK((Sent(L, "C_AppearanceOutfit.DeleteOutfit('Tank')") == Packets { CoATest::Expected(0x6A0, CoATest::Bytes().Str("Tank")) }));
    CHECK((Feed(0x6A1, CoATest::Bytes().Str("DELETE_APPEARANCE_OUTFIT_OK")) ==
        V{ "APPEARANCE_OUTFIT_DELETED_RESULT:true,DELETE_APPEARANCE_OUTFIT_OK" }));
    CHECK((Run(L, "return unpack(C_AppearanceOutfit.GetAppearanceOutfits())") == V{ "PvP" }));
    Teardown(L);
}

// Seasons, chapters and seasonal items (available rows only, items ordered by +0x10).
COA_TEST(AppearanceBrowser_Seasonal) {
    lua_State* L = Setup();
    CHECK((Run(L, "return unpack(C_AppearanceCollection.GetAvailableSeasons())") == V{ "1", "2" }));
    CHECK((Run(L, "return unpack(C_AppearanceCollection.GetAvailableChapters(1))") == V{ "1", "2" }));
    CHECK((Run(L, "return unpack(C_AppearanceCollection.GetAvailableChapters(0))") == V{ "1", "2" }));
    CHECK((Run(L, "return unpack(C_AppearanceCollection.GetSeasonalItems(1, 0))") == V{ "101", "100" }));
    CHECK((Run(L, "return unpack(C_AppearanceCollection.GetSeasonalItems(0, 1))") == V{ "105", "100" }));
    Teardown(L);
}

// CollectItemAppearance sends the item's GUID; an unknown item is false.
COA_TEST(AppearanceBrowser_CollectItem) {
    lua_State* L = Setup();
    CHECK((Sent(L, "return C_AppearanceCollection.CollectItemAppearance('0x10')") ==
        Packets { CoATest::Expected(0x6EA, CoATest::Bytes().U64(0xABC)) }));
    CHECK((Run(L, "return C_AppearanceCollection.CollectItemAppearance('0x11')") == V{ "false" }));
    Teardown(L);
}
