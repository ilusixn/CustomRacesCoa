#ifndef COA_SOURCE_MODULES_BUILDCREATOR_H
#define COA_SOURCE_MODULES_BUILDCREATOR_H

#include "CoA/Source/Modules/CA.h"
#include <cstdint>
#include <string>
#include <vector>

// AscBuildCreator.cpp: C_BuildCreator / C_BuildEditor -- the original's BuildCreatorEntryContainer (static 0x10BDCDF0,
// accessor FUN_10101370, ctor FUN_100f7580, module init 0x10101220: the glue reset FUN_100fc070, the two binding tables
// 0x1010DF40 / 0x1010D980 after the world state's natives, 12 packet handlers).
//
//   +0x000  u32 category of the last QueryAllBuilds (BC_CATEGORY_*)
//   +0x004  vector<CBuildCreatorEntry> queried / bookmarked builds (SMSG 0x631, BookmarkBuild)
//   +0x010  CBuildCreatorEntry pending (the editor's build)
//   +0x148  vector<CBuildCreatorEntry> the category page (SMSG 0x62F)
//   +0x154  FilterableContainerBase<CBuildCreatorEntry> (reset FUN_101016d0; +0x164 filtered)
//   +0x1D0  vector<{string id, u8, u8}> per-spec active builds (SMSG 0x632 / 0x633)
//   +0x1DC  vector<string> owned build ids (SMSG 0x634)     +0x1E8 upvoted ids (SMSG 0x635)
//   +0x1F4  string id of the last RateBuild, +0x20C its up/down flag; +0x210 id of the last DeleteBuild
//   +0x228  the CA editor container (CAFilter, reset FUN_101c9df0)
//
// CBuildCreatorEntry (0x138, ctor FUN_100f7bc0): +0 id, +0x18 leveling id, +0x30 PvE id, +0x48 PvP id, +0x60 class,
// +0x64 author, +0x7C name, +0x94 subtext, +0xAC description, +0xC4 icon, +0xDC upvotes, +0xE0 / +0xE8 created / updated
// (u64), +0xF0 category, +0xF4 roles (8 by default), +0xF8 primary stat, +0xFC spells {spell, level, core, optimal,
// empowering, synergistic, flags, comment} (0x28), +0x108 random enchants {id, stacks, level, flags, comment} (0x28),
// +0x114 weapon / +0x120 armour types {type, comment} (0x1C), +0x12C hidden, +0x130 flags, +0x134 difficulty. Wire form
// FUN_100f9330 (read) / FUN_100f9c30 (write); after every read or edit FUN_10107e00 sorts the four lists.
//
// The build link (C_BuildCreator.ExportBuild, C_CharacterAdvancement.ExportBuild / ImportPendingBuild): FUN_100fd8f0's
// ":<spell>" / ":<row>t<rank>" chain tokens, "::", ":<enchant>e<stacks>" tokens, ":"; raw deflate (level 6), base64
// and URL encoding (FUN_101094a0 / FUN_1010faf0), decoded by FUN_1010f5e0 + FUN_10109ba0 and parsed by FUN_101001e0.
//
// Every client question goes through Deps (BuildCreatorClient.cpp).
namespace CoA::BuildCreator {
    struct Deps {
        bool (*hasPlayer)() = nullptr;                          // 0x4038F0 found the active player
        uint8_t (*playerClass)() = nullptr;                     // UNIT_FIELD_BYTES_0 byte 1 (descriptor +0x5C >> 8)
        const char* (*playerName)() = nullptr;                  // CGObject vtable +0xD8 (nullptr reads as "")
        uint32_t (*firstRank)(uint32_t spell) = nullptr;        // AscSpellRank::FirstRank
        uint32_t (*rankNumber)(uint32_t spell) = nullptr;       // AscSpellRank::RankNumber
        const std::vector<uint32_t>& (*rankChain)(uint32_t firstRank) = nullptr;   // AscSpellRank::Chain
        int64_t (*now)() = nullptr;                             // _time64(nullptr)
    };

    Deps& Dependencies();

    // FUN_100fd750 + what FUN_1014f0f0 reads from the entry: false when no known build has this id.
    bool SeedOfBuild(const std::string& id, CoA::CA::BuildSeed& out);

    // The link codec, for C_CharacterAdvancement.ImportPendingBuild: FUN_1010f5e0 (URL decode) and FUN_10109ba0
    // (base64 + raw inflate); and FUN_101001e0 into a scratch entry, as a CA seed.
    std::string UrlDecode(const std::string& in);
    std::string DecodeLink(const std::string& url);
    CoA::CA::BuildSeed SeedFromLink(const std::string& url);

    // FUN_100fd6c0(spec) +0x10: the spec's per-spec record (container +0x1D0) names an active build.
    bool SpecHasActiveBuild(int32_t spec);

    // FUN_100fc240 + FUN_100fd8f0: the link of an entry made from a CA build (C_CharacterAdvancement.ExportBuild).
    std::string ExportBuildOf(const CoA::CA::Build& b, bool enchants);

    // What the C_BuildEditor entry filters ask of the pending entry (+0x10), one spell at a time: FUN_100fac40 with
    // {spell, FUN_102fc4d0()} and FUN_100fb9b0 without errors, and FUN_10100180's lookup in its spell list (+0xFC).
    bool PendingCanAddSpell(uint32_t spell);
    bool PendingCanRemoveSpell(uint32_t spell);
    bool PendingHasSpell(uint32_t spell);

    // The link pieces, exposed for the tests: FUN_101094a0 (raw deflate into a buffer the input's size + base64),
    // FUN_1010faf0 (URL encode), FUN_10109ba0's two halves.
    std::string Deflate(const std::string& in);
    std::string Inflate(const std::string& in);
    std::string Base64Encode(const std::string& in);
    std::string Base64Decode(const std::string& in);
    std::string UrlEncode(const std::string& in);

    void Init();
    // Tests only: the container back to its constructed state, Deps forgotten.
    void Reset();
}

#endif
