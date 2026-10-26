#ifndef COA_SOURCE_MODULES_CLIENTOPTIONS_H
#define COA_SOURCE_MODULES_CLIENTOPTIONS_H

#include <cstdint>

struct CVar;

// AscClientOptions.cpp: the attach init's (FUN_10a66100) twenty option CVars (FUN_10114540 world queue, no help text;
// checked against the genuine DLL's call sites 0x10A67607 .. 0x10A6B338) and the hooks that read them (installer
// FUN_10a66d40). The callbacks' byte patches are ClientHooks::options flags; the decomp sites call the hooks below.
//
//   0x56C500 CGChatBubbleFrame::Update   before: the bubble height (sub_10a429a0)
//   0x744150 BuildSelectionRotMatrix     arrow textures turn the circle with the unit (sub_10a44df0)
//   0x720330 UpdateSelectionRadius       both units' bounding radii (sub_10a462f0)
//   0x71F990 AddWorldText                after: "AbsorbGetHit" on type 10 (FUN_10a468f0)
//   0x722340 AddWorldDamageText          melee numbers off (sub_10a468b0)
//   0x606330 CGCamera::AddShake          camera shake off (sub_10a42930)
//   0x5873E0 HandleTradeMessage          TRADE_REQUEST instead of the stock prompt (FUN_10a44c90)
//   glue list 0x10BE284C: FUN_10a6b890 jumps to 0x5238A0 (CGGameUI::Initialize: uiScale / useUiScale), whose call in
//   InitializeFrameXML (0x52ADCC) AscAttachInline NOPs -- the UI-scale CVars exist on the glue state.
namespace CoA::ClientOptions {
    struct Deps {
        bool (*inWorld)() = nullptr;                              // 0xBD0792
        void (*registerUiScaleCVars)() = nullptr;                 // 0x5238A0
        void (*reloadSelectionTexture)() = nullptr;               // 0x7460C0 (CGObject_C::Initialize)
        void (*recomputeSelectionRadii)() = nullptr;              // 0x4D4B30 over the visible objects -> 0x720330
        void (*setLootArtScale)(float scale) = nullptr;           // 0x6F75B0(4) +0x10
        // 0xAF4748 + i * 0x1C (+0x10 / +0x14) and 0xAF487C + i * 4, i < 11.
        void (*setWorldTextSize)(uint32_t type, float width, float height) = nullptr;
        void (*setWorldTextOther)(uint32_t index, float value) = nullptr;
        void (*playSound)(const char* kit, float x, float y, float z) = nullptr;   // 0x4C74A0(kit, pos, 0, 0)
        const char* (*traderName)() = nullptr;                    // the trader (0xCA0FE8) vtable +0xD8; nullptr without
    };

    Deps& Dependencies();

    // The CVar callbacks, as the original installs them (all return 1).
    bool OnCircleTexture(CVar* var, const char* oldValue, const char* value, void* arg);          // FUN_10a46fa0
    bool OnCircleMode(CVar* var, const char* oldValue, const char* value, void* arg);             // FUN_10a46f60
    bool OnChatBubblesNameplate(CVar* var, const char* oldValue, const char* value, void* arg);   // FUN_10a48c10
    bool OnHideDangerousEnemyLevel(CVar* var, const char* oldValue, const char* value, void* arg);  // FUN_10a49850
    bool OnLootArtScale(CVar* var, const char* oldValue, const char* value, void* arg);           // FUN_10a4a200
    bool OnTargetName(CVar* var, const char* oldValue, const char* value, void* arg);             // FUN_10a4a270
    bool OnWorldTextScale(CVar* var, const char* oldValue, const char* value, void* arg);         // FUN_10a47b70
    bool OnAutoAssistCast(CVar* var, const char* oldValue, const char* value, void* arg);         // FUN_10a47c30
    bool OnSoundEnableFall(CVar* var, const char* oldValue, const char* value, void* arg);        // FUN_10a47780

    // atof clamped by the original's cmovbe pairs: NaN passes through.
    float Clamp(float x, float lo, float hi);

    // sub_10a429a0, before 0x56C500: the bubble height (ClientHooks::options.chatBubbleHeight). With
    // chatBubblesNameplate 1: the player's own bubble with nameplateShowPersonal 1 is 1.25 at nameplatePersonalPosition
    // 1, else 0.7; anyone else's is nameplateVerticalOffset + 1.0 while the unit has a nameplate, else 0.7.
    void BeforeChatBubbleUpdate(bool unitIsActivePlayer, bool unitHasNamePlate);

    // FUN_102bd3f0: m = Rz(angle) * m (row-major 4x4).
    void RotateZ(float* m, float angle);
    // sub_10a44df0: with an arrow texture (2 / 3) and the object's descriptor, the matrix (the caller's identity) is
    // turned by -renderFacing and a half turn in place of the original (true).
    bool BuildArrowMatrix(float* m, bool hasDescriptor, float renderFacing);
    // sub_10a462f0: unless ObjectSelectionCircleMode is 0, the radius (+0xB0C) is both units' descriptor +0x108
    // (UNIT_FIELD_COMBATREACH) + 4/3, at least 5 (5 without a player), in place of the original (true).
    bool SelectionRadius(bool hasPlayer, float playerReach, float unitReach, float& radius);

    // FUN_10a468f0, after 0x71F990: type 10 with absorbSoundOnSpellHit 1 plays "(DONOTRENAME)AbsorbGetHit" 2 above the
    // unit's position.
    void AfterAddWorldText(uint32_t type, float x, float y, float z);
    // sub_10a468b0: a melee number (no spell) shows only with CombatLogMelee non-zero.
    bool ShowDamageText(bool hasSpell);
    // sub_10a42930: a shake runs unless cameraShake is 0.
    bool ShakeAllowed();
    // FUN_10a44c90: status 1 with autoAcceptTrades not 1 and a trader: TRADE_REQUEST(name) replaces the client's
    // handling (true).
    bool HandleTradeProposal(int32_t status);

    void Init();
    // Tests only: Deps, slots and patches back to their start.
    void Reset();
}

#endif
