#include "CoA/Source/Modules/ClientOptions.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {
    bool s_inWorld = true;
    int s_uiScaleCalls = 0;
    int s_reloads = 0;
    int s_recomputes = 0;
    float s_lootArt = -1.0f;
    float s_sizes[11][2];
    float s_other[11];
    std::vector<std::string> s_sounds;
    const char* s_trader = "Bob";

    bool InWorld() { return s_inWorld; }
    void RegisterUiScale() {
        ++s_uiScaleCalls;
        CVar::Register("useUiScale", "Whether or not the UI scale should be used", 0, "1", nullptr, 4, false, nullptr, false);
    }
    void Reload() { ++s_reloads; }
    void Recompute() { ++s_recomputes; }
    void LootArt(float v) { s_lootArt = v; }
    void Size(uint32_t i, float w, float h) { s_sizes[i][0] = w; s_sizes[i][1] = h; }
    void Other(uint32_t i, float v) { s_other[i] = v; }
    void Sound(const char* kit, float x, float y, float z) {
        char line[128];
        snprintf(line, sizeof(line), "%s %g %g %g", kit, x, y, z);
        s_sounds.push_back(line);
    }
    const char* Trader() { return s_trader; }

    void Start() {
        CoA::CVars::Reset();
        CoA::LuaReg::Reset();
        CoA::ClientOptions::Reset();
        s_inWorld = true;
        s_uiScaleCalls = s_reloads = s_recomputes = 0;
        s_lootArt = -1.0f;
        s_sounds.clear();
        s_trader = "Bob";
        CoA::ClientOptions::Deps& d = CoA::ClientOptions::Dependencies();
        d.inWorld = InWorld;
        d.registerUiScaleCVars = RegisterUiScale;
        d.reloadSelectionTexture = Reload;
        d.recomputeSelectionRadii = Recompute;
        d.setLootArtScale = LootArt;
        d.setWorldTextSize = Size;
        d.setWorldTextOther = Other;
        d.playSound = Sound;
        d.traderName = Trader;
        CoA::ClientOptions::Init();
        CoA::CVars::RegisterWorld();
    }

    void SetCVar(const char* name, const char* value) {
        CVar* var = CVar::Lookup(name);
        CHECK(var != nullptr);

        if (var) {
            var->Set(value, true, false, false, false);
        }
    }

    struct Spec {
        const char* name;
        const char* def;
        uint32_t flags;
        uint32_t category;
        bool callback;
    };

    // FUN_10a66100's FUN_10114540 calls, in order (genuine DLL 0x10A67607 .. 0x10A6B338).
    const Spec kSpecs[] = {
        { "ObjectSelectionCircleTexture", "2", 1, 4, true },
        { "ObjectSelectionCircleMode", "0", 1, 4, true },
        { "flashWindow", "0", 1, 4, false },
        { "absorbSoundOnSpellHit", "0", 1, 3, false },
        { "CombatLogMelee", "1", 0x21, 3, false },
        { "chatBubblesNameplate", "1", 1, 4, true },
        { "holdToCast", "0", 1, 3, false },
        { "hideDangerousEnemyLevel", "1", 0x21, 4, true },
        { "lootArtScale", "1.0", 1, 4, true },
        { "targetName", "1", 1, 4, true },
        { "WorldTextScale", "1.0", 1, 4, true },
        { "autoAssistCast", "0", 1, 3, true },
        { "cameraShake", "1", 1, 4, false },
        { "UnitNameGO", "0", 1, 4, false },
        { "UnitNameInteractiveNPC", "0", 1, 4, false },
        { "UnitNameQuestNPC", "1", 1, 4, false },
        { "UnitNameHostileNPC", "1", 1, 4, false },
        { "UnitNameAllNPC", "0", 1, 4, false },
        { "Sound_EnableFall", "1", 1, 7, true },
        { "autoAcceptTrades", "1", 1, 4, false },
    };
}

COA_TEST(ClientOptions_QueuesTheTwentyWorldCVarsInOrder) {
    Start();
    size_t count = 0;
    const CoA::CVarEntry* world = CoA::CVars::World(count);
    CHECK(count == sizeof(kSpecs) / sizeof(kSpecs[0]));

    for (size_t i = 0; i < count && i < sizeof(kSpecs) / sizeof(kSpecs[0]); ++i) {
        CHECK(strcmp(world[i].name, kSpecs[i].name) == 0);
        CHECK(strcmp(world[i].defaultValue, kSpecs[i].def) == 0);
        CHECK(world[i].flags == kSpecs[i].flags);
        CHECK(world[i].category == kSpecs[i].category);
        CHECK((world[i].callback != nullptr) == kSpecs[i].callback);
        CHECK(world[i].help == nullptr);
        CHECK(!world[i].setCommand);
        CHECK(world[i].slot != nullptr);
    }

    size_t glue = 0;
    CoA::CVars::Glue(glue);
    CHECK(glue == 0);
    CHECK(CVar::Lookup("autoAcceptTrades") != nullptr);
    CHECK(CVar::Lookup("ObjectSelectionCircleTexture") != nullptr);
}

// The callbacks run at registration with the defaults: arrow texture (2), bubbles over nameplates (1), the 0x89 quirk
// (autoAssistCast 0).
COA_TEST(ClientOptions_CallbacksApplyTheDefaultsAtRegistration) {
    Start();
    SetCVar("ObjectSelectionCircleTexture", "2");
    SetCVar("chatBubblesNameplate", "1");
    SetCVar("autoAssistCast", "0");
    const CoA::ClientHooks::OptionPatches& p = CoA::ClientHooks::options;
    CHECK(strcmp(p.selectionTexture, "Textures\\UnitSelectTextureArrow.blp") == 0);
    CHECK(p.bubblesOverNameplates);
    CHECK(!p.assistCastAlways);
    CHECK(p.assistCastNeedTargets == 0x89);
    CHECK(p.dangerousEnemyLevelHidden);
    CHECK(p.lockedTargetNameShown);
    CHECK(p.fallInjurySound);
}

COA_TEST(ClientOptions_SelectionTextureByValue) {
    Start();
    const char* const kExpected[][2] = {
        { "1", "Textures\\UnitSelectTextureNoFill.blp" },
        { "2", "Textures\\UnitSelectTextureArrow.blp" },
        { "3", "Textures\\UnitSelectTextureArrowNoFill.blp" },
        { "0", "Textures\\UnitSelectTexture.blp" },
        { "4", "Textures\\UnitSelectTexture.blp" },
        { "01", "Textures\\UnitSelectTexture.blp" },
    };

    for (const auto& e : kExpected) {
        s_reloads = 0;
        CHECK(CoA::ClientOptions::OnCircleTexture(nullptr, nullptr, e[0], nullptr));
        CHECK(strcmp(CoA::ClientHooks::options.selectionTexture, e[1]) == 0);
        CHECK(s_reloads == 1);
    }
}

// FUN_10a46f60: in world the value is stored first, then every visible unit's radius recomputed; at glue nothing.
COA_TEST(ClientOptions_CircleModeRecomputesInWorldOnly) {
    Start();
    CVar* mode = CVar::Lookup("ObjectSelectionCircleMode");
    CHECK(mode != nullptr);
    s_recomputes = 0;
    CHECK(CoA::ClientOptions::OnCircleMode(mode, "0", "1", nullptr));
    CHECK(s_recomputes == 1 && mode && mode->m_intValue == 1);
    s_inWorld = false;
    CHECK(CoA::ClientOptions::OnCircleMode(mode, "1", "0", nullptr));
    CHECK(s_recomputes == 1 && mode && mode->m_intValue == 1);
}

COA_TEST(ClientOptions_ByteFlagCallbacks) {
    Start();
    using CoA::ClientHooks::options;
    CoA::ClientOptions::OnChatBubblesNameplate(nullptr, nullptr, "0", nullptr);
    CHECK(!options.bubblesOverNameplates);
    CoA::ClientOptions::OnChatBubblesNameplate(nullptr, nullptr, "1", nullptr);
    CHECK(options.bubblesOverNameplates);

    CoA::ClientOptions::OnHideDangerousEnemyLevel(nullptr, nullptr, "0", nullptr);
    CHECK(!options.dangerousEnemyLevelHidden);
    CoA::ClientOptions::OnHideDangerousEnemyLevel(nullptr, nullptr, "1", nullptr);
    CHECK(options.dangerousEnemyLevelHidden);

    CoA::ClientOptions::OnTargetName(nullptr, nullptr, "2", nullptr);
    CHECK(!options.lockedTargetNameShown);
    CoA::ClientOptions::OnTargetName(nullptr, nullptr, "1", nullptr);
    CHECK(options.lockedTargetNameShown);

    CoA::ClientOptions::OnSoundEnableFall(nullptr, nullptr, "0", nullptr);
    CHECK(!options.fallInjurySound);
    CoA::ClientOptions::OnSoundEnableFall(nullptr, nullptr, "1", nullptr);
    CHECK(options.fallInjurySound);

    CoA::ClientOptions::OnAutoAssistCast(nullptr, nullptr, "1", nullptr);
    CHECK(options.assistCastAlways && options.assistCastNeedTargets == 0);
    CoA::ClientOptions::OnAutoAssistCast(nullptr, nullptr, "0", nullptr);
    CHECK(!options.assistCastAlways && options.assistCastNeedTargets == 0x89);
}

COA_TEST(ClientOptions_ClampLetsNaNThrough) {
    CHECK(CoA::ClientOptions::Clamp(0.1f, 0.25f, 3.0f) == 0.25f);
    CHECK(CoA::ClientOptions::Clamp(5.0f, 0.25f, 3.0f) == 3.0f);
    CHECK(CoA::ClientOptions::Clamp(1.5f, 0.25f, 3.0f) == 1.5f);
    CHECK(std::isnan(CoA::ClientOptions::Clamp(std::nanf(""), 0.25f, 3.0f)));
}

// FUN_10a47b70: the stock sizes (the DLL's copies at 0x10BCCED0 / 0x10BCCFF4) times the clamped scale.
COA_TEST(ClientOptions_WorldTextScale) {
    Start();
    CoA::ClientOptions::OnWorldTextScale(nullptr, nullptr, "2", nullptr);
    CHECK(s_sizes[0][0] == 2.0f * 0.018333333f && s_sizes[0][1] == 2.0f * 0.018333333f);
    CHECK(s_sizes[2][0] == 0.0f && s_sizes[2][1] == 2.0f * 0.027499998f);
    CHECK(s_sizes[8][0] == 2.0f * 0.027499998f);
    CHECK(s_other[0] == 2.0f * 0.002f && s_other[3] == 2.0f && s_other[10] == 0.0f);
    CoA::ClientOptions::OnWorldTextScale(nullptr, nullptr, "9", nullptr);
    CHECK(s_other[3] == 3.0f);
    CoA::ClientOptions::OnWorldTextScale(nullptr, nullptr, "0", nullptr);
    CHECK(s_other[3] == 0.25f);
}

COA_TEST(ClientOptions_LootArtScaleClampedZeroToFive) {
    Start();
    CoA::ClientOptions::OnLootArtScale(nullptr, nullptr, "7", nullptr);
    CHECK(s_lootArt == 5.0f);
    CoA::ClientOptions::OnLootArtScale(nullptr, nullptr, "-1", nullptr);
    CHECK(s_lootArt == 0.0f);
    CoA::ClientOptions::OnLootArtScale(nullptr, nullptr, "2.5", nullptr);
    CHECK(s_lootArt == 2.5f);
}

COA_TEST(ClientOptions_ChatBubbleHeight) {
    Start();
    float& h = CoA::ClientHooks::options.chatBubbleHeight;
    SetCVar("chatBubblesNameplate", "1");
    // No nameplate module yet: no plate -> 0.7; a plate -> the missing offset reads 0.7, + 1.0.
    CoA::ClientOptions::BeforeChatBubbleUpdate(false, false);
    CHECK(h == 0.7f);
    CoA::ClientOptions::BeforeChatBubbleUpdate(false, true);
    CHECK(h == 0.7f + 1.0f);

    CVar::Register("nameplateVerticalOffset", nullptr, 1, "0.5", nullptr, 4, false, nullptr, false);
    CVar::Register("nameplateShowPersonal", nullptr, 1, "1", nullptr, 4, false, nullptr, false);
    CVar::Register("nameplatePersonalPosition", nullptr, 1, "1", nullptr, 4, false, nullptr, false);
    SetCVar("nameplateVerticalOffset", "0.5");
    SetCVar("nameplateShowPersonal", "1");
    SetCVar("nameplatePersonalPosition", "1");
    CoA::ClientOptions::BeforeChatBubbleUpdate(false, true);
    CHECK(h == 1.5f);
    CoA::ClientOptions::BeforeChatBubbleUpdate(true, true);
    CHECK(h == 1.25f);
    SetCVar("nameplatePersonalPosition", "0");
    CoA::ClientOptions::BeforeChatBubbleUpdate(true, false);
    CHECK(h == 0.7f);
    // Personal plate off: the player is treated like anyone else.
    SetCVar("nameplateShowPersonal", "0");
    CoA::ClientOptions::BeforeChatBubbleUpdate(true, true);
    CHECK(h == 1.5f);

    SetCVar("chatBubblesNameplate", "0");
    CoA::ClientOptions::BeforeChatBubbleUpdate(false, true);
    CHECK(h == 0.7f);
}

COA_TEST(ClientOptions_ArrowMatrix) {
    Start();
    float m[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    SetCVar("ObjectSelectionCircleTexture", "1");
    CHECK(!CoA::ClientOptions::BuildArrowMatrix(m, true, 0.5f));
    SetCVar("ObjectSelectionCircleTexture", "3");
    CHECK(!CoA::ClientOptions::BuildArrowMatrix(m, false, 0.5f));
    CHECK(CoA::ClientOptions::BuildArrowMatrix(m, true, 0.5f));
    // Rz(pi) * Rz(-0.5): the rows hold cos / sin of (pi - 0.5).
    const float a = 3.14159274f - 0.5f;
    CHECK(std::fabs(m[0] - std::cos(a)) < 1e-5f && std::fabs(m[1] - std::sin(a)) < 1e-5f);
    CHECK(std::fabs(m[4] + std::sin(a)) < 1e-5f && std::fabs(m[5] - std::cos(a)) < 1e-5f);
    CHECK(m[10] == 1.0f && m[15] == 1.0f);

    float r[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    CoA::ClientOptions::RotateZ(r, 0.25f);
    CHECK(std::fabs(r[0] - std::cos(0.25f)) < 1e-6f && std::fabs(r[1] - std::sin(0.25f)) < 1e-6f);
    CHECK(std::fabs(r[4] + std::sin(0.25f)) < 1e-6f);
}

COA_TEST(ClientOptions_SelectionRadius) {
    Start();
    float radius = 0.0f;
    SetCVar("ObjectSelectionCircleMode", "0");
    CHECK(!CoA::ClientOptions::SelectionRadius(true, 1.0f, 1.0f, radius));
    SetCVar("ObjectSelectionCircleMode", "1");
    CHECK(CoA::ClientOptions::SelectionRadius(true, 1.0f, 1.0f, radius));
    CHECK(radius == 5.0f);
    CHECK(CoA::ClientOptions::SelectionRadius(true, 3.0f, 2.0f, radius));
    CHECK(radius == 3.0f + 2.0f + 1.33333337f);
    CHECK(CoA::ClientOptions::SelectionRadius(false, 3.0f, 2.0f, radius));
    CHECK(radius == 5.0f);
    // maxss(5, sum): a NaN sum is kept.
    CHECK(CoA::ClientOptions::SelectionRadius(true, std::nanf(""), 2.0f, radius));
    CHECK(std::isnan(radius));
}

COA_TEST(ClientOptions_AbsorbSoundDamageTextShake) {
    Start();
    SetCVar("absorbSoundOnSpellHit", "1");
    CoA::ClientOptions::AfterAddWorldText(9, 1.0f, 2.0f, 3.0f);
    CHECK(s_sounds.empty());
    CoA::ClientOptions::AfterAddWorldText(10, 1.0f, 2.0f, 3.0f);
    CHECK(s_sounds.size() == 1 && s_sounds[0] == "(DONOTRENAME)AbsorbGetHit 1 2 5");
    SetCVar("absorbSoundOnSpellHit", "0");
    CoA::ClientOptions::AfterAddWorldText(10, 1.0f, 2.0f, 3.0f);
    CHECK(s_sounds.size() == 1);

    SetCVar("CombatLogMelee", "1");
    CHECK(CoA::ClientOptions::ShowDamageText(false));
    SetCVar("CombatLogMelee", "0");
    CHECK(!CoA::ClientOptions::ShowDamageText(false));
    CHECK(CoA::ClientOptions::ShowDamageText(true));

    SetCVar("cameraShake", "1");
    CHECK(CoA::ClientOptions::ShakeAllowed());
    SetCVar("cameraShake", "0");
    CHECK(!CoA::ClientOptions::ShakeAllowed());
}

COA_TEST(ClientOptions_TradeRequest) {
    Start();
    CoATest::RecordSignals();
    SetCVar("autoAcceptTrades", "1");
    CHECK(!CoA::ClientOptions::HandleTradeProposal(1));
    SetCVar("autoAcceptTrades", "0");
    CHECK(!CoA::ClientOptions::HandleTradeProposal(2));
    CHECK(CoATest::Signals().empty());
    CHECK(CoA::ClientOptions::HandleTradeProposal(1));
    CHECK(CoATest::Signals().size() == 1 && CoATest::Signals()[0] == "TRADE_REQUEST:Bob");
    s_trader = nullptr;
    CHECK(!CoA::ClientOptions::HandleTradeProposal(1));
    CoATest::StopRecordingSignals();
}

// FUN_10a6b890 on the glue list: the client's UI-scale registration runs once the glue state has its natives, and
// InitializeFrameXML's own call (0x52ADCC) is NOPed.
COA_TEST(ClientOptions_UiScaleCVarsOnTheGlueState) {
    Start();
    CHECK(!CoA::ClientHooks::options.frameXmlRegistersUiScale);
    CHECK(s_uiScaleCalls == 0);
    CoA::LuaReg::RunGlueRegistered();
    CHECK(s_uiScaleCalls == 1);
    CHECK(CVar::Lookup("useUiScale") != nullptr);
    CoA::LuaReg::RunWorldRegistered();
    CHECK(s_uiScaleCalls == 1);
    CoA::ClientOptions::Reset();
    CHECK(CoA::ClientHooks::options.frameXmlRegistersUiScale);
}
