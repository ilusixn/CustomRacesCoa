#ifndef COA_SOURCE_RUNTIME_CLIENTHOOKS_H
#define COA_SOURCE_RUNTIME_CLIENTHOOKS_H

#include <cstdint>
#include <utility>
#include <vector>

struct lua_State;

// The original's hook-manager detours on client functions (AscRuntime.cpp's InstallHooks): each runs a callback list
// before or after the client function, and the modules that need a point register into its list. The client calls
// the list at the point the detour sits (marked source edits). Lists nothing registers into yet run empty.
namespace CoA::ClientHooks {
    // A callback list: Run calls each in registration order.
    template <class... Args>
    class Hooks {
    public:
        using Fn = void (*)(Args...);

        void Add(Fn fn) {
            this->m_fns.push_back(fn);
        }

        void Run(Args... args) const {
            for (Fn fn : this->m_fns) {
                fn(args...);
            }
        }

        bool Empty() const {
            return this->m_fns.empty();
        }

        void Clear() {
            this->m_fns.clear();
        }

    private:
        std::vector<Fn> m_fns;
    };

    // A predicate list kept in call-site order (the original's InsertBySite: after every entry with a site <= this
    // one). AllPass stops at the first false; AnyTrue at the first true.
    template <class... Args>
    class Predicates {
    public:
        using Fn = bool (*)(Args...);

        void Add(Fn fn, uint32_t site) {
            auto at = this->m_fns.begin();

            while (at != this->m_fns.end() && at->first <= site) {
                ++at;
            }

            this->m_fns.insert(at, { site, fn });
        }

        bool AllPass(Args... args) const {
            for (const auto& entry : this->m_fns) {
                if (!entry.second(args...)) {
                    return false;
                }
            }

            return true;
        }

        bool AnyTrue(Args... args) const {
            for (const auto& entry : this->m_fns) {
                if (entry.second(args...)) {
                    return true;
                }
            }

            return false;
        }

        bool Empty() const {
            return this->m_fns.empty();
        }

        void Clear() {
            this->m_fns.clear();
        }

    private:
        std::vector<std::pair<uint32_t, Fn>> m_fns;
    };

    // Runs a callable when the scope ends (an "after" list over a function with several returns).
    template <class F>
    struct ScopeExit {
        F fn;

        ~ScopeExit() {
            fn();
        }
    };

    template <class F>
    ScopeExit<F> OnScopeExit(F fn) {
        return ScopeExit<F>{ fn };
    }

    inline Hooks<> enterWorld;                       // 0x528010 CGGameUI::EnterWorld, before
    inline Hooks<> afterEnterWorld;                  // 0x528010, after
    inline Hooks<> leaveWorld;                       // 0x528C30 CGGameUI::LeaveWorld, before (list 0x10BE28AC)
    inline Hooks<> beforeShutdown;                   // 0x528F00 CGGameUI::Shutdown, before (list 0x10BE28CC)
    inline Hooks<> afterClientDestroy;               // 0x402910 WowClientDestroy, after (list 0x10BE2A8C)
    inline Hooks<> afterGlueInitialize;              // 0x4DB9F0 CGlueMgr::Initialize, after (AscDiscord's detour 0x10A46350)
    inline Hooks<> afterSetPaths;                    // 0x402B20 SetPaths, after (list 0x10BE2ACC)
    inline Hooks<> afterUiReload;                    // 0x52A980 InitializeFrameXML, after, when it was a reload (0x10BE29EC)
    inline Hooks<> afterClearTrackedObjects;         // 0x4F6F90 CGWorldFrame::ClearTrackedObjects, after (0x10BE2AAC)
    inline Hooks<> beforeTarget;                     // 0x524BF0 CGGameUI::Target, before (list 0x10BE2950)
    inline Hooks<> afterTarget;                      // 0x524BF0, after (list 0x10BE2970)
    inline Hooks<> initialSpellsAlternate;           // 0x6DF050 SMSG_INITIAL_SPELLS, every second packet (0x10BE28EC)
    inline Hooks<int32_t> afterZoneFeedback;         // 0x5204C0 CGGameUI::NewZoneFeedback, after, with the area (0x10BE290C)
    inline Hooks<void*> objectFree;                  // 0x6FC0F0 CMissileCollision::RemoveUnitFromSystem, before (0x10BE2BEC)
    inline Hooks<void*> gameObjectUsed;              // 0x70F680 CGGameObject_C::UseObject, after
    inline Hooks<void*> itemUsed;                    // 0x708C20 CGItem_C::UseItem, after
    inline Hooks<void*> afterPreAnimate;             // 0x743EC0 CGObject_C::PreAnimate, after (list 0x10BE2CF8)
    inline Hooks<void*> afterActivePlayerInit;       // 0x6E7F50 CGPlayer_C::PostInitActivePlayer, after (0x10BE2DB8)
    inline Hooks<void*> afterUnitCreated;            // 0x73F660 CGUnit_C::CGUnit_C, after (list 0x10BE2BCC)
    inline Hooks<void*> beforeTeleport;              // 0x6ECF80 CMovement_C::Teleport, before, with its unit (0x10BE2BAC)
    inline Hooks<void*, uint32_t, uint32_t, void*> beforeSpellPacket;   // 0x80FEE0 Packet_Group_30, before (0x10BE2D58)
    inline Hooks<const void*> beforeKeyDown;         // 0x4943C0 CSimpleTop::OnKeyDown, in world (list 0x10BE2B0C)
    inline Hooks<const void*> beforeKeyUp;           // 0x494490 CSimpleTop::OnKeyUp, in world (list 0x10BE2B4C)
    inline Hooks<const void*> beforeKeyRepeat;       // 0x494530 CSimpleTop::OnKeyDownRepeat, in world (list 0x10BE2B2C)
    inline Predicates<void*, uint32_t, const void*> addAuraEffect;      // 0x724820 CGUnit_C::AddAuraEffect: false skips it
    inline Predicates<void*, uint32_t, const void*> removeAuraEffect;   // 0x71E930 CGUnit_C::RemoveAuraEffect: false skips it
    // 0x80B5D0 Spell_C_IsUsableAction: after a true result, false makes it unusable (list 0x10BE2DD8).
    inline Predicates<void*, const void*, const void*, const void*, bool> usableAction;
    // 0x744AC0 CGObject_C::ClearEffectList (replaced): true removes the effect (list 0x10BE2D78).
    inline Predicates<void*, void*> removeEffect;
    // FUN_102766c0 (0x81AA00 FrameScript_SignalEvent): callbacks for one stock event id, before its listeners.
    inline std::vector<std::pair<int32_t, void (*)(lua_State*)>> stockEvents;

    // FUN_102766c0: a signal without a Lua state or without an event object for the id is refused and logged to
    // the Fatal channel. Otherwise the id's stock-event callbacks run and the name becomes LastSignalEvent.
    bool AllowSignalEvent(int32_t eventId, lua_State* L, uint32_t registeredCount, bool hasEventData, bool hasEventObject,
        const char* name);
    // DAT_10bcbcc8: the crash report's "Last FrameScript_SignalEvent".
    const char* LastSignalEvent();

    // AscCAMgr.cpp's detours on the spell packets (FUN_10172660 / 10172850 / 101729d0 / 101728e0 / 10172b50): each
    // handler asks before the client's handling and reports after. Unset, the client handles the packet alone.
    struct SpellPacketHooks {
        void (*initialSpells)(const uint32_t* spells, uint32_t count) = nullptr;    // 0x6DF050, after
        bool (*learnedIsQuiet)(uint32_t spell) = nullptr;                          // 0x6E7D60, before
        void (*learned)(uint32_t spell) = nullptr;                                 // 0x6E7D60, after
        void (*removedSpells)(const uint32_t* spells, uint32_t count) = nullptr;   // 0x6E2240, after
        void (*removedBefore)(uint32_t spell, bool trailingZero) = nullptr;        // 0x6E7840, before
        void (*removedAfter)() = nullptr;                                          // 0x6E7840, after
        bool (*supersedeIsQuiet)(uint32_t oldSpell, uint32_t newSpell) = nullptr;  // 0x6E7E00, before
        void (*superseded)(uint32_t oldSpell, uint32_t newSpell) = nullptr;        // 0x6E7E00, after
        bool (*removeMacro)(uint32_t spell) = nullptr;                             // 0x5AAB90: false skips it
    };
    inline SpellPacketHooks spellPackets;
    // 0x542030 CGSpellBook::AddKnownSpell: AscRuntime.cpp's detour 0x10275ED0 (C_Wildcard's mode manager,
    // WildcardRolls.cpp): true when it took the add over (deferred it, or made it itself); unset, the client adds alone.
    inline bool (*addKnownSpell)(int32_t spell, int32_t notify, int32_t replace) = nullptr;
    // List 0x10BE2AEC (FUN_10278450): run with the spell after the detour's add (FUN_102781e0).
    inline Hooks<uint32_t> afterAddKnownSpell;
    // FUN_10308830 / FUN_10308e40: while above zero, the client's learn messages (the three 0x5216F0 calls at
    // 0x542679 / 0x5427BE / 0x54284E) and unlearn messages (0x6E7346 / 0x6E7356) are NOPed.
    inline int suppressLearnMessages = 0;
    inline int suppressUnlearnMessages = 0;
    // FUN_10193a60: while RemoveMacro runs under the hook, GetSpell's macro test (0x5A8C70) is a jmp.
    inline int getSpellSkipsMacros = 0;
    // AscManastorm.cpp FUN_102a1e90: inside a Manastorm the jne at 0x80DB37 (Spell_C_ClickSpell's modal-next-spell
    // test) is a jmp, so a click on the modal spell goes on to cast it.
    inline bool manastormSkipsModalSpell = false;
    // AscAura137.cpp FUN_10a73ea0: while on, the falling tests of CMovementShared::StartTurn / StopTurn (0x988E48 /
    // 0x989053) are NOPed and SetRawFacing (0x989B97 / 0x989B9A) always re-anchors with force 1 (turning while falling).
    inline bool aura137TurnWhileFalling = false;

    // AscClientOptions.cpp / AscCamera.cpp: the client bytes their CVar callbacks rewrite, as source flags (Plan 13
    // ruling). Each starts at the stock bytes; the callbacks run at every world CVar registration.
    struct OptionPatches {
        // AscAttachInline (0x52ADCC): InitializeFrameXML's call to CGGameUI::Initialize (0x5238A0) is NOPed; the DLL
        // makes that call at glue (ClientOptions). False once the patch is applied.
        bool frameXmlRegistersUiScale = true;
        // FUN_10a48c10 (chatBubblesNameplate "1"): 0x720020's jne NOPed (a bubble shows over a nameplate) and 0x725737's
        // je a jmp (a new nameplate keeps the bubble).
        bool bubblesOverNameplates = false;
        // 0x56C76B: the bubble's height above the head; the DLL's float (DAT_10bccb9c) once the callback ran, 0.7 both.
        float chatBubbleHeight = 0.7f;
        // FUN_10a46fa0 (ObjectSelectionCircleTexture): the selection texture 0x746147 pushes.
        const char* selectionTexture = "Textures\\UnitSelectTexture.blp";
        // FUN_10a47c30 (autoAssistCast): "1" NOPs 0x80C8C1 / 0x80D7C1 (every spell casts through assist targets) and
        // the `or eax, 0x80` at 0x80C91D; any other value writes `or eax, 0x89` there (the original's quirk).
        bool assistCastAlways = false;
        uint32_t assistCastNeedTargets = 0x80;
        // FUN_10a49850 (hideDangerousEnemyLevel): not "1" NOPs 0x60FAB2 / 0x621766, the "??" level test for an enemy
        // ten levels above the player.
        bool dangerousEnemyLevelHidden = true;
        // FUN_10a4a270 (targetName): not "1" NOPs 0x729D3D's je, the locked target's name shortcut.
        bool lockedTargetNameShown = true;
        // FUN_10a47780 (Sound_EnableFall): not "1" makes 0x73D46A's jle a jmp (no fall-damage injury sound).
        bool fallInjurySound = true;
        // FUN_10199420 / FUN_10199da0 (ActionCam "1" / cameraActionHeadBobs): 0x5FFD34's jp a jmp (head bobbing at any
        // camera distance).
        bool bobAtAnyDistance = false;
        // FUN_1019a9e0 (cameraTargetFocusEnemyEnable "1"): the jne at 0x72E870 / 0x72E5FE / 0x72E6AE / 0x5FBC80 become
        // jmp (moving, strafing, turning, a blocked turn keep the tracking target) and the ClearTrackingTarget call at
        // 0x7569B6 (a melee round) is NOPed.
        bool movementKeepsTracking = false;
        // FUN_1019bcb0 (cameraTargetFocusTurnSpeed): the client's own writers of the tracking turn rate (0xCA11DC) at
        // 0x727520 (ClickToMove) and 0x731720 (HandleTrackingFacing) are NOPed.
        bool trackingTurnRateFixed = false;
        // FUN_1019a190 (cameraM2Collision): "1" keeps the doodad fade store at 0x791E60 and the M2 bit of the camera
        // collision flags (0x605E13: 0x100171, else 0x170).
        bool doodadFadeStore = true;
        uint32_t cameraCollisionFlags = 0x100171;
        // FUN_10199650 (autoTarget): not "1" makes 0x75697C / 0x806AA2 jmp (an attacker is never auto-targeted).
        bool autoTargetAttacker = true;
    };
    inline OptionPatches options;

    // 0x102776A0: the alternate list runs on every second SMSG_INITIAL_SPELLS (flag 0x10BE2E38, starting clear).
    void InitialSpellsReceived();

    // Tests only.
    void Reset();

    // ClientHooksClient.cpp: the Engine hook pointers, the effect sweep after each idle tick and the portrait refresh.
    void InstallClient();
}

#endif
