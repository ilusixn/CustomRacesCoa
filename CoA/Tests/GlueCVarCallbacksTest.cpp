#include "CoA/Source/Modules/Cursor.h"
#include "CoA/Source/Modules/Graphics.h"
#include "CoA/Source/Modules/RaidVideo.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>

namespace {
    bool Callback(CVar*, const char*, const char*, void*) {
        return true;
    }
}

// A client module attaches its callback to a queued CVar before registration.
COA_TEST(CVars_SetCallbackReplacesQueued) {
    CoA::CVars::Reset();
    COA_CVAR("cursorSizePreferred", nullptr, 1, "0", nullptr, 1, false, nullptr, false);
    CHECK(CoA::CVars::SetCallback("cursorSizePreferred", Callback));
    CHECK(!CoA::CVars::SetCallback("noSuchCVar", Callback));
    size_t count = 0;
    const CoA::CVarEntry* glue = CoA::CVars::Glue(count);
    CHECK(count == 1 && glue[0].callback == Callback);
    CoA::CVars::Reset();
}

// FUN_1025e290 / FUN_10261d60: only "1" keeps entity shadows / the LOD doodad cull.
COA_TEST(Graphics_EntityShadowsAndLodCullOnlyOne) {
    CHECK(CoA::Graphics::OnEntityShadows(nullptr, nullptr, "0", nullptr));
    CHECK(!CoA::Graphics::EntityShadows());
    CoA::Graphics::OnEntityShadows(nullptr, nullptr, "1", nullptr);
    CHECK(CoA::Graphics::EntityShadows());
    CoA::Graphics::OnEntityShadows(nullptr, nullptr, "2", nullptr);
    CHECK(!CoA::Graphics::EntityShadows());

    CoA::Graphics::OnLodObjectCull(nullptr, nullptr, "0", nullptr);
    CHECK(!CoA::Graphics::LodObjectCull());
    CoA::Graphics::OnLodObjectCull(nullptr, nullptr, "1", nullptr);
    CHECK(CoA::Graphics::LodObjectCull());
}

// FUN_10a1e320: "1".."4" pick 48 / 64 / 96 / 128 px, anything else 32; the HD variant only with Patch-Q.
COA_TEST(Cursor_SizePreferred) {
    CoA::Cursor::SizePreferred("1", true);
    CHECK(CoA::Cursor::Size() == 48 && CoA::Cursor::Variant() == 2);
    CoA::Cursor::SizePreferred("4", true);
    CHECK(CoA::Cursor::Size() == 128 && CoA::Cursor::Variant() == 4);
    // Without Patch-Q a non-zero size keeps the previous variant.
    CoA::Cursor::SizePreferred("2", false);
    CHECK(CoA::Cursor::Size() == 64 && CoA::Cursor::Variant() == 4);
    CoA::Cursor::SizePreferred("7", true);
    CHECK(CoA::Cursor::Size() == 32 && CoA::Cursor::Variant() == 0);
    CoA::Cursor::SizePreferred("0", true);
    CHECK(CoA::Cursor::Size() == 32 && CoA::Cursor::Variant() == 0);
}

// FUN_102dcdb0: a <name>_raid CVar's base name; useRaidVideoSettings has none.
COA_TEST(RaidVideo_BaseName) {
    CHECK(CoA::RaidVideo::BaseName("farclip_raid") == "farclip");
    CHECK(CoA::RaidVideo::BaseName("fogDistance_raid") == "fogDistance");
    CHECK(CoA::RaidVideo::BaseName("useRaidVideoSettings") == "");
    CHECK(strcmp(CoA::RaidVideo::Setting(0).base, "farclip") == 0);
    CHECK(strcmp(CoA::RaidVideo::Setting(10).raidName, "animateClouds_raid") == 0);
}
