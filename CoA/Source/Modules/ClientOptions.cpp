// AscClientOptions.cpp (see ClientOptions.h): the attach init's option CVars and the hooks that read them.
#include "CoA/Source/Modules/ClientOptions.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {
    CoA::ClientOptions::Deps s_deps;

    // The CVar* slots FUN_10114090 writes at every registration.
    CVar* s_circleTexture = nullptr;    // 0x10D3D9D0
    CVar* s_circleMode = nullptr;       // 0x10D3DA14
    CVar* s_flashWindow = nullptr;      // 0x10D3D72C
    CVar* s_absorbSound = nullptr;      // 0x10D3D9D4
    CVar* s_combatLogMelee = nullptr;   // 0x10D3D9D8
    CVar* s_bubbles = nullptr;          // 0x10D3D828
    CVar* s_holdToCast = nullptr;       // 0x10D3D9FC
    CVar* s_dangerousLevel = nullptr;   // 0x10D3DA00
    CVar* s_lootArtScale = nullptr;     // 0x10D3DA10
    CVar* s_targetName = nullptr;       // 0x10D3D82C
    CVar* s_worldTextScale = nullptr;   // 0x10D3DA1C
    CVar* s_autoAssistCast = nullptr;   // 0x10D3DA20
    CVar* s_cameraShake = nullptr;      // 0x10D3DA28
    CVar* s_unitNameGO = nullptr;       // 0x10D3DA24
    CVar* s_unitNameInteractive = nullptr;   // 0x10D3DA2C
    CVar* s_unitNameQuest = nullptr;    // 0x10D3DC30
    CVar* s_unitNameHostile = nullptr;  // 0x10D3DC34
    CVar* s_unitNameAll = nullptr;      // 0x10D3DC38
    CVar* s_soundEnableFall = nullptr;  // 0x10D3DC3C
    CVar* s_autoAcceptTrades = nullptr; // 0x10D3DC44

    bool IsOne(const char* value) {
        return value && strcmp(value, "1") == 0;
    }

    int32_t Int(const CVar* var) {
        return var->m_intValue;
    }

    // The stock world-text sizes (0xAF4748 + i * 0x1C, +0x10 / +0x14) and the eleven floats from 0xAF487C (the shadow
    // offset and the first three icon corners), as the DLL copied them (0x10BCCED0 / 0x10BCCFF4).
    const float kTextSize[11][2] = {
        { 0.018333333f, 0.018333333f }, { 0.018333333f, 0.018333333f }, { 0.0f, 0.027499998f },
        { 0.018333333f, 0.018333333f }, { 0.018333333f, 0.018333333f }, { 0.018333333f, 0.018333333f },
        { 0.018333333f, 0.018333333f }, { 0.0f, 0.027499998f }, { 0.027499998f, 0.027499998f },
        { 0.027499998f, 0.027499998f }, { 0.018333333f, 0.018333333f },
    };
    const float kTextOther[11] = { 0.002f, 0.002f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f };

    // FUN_10a6b890: jmp 0x5238A0.
    void RegisterUiScaleCVars() {
        if (s_deps.registerUiScaleCVars) {
            s_deps.registerUiScaleCVars();
        }
    }
}

CoA::ClientOptions::Deps& CoA::ClientOptions::Dependencies() {
    return s_deps;
}

float CoA::ClientOptions::Clamp(float x, float lo, float hi) {
    const float low = !(lo > x) ? x : lo;
    return !(x > hi) ? low : hi;
}

bool CoA::ClientOptions::OnCircleTexture(CVar*, const char*, const char* value, void*) {
    const char*& texture = CoA::ClientHooks::options.selectionTexture;

    if (IsOne(value)) {
        texture = "Textures\\UnitSelectTextureNoFill.blp";
    } else if (strcmp(value, "2") == 0) {
        texture = "Textures\\UnitSelectTextureArrow.blp";
    } else if (strcmp(value, "3") == 0) {
        texture = "Textures\\UnitSelectTextureArrowNoFill.blp";
    } else {
        texture = "Textures\\UnitSelectTexture.blp";
    }

    if (s_deps.reloadSelectionTexture) {
        s_deps.reloadSelectionTexture();
    }

    return true;
}

bool CoA::ClientOptions::OnCircleMode(CVar* var, const char*, const char* value, void*) {
    if (s_deps.inWorld && s_deps.inWorld()) {
        var->m_intValue = atoi(value);

        if (s_deps.recomputeSelectionRadii) {
            s_deps.recomputeSelectionRadii();
        }
    }

    return true;
}

bool CoA::ClientOptions::OnChatBubblesNameplate(CVar*, const char*, const char* value, void*) {
    CoA::ClientHooks::options.bubblesOverNameplates = IsOne(value);
    return true;
}

bool CoA::ClientOptions::OnHideDangerousEnemyLevel(CVar*, const char*, const char* value, void*) {
    CoA::ClientHooks::options.dangerousEnemyLevelHidden = IsOne(value);
    return true;
}

bool CoA::ClientOptions::OnLootArtScale(CVar*, const char*, const char* value, void*) {
    const float scale = CoA::ClientOptions::Clamp(static_cast<float>(atof(value)), 0.0f, 5.0f);

    if (s_deps.setLootArtScale) {
        s_deps.setLootArtScale(scale);
    }

    return true;
}

bool CoA::ClientOptions::OnTargetName(CVar*, const char*, const char* value, void*) {
    CoA::ClientHooks::options.lockedTargetNameShown = IsOne(value);
    return true;
}

bool CoA::ClientOptions::OnWorldTextScale(CVar*, const char*, const char* value, void*) {
    const float scale = CoA::ClientOptions::Clamp(static_cast<float>(atof(value)), 0.25f, 3.0f);

    for (uint32_t i = 0; i < 11; ++i) {
        if (s_deps.setWorldTextSize) {
            s_deps.setWorldTextSize(i, scale * kTextSize[i][0], scale * kTextSize[i][1]);
        }

        if (s_deps.setWorldTextOther) {
            s_deps.setWorldTextOther(i, scale * kTextOther[i]);
        }
    }

    return true;
}

bool CoA::ClientOptions::OnAutoAssistCast(CVar*, const char*, const char* value, void*) {
    const bool on = IsOne(value);
    CoA::ClientHooks::options.assistCastAlways = on;
    CoA::ClientHooks::options.assistCastNeedTargets = on ? 0 : 0x89;
    return true;
}

bool CoA::ClientOptions::OnSoundEnableFall(CVar*, const char*, const char* value, void*) {
    CoA::ClientHooks::options.fallInjurySound = IsOne(value);
    return true;
}

void CoA::ClientOptions::BeforeChatBubbleUpdate(bool unitIsActivePlayer, bool unitHasNamePlate) {
    float& height = CoA::ClientHooks::options.chatBubbleHeight;

    if (!s_bubbles || Int(s_bubbles) != 1) {
        height = 0.7f;
        return;
    }

    const CVar* personal = CVar::Lookup("nameplateShowPersonal");

    if (unitIsActivePlayer && personal && Int(personal) == 1) {
        const CVar* position = CVar::Lookup("nameplatePersonalPosition");
        height = position && Int(position) == 1 ? 1.25f : 0.7f;
        return;
    }

    const CVar* offset = CVar::Lookup("nameplateVerticalOffset");
    const float value = offset ? offset->m_floatValue : 0.7f;
    height = unitHasNamePlate ? value + 1.0f : 0.7f;
}

void CoA::ClientOptions::RotateZ(float* m, float angle) {
    const float c = static_cast<float>(cos(static_cast<double>(angle)));
    const float s = static_cast<float>(sin(static_cast<double>(angle)));
    const float r[16] = { c, s, 0.0f, 0.0f, -s, c, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    float out[16];

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            out[i * 4 + j] = r[i * 4 + 1] * m[4 + j] + r[i * 4] * m[j] + m[8 + j] * r[i * 4 + 2] + m[12 + j] * r[i * 4 + 3];
        }
    }

    memcpy(m, out, sizeof(out));
}

bool CoA::ClientOptions::BuildArrowMatrix(float* m, bool hasDescriptor, float renderFacing) {
    if (!hasDescriptor || !s_circleTexture || (Int(s_circleTexture) != 2 && Int(s_circleTexture) != 3)) {
        return false;
    }

    CoA::ClientOptions::RotateZ(m, -renderFacing);
    CoA::ClientOptions::RotateZ(m, 3.14159274f);
    return true;
}

bool CoA::ClientOptions::SelectionRadius(bool hasPlayer, float playerRadius, float unitRadius, float& radius) {
    if (s_circleMode && Int(s_circleMode) == 0) {
        return false;
    }

    radius = 5.0f;

    if (hasPlayer) {
        const float sum = playerRadius + unitRadius + 1.33333337f;
        radius = 5.0f > sum ? 5.0f : sum;   // maxss: NaN gives the sum
    }

    return true;
}

void CoA::ClientOptions::AfterAddWorldText(uint32_t type, float x, float y, float z) {
    if (type != 10 || !s_absorbSound || Int(s_absorbSound) != 1 || !s_deps.playSound) {
        return;
    }

    s_deps.playSound("(DONOTRENAME)AbsorbGetHit", x, y, static_cast<float>(static_cast<double>(z) + 2.0));
}

bool CoA::ClientOptions::ShowDamageText(bool hasSpell) {
    // The original reads the slot without a null test; it is always registered in the world.
    return hasSpell || (s_combatLogMelee && Int(s_combatLogMelee) != 0);
}

bool CoA::ClientOptions::ShakeAllowed() {
    return !s_cameraShake || Int(s_cameraShake) != 0;
}

bool CoA::ClientOptions::HandleTradeProposal(int32_t status) {
    if (status != 1 || !s_autoAcceptTrades || Int(s_autoAcceptTrades) == 1) {
        return false;
    }

    const char* name = s_deps.traderName ? s_deps.traderName() : nullptr;

    if (!name) {
        return false;
    }

    CoA::Events::Signal("TRADE_REQUEST", "%s", name);
    return true;
}

void CoA::ClientOptions::Init() {
    // AscAttachInline (Extensions.dll patch 0x52ADCC): InitializeFrameXML no longer registers the UI-scale CVars ...
    CoA::ClientHooks::options.frameXmlRegistersUiScale = false;
    // ... FUN_10a6b890 (0x10A67D5E) does it on the glue state.
    CoA::LuaReg::OnGlueRegistered(RegisterUiScaleCVars);

    using namespace CoA::ClientOptions;
    // name, help, flags, default, callback, category, setCommand, slot, world
    COA_CVAR("ObjectSelectionCircleTexture", nullptr, 1, "2", OnCircleTexture, 4, false, &s_circleTexture, true);
    COA_CVAR("ObjectSelectionCircleMode", nullptr, 1, "0", OnCircleMode, 4, false, &s_circleMode, true);
    COA_CVAR("flashWindow", nullptr, 1, "0", nullptr, 4, false, &s_flashWindow, true);
    COA_CVAR("absorbSoundOnSpellHit", nullptr, 1, "0", nullptr, 3, false, &s_absorbSound, true);
    COA_CVAR("CombatLogMelee", nullptr, 0x21, "1", nullptr, 3, false, &s_combatLogMelee, true);
    COA_CVAR("chatBubblesNameplate", nullptr, 1, "1", OnChatBubblesNameplate, 4, false, &s_bubbles, true);
    COA_CVAR("holdToCast", nullptr, 1, "0", nullptr, 3, false, &s_holdToCast, true);
    COA_CVAR("hideDangerousEnemyLevel", nullptr, 0x21, "1", OnHideDangerousEnemyLevel, 4, false, &s_dangerousLevel, true);
    COA_CVAR("lootArtScale", nullptr, 1, "1.0", OnLootArtScale, 4, false, &s_lootArtScale, true);
    COA_CVAR("targetName", nullptr, 1, "1", OnTargetName, 4, false, &s_targetName, true);
    COA_CVAR("WorldTextScale", nullptr, 1, "1.0", OnWorldTextScale, 4, false, &s_worldTextScale, true);
    COA_CVAR("autoAssistCast", nullptr, 1, "0", OnAutoAssistCast, 3, false, &s_autoAssistCast, true);
    COA_CVAR("cameraShake", nullptr, 1, "1", nullptr, 4, false, &s_cameraShake, true);
    COA_CVAR("UnitNameGO", nullptr, 1, "0", nullptr, 4, false, &s_unitNameGO, true);
    COA_CVAR("UnitNameInteractiveNPC", nullptr, 1, "0", nullptr, 4, false, &s_unitNameInteractive, true);
    COA_CVAR("UnitNameQuestNPC", nullptr, 1, "1", nullptr, 4, false, &s_unitNameQuest, true);
    COA_CVAR("UnitNameHostileNPC", nullptr, 1, "1", nullptr, 4, false, &s_unitNameHostile, true);
    COA_CVAR("UnitNameAllNPC", nullptr, 1, "0", nullptr, 4, false, &s_unitNameAll, true);
    COA_CVAR("Sound_EnableFall", nullptr, 1, "1", OnSoundEnableFall, 7, false, &s_soundEnableFall, true);
    COA_CVAR("autoAcceptTrades", nullptr, 1, "1", nullptr, 4, false, &s_autoAcceptTrades, true);
    COA_EVENT("TRADE_REQUEST");
}

void CoA::ClientOptions::Reset() {
    s_deps = Deps();
    CoA::ClientHooks::options = CoA::ClientHooks::OptionPatches();

    for (CVar** slot : { &s_circleTexture, &s_circleMode, &s_flashWindow, &s_absorbSound, &s_combatLogMelee, &s_bubbles,
             &s_holdToCast, &s_dangerousLevel, &s_lootArtScale, &s_targetName, &s_worldTextScale, &s_autoAssistCast,
             &s_cameraShake, &s_unitNameGO, &s_unitNameInteractive, &s_unitNameQuest, &s_unitNameHostile, &s_unitNameAll,
             &s_soundEnableFall, &s_autoAcceptTrades }) {
        *slot = nullptr;
    }
}
