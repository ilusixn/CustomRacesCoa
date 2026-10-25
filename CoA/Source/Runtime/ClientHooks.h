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
    // FUN_10308830 / FUN_10308e40: while above zero, the client's learn messages (the three 0x5216F0 calls at
    // 0x542679 / 0x5427BE / 0x54284E) and unlearn messages (0x6E7346 / 0x6E7356) are NOPed.
    inline int suppressLearnMessages = 0;
    inline int suppressUnlearnMessages = 0;
    // FUN_10193a60: while RemoveMacro runs under the hook, GetSpell's macro test (0x5A8C70) is a jmp.
    inline int getSpellSkipsMacros = 0;

    // 0x102776A0: the alternate list runs on every second SMSG_INITIAL_SPELLS (flag 0x10BE2E38, starting clear).
    void InitialSpellsReceived();

    // Tests only.
    void Reset();

    // ClientHooksClient.cpp: the Engine hook pointers, the effect sweep after each idle tick and the portrait refresh.
    void InstallClient();
}

#endif
