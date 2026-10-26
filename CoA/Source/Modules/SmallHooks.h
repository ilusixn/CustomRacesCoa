#ifndef COA_SOURCE_MODULES_SMALLHOOKS_H
#define COA_SOURCE_MODULES_SMALLHOOKS_H

#include <cstdint>

struct CVar;

// AscSmallHooks.cpp: single-hook installers (0x6B2F60 is in CVarHooks; 0x6E09E0 in ItemCollections).
//
//   FUN_103223a0  SpellQueueWindow ("400", flags 1, category 3, world) and 0x805F60 (Spell_C_IsSpellPending(castItem,
//                 spell, except), from Spell_C_CastSpell) REPLACED by FUN_10322440: false without a player or a cast end
//                 time (+0xA7C); true once that time has passed; otherwise whether it is more than SpellQueueWindow ms
//                 away (unsigned compare).
//   FUN_10227970  bankEquipmentManager ("0", flags 1, category 4, world, callback FUN_102277e0) and the detour
//                 FUN_10228610 on 0x5ADA20 (CGEquipmentManager::UseEquipmentSet): with the CVar on and no banker GUID
//                 (0xBEB9A0) the GUID is 1 while the client function runs, then 0 again when the CVar is still on.
//                 The callback sends CMSG 0x55D (u8: "1" -> 1, else 0) in world; entering the world sends 1 while on.
//   FUN_10111c60  0x5C2030 (CGCalendar::InsertEvent) runs with the flag 0x10D3D69C set; nothing reads it (marker only).
//   FUN_102f8930  0x454070 (System_SFile::ConvertRelativePathName): after it, a path with the realm data path inserted
//                 is built and discarded (marker only).
namespace CoA::SmallHooks {
    struct Deps {
        bool (*castEndTime)(uint32_t& out) = nullptr;   // the active player's +0xA7C; false without a player
        uint32_t (*now)() = nullptr;                     // OsGetAsyncTimeMs (0x86AE20)
        uint64_t (*bankerGuid)() = nullptr;              // CGBankInfo::m_bankerGUID (0xBEB9A0)
        void (*setBankerGuid)(uint64_t guid) = nullptr;
        uint64_t (*activePlayerGuid)() = nullptr;        // AscScript::ActivePlayerGuid
    };

    Deps& Dependencies();

    // FUN_10322440, replacing 0x805F60.
    bool SpellQueueReady();

    // FUN_10228610 around 0x5ADA20: Before returns whether it wrote the GUID; After takes that.
    bool BeforeUseEquipmentSet();
    void AfterUseEquipmentSet(bool wrote);

    // FUN_102277e0, bankEquipmentManager's callback; FUN_10227700 on entering the world.
    bool OnBankManager(CVar* var, const char* oldValue, const char* value, void* arg);
    void BankManagerEnterWorld();

    // Tests only.
    void Reset();

    void Init();
}

#endif
