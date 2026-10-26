#ifndef COA_SOURCE_MODULES_HOTPATCH_H
#define COA_SOURCE_MODULES_HOTPATCH_H

#include <cstdint>
#include <map>
#include <string>

struct lua_State;

// AscHotPatch.cpp (reconstruction 01d7400 / 50280be / 03744a1; not in the genuine DLL, the reconstruction's
// ASCENSION_HOT_PATCH build): a supported channel for server-sent interface Lua, and the 0x5BC formatter check that
// closes the route servers used before it. Design: AscensionRebirth/HOT_PATCH_DESIGN.md.
//
//   SMSG 0x8F0  HOT_PATCH: sized patchId, sized targetAddon, sized source (u32 length + bytes each).
//   CMSG 0x8F1  capability, after every world entry: u32 protocol version, u8 mode (0 off, 1 ask, 2 allow).
//   CMSG 0x8F2  result: sized patchId, u8 outcome (Outcome), sized detail (at most 255 bytes).
//
// A patch is source only (no ESC byte, no `loadstring`, no `string.dump`), at most 32 KB, 128 KB per session. It runs
// once per Lua session per (patchId, content hash), kept in the Lua global AscHotPatchApplied, which a /reload or
// relog clears. The server sends patches only at login, so patches that passed the guards are kept until the next
// glue screen and run again in each new world Lua state (/reload), consent and the run-once record checked afresh.
// An empty target runs at once with no taint source; a named target runs at that addon's ADDON_LOADED (or at once if
// it is loaded) with the addon's name as the taint source, so it gets the trust SMSG 0x94E gave that addon. The chunk
// may return "applied" or "skipped: <reason>"; otherwise "ran" or "error".
//
// The player's choice is in WTF\HotPatch.txt: "mode=0|1|2" and one "realm=<name>=<0|1>" line per answered realm.
// Without the file patches are allowed; /hotpatch off refuses them, /hotpatch ask asks once per realm, /hotpatch on
// allows them again. /hotpatch alone lists this session's patches.
//
// 0x5BC (ChallengeRequirementTypes): a formatter (fields 6-8) must be one `return <expression>` built from `value`,
// literals, operators and calls to the seven functions the shipped table uses. Anything else drops the row and the
// client keeps the one it had.
namespace CoA::HotPatch {
    enum Outcome : uint8_t {
        Ran, Applied, Deferred, Skipped, Duplicate, RefusedByPlayer, RefusedGuard, Error
    };

    struct Deps {
        lua_State* (*state)() = nullptr;                                   // FrameScript_GetContext
        void (*execute)(const char* source, const char* name) = nullptr;    // FrameScript_Execute(source, name, nullptr)
    };

    Deps& Dependencies();

    // The patch guards: true when the source may run; otherwise reason says why.
    bool Guard(const std::string& id, const std::string& source, size_t sessionBytes, std::string& reason);

    // The 0x5BC formatter grammar.
    bool FormatterAllowed(const std::string& text, std::string& reason);
    // DbcPatch's validator for 0x5BC: false drops the row.
    bool ValidateRequirementType(uint32_t opcode, const std::map<uint32_t, std::string>& strings);

    // FNV-1a, the patch's content hash.
    uint64_t Hash(const std::string& s);

    // AscRuntime::OnAfterEnterWorld / OnGlueScreen.
    void OnEnteredWorld();
    void OnGlueScreen();

    // Tests only.
    void Reset();

    void Init();
}

#endif
