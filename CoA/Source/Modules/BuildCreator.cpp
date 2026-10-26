// AscBuildCreator.cpp (see BuildCreator.h): the container, the entry's wire form and sort, the build link, the
// validators, the packets, the glue reset and the exported pieces. The Lua natives are BuildCreatorNatives.cpp.
//
// std::unordered_map / std::sort are used where the original uses them: MSVC's bucket policy (with the 32-bit FNV hash,
// BuildCreatorInternal.h) and its introsort give the same iteration and tie orders.
#include "CoA/Source/Modules/BuildCreatorInternal.h"
#include "CoA/Source/Modules/Account.h"
#include "CoA/Source/Modules/CAFilter.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/MysticEnchant.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "ThirdParty/zlib/zlib.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <stdexcept>

using namespace CoA::BuildCreator::detail;
using CoA::CA::Row;

namespace {
#include "CoA/Source/Modules/CAEnums.generated.inc"

    CoA::BuildCreator::Deps s_deps;
    Mgr s_mgr;
    BuildFilter s_filter;

    // The 7a convention: a list's count is trusted only while a record's minimum size (its strings empty) is left.
    const uint32_t kSpellMinSize = 17;     // u32 spell, u32 level, 4 x u8, u32 flags, cstring comment
    const uint32_t kEnchantMinSize = 17;   // u32 id, u32 stacks, u32 level, u32 flags, cstring comment
    const uint32_t kTypedMinSize = 5;      // u32 type, cstring comment
    const uint32_t kEntryMinSize = 70;     // FUN_100f9330 with every string and list empty
    const uint32_t kSpecMinSize = 3;       // cstring id, u8, u8
    const uint32_t kIdMinSize = 1;         // cstring
    // SMSG 0x632's spec index sizes the per-spec vector (resize(spec + 1)); past this the original's allocation fails.
    const uint32_t kMaxSpec = 0x10000;

    // ---- wire form -------------------------------------------------------------------------------------------
    Entry ReadEntry(CDataStore* p) {   // FUN_100f9330
        Entry e;
        e.id = CoA::Read::Str(p);
        e.levelingId = CoA::Read::Str(p);
        e.pveId = CoA::Read::Str(p);
        e.pvpId = CoA::Read::Str(p);
        e.cls = CoA::Read::U32(p);
        e.author = CoA::Read::Str(p);
        e.name = CoA::Read::Str(p);
        e.subtext = CoA::Read::Str(p);
        e.description = CoA::Read::Str(p);
        e.icon = CoA::Read::Str(p);
        e.upvotes = CoA::Read::U32(p);
        e.created = CoA::Read::U64(p);
        e.updated = CoA::Read::U64(p);
        e.category = CoA::Read::U32(p);
        e.roles = CoA::Read::U32(p);
        e.primaryStat = CoA::Read::U32(p);

        for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kSpellMinSize; --n) {   // FUN_100ea6d0 / FUN_100f6180
            Spell s;
            s.spell = CoA::Read::U32(p);
            s.level = CoA::Read::U32(p);
            s.core = CoA::Read::U8(p);
            s.optimal = CoA::Read::U8(p);
            s.empowering = CoA::Read::U8(p);
            s.synergistic = CoA::Read::U8(p);
            s.flags = CoA::Read::U32(p);
            s.comment = CoA::Read::Str(p);
            e.spells.push_back(s);
        }

        for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kEnchantMinSize; --n) {   // FUN_100ea200
            Enchant c;
            c.id = CoA::Read::U32(p);
            c.stacks = CoA::Read::U32(p);
            c.level = CoA::Read::U32(p);
            c.flags = CoA::Read::U32(p);
            c.comment = CoA::Read::Str(p);
            e.enchants.push_back(c);
        }

        for (std::vector<Typed>* list : { &e.weapons, &e.armor }) {   // FUN_100eab00 / FUN_100e9d50
            for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kTypedMinSize; --n) {
                Typed t;
                t.type = CoA::Read::U32(p);
                t.comment = CoA::Read::Str(p);
                list->push_back(t);
            }
        }

        e.hidden = CoA::Read::U8(p);
        e.flags = CoA::Read::U32(p);
        e.difficulty = CoA::Read::U32(p);
        return e;
    }

    // A hidden (+0x12C) build a non-GM does not own is dropped (SMSG 0x631 / 0x62F).
    bool Dropped(const Entry& e) {
        return e.hidden && CoA::Account::GmLevel() == 0 && !Contains(s_mgr.owned, e.id);
    }

    template <class Pred>
    std::vector<Entry>::iterator FindIn(std::vector<Entry>& v, Pred pred) {
        return std::find_if(v.begin(), v.end(), pred);
    }

    std::vector<Entry>::iterator FindId(std::vector<Entry>& v, const std::string& id) {
        return FindIn(v, [&](const Entry& q) { return q.id == id; });
    }

    // ---- the link codec --------------------------------------------------------------------------------------
    const char kBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    // FUN_1010f5e0: %XX (read with istringstream >> hex) when two characters follow; '+' stays.
    std::string UrlDecodeImpl(const std::string& in) {
        std::string out;

        for (size_t i = 0; i < in.size(); ++i) {
            if (in[i] == '%' && i + 2 < in.size()) {
                std::istringstream is(in.substr(i + 1, 2));
                int v = 0;
                is >> std::hex >> v;
                out += static_cast<char>(v);
                i += 2;
            } else {
                out += in[i];
            }
        }

        return out;
    }

    void UpsertSpell(Entry& e, const Spell& s) {   // FUN_100fa550
        auto it = std::find_if(e.spells.begin(), e.spells.end(), [&](const Spell& o) { return o.spell == s.spell; });

        if (it == e.spells.end()) {
            e.spells.push_back(s);
        } else {
            *it = s;
        }

        SortEntry(e);
    }

    void UpsertEnchant(Entry& e, const Enchant& c) {   // FUN_100fa430
        auto it = std::find_if(e.enchants.begin(), e.enchants.end(), [&](const Enchant& o) { return o.id == c.id; });

        if (it == e.enchants.end()) {
            e.enchants.push_back(c);
        } else {
            *it = c;
        }

        SortEntry(e);
    }

    void ImportLog(const char* what, uint32_t v) {
        CoA::Log::Printf("BuildCreatorEntry::ImportBuildURL: %s: %u", what, v);
    }

    uint32_t Stoul(const std::string& s) {   // std::stoul: unsigned long is 32 bits on both builds
        return static_cast<uint32_t>(std::stoul(s));
    }

    // ---- SMSG handlers ---------------------------------------------------------------------------------------
    // SMSG 0x631 (FUN_100ff200): cstring, one entry. A dropped build signals nothing; otherwise it replaces the queried
    // copy, erases it when it has no spells, or is appended.
    int32_t OnBuild(void*, uint32_t, uint32_t, CDataStore* p) {
        const std::string text = CoA::Read::Str(p);
        Entry e = ReadEntry(p);
        SortEntry(e);

        if (Dropped(e)) {
            return 1;
        }

        auto it = FindId(s_mgr.queried, e.id);

        if (it == s_mgr.queried.end()) {
            s_mgr.queried.push_back(e);
        } else if (e.spells.empty()) {
            s_mgr.queried.erase(it);
        } else {
            *it = e;
        }

        CoA::Events::Signal("BUILD_CREATOR_BUILD_RESULT", "%s%s", text.c_str(), e.id.c_str());
        return 1;
    }

    // SMSG 0x62F (FUN_100ff530): cstring, u32 page, u32 pages, u32 n, n entries into the category page (page 0 clears it
    // and the filter); a known id is erased and re-appended unless it has no spells. BUILD_CREATOR_CATEGORY_RESULT after
    // the last page.
    int32_t OnCategoryPage(void*, uint32_t, uint32_t, CDataStore* p) {
        const std::string text = CoA::Read::Str(p);
        const uint32_t pageNo = CoA::Read::U32(p);
        const uint32_t pages = CoA::Read::U32(p);
        uint32_t n = CoA::Read::U32(p);

        if (pageNo == 0) {
            s_mgr.page.clear();
            ResetFilter();
        }

        for (; n && CoA::Read::Remaining(p) >= kEntryMinSize; --n) {
            Entry e = ReadEntry(p);
            SortEntry(e);

            if (Dropped(e)) {
                continue;
            }

            auto it = FindId(s_mgr.page, e.id);

            if (it != s_mgr.page.end()) {
                s_mgr.page.erase(it);

                if (!e.spells.empty()) {
                    s_mgr.page.push_back(e);
                }
            } else {
                s_mgr.page.push_back(e);
            }
        }

        if (pageNo == pages - 1) {
            CoA::Events::Signal("BUILD_CREATOR_CATEGORY_RESULT", "%s%s", EnumName(kBcCategories, s_mgr.category).c_str(), text.c_str());
        }

        return 1;
    }

    // FUN_100fe1d0 (0x627) / FUN_100ffaf0 (0x629): the result names PUBLISH_BUILD_*, reported with the prefix swapped for
    // CREATE_ / SAVE_ (std::search over the first 8 characters, then replace(0, 8, prefix)).
    std::string Renamed(const std::string& result, const char* prefix) {
        if (result.size() >= 8 && result.compare(0, 8, "PUBLISH_") == 0) {
            return prefix + result.substr(8);
        }

        return result;
    }

    uint32_t PublishResult(const std::string& s) {   // FUN_100f00d0 on the original name
        return EnumOrLog(kPublishResults, s, kNoValue);
    }

    // SMSG 0x627: on success the created build becomes the pending one and joins the queried list, the category page
    // (when it is the queried category, or none was; the filter is reset then) and the owned ids.
    int32_t OnCreate(void*, uint32_t, uint32_t, CDataStore* p) {
        const std::string result = CoA::Read::Str(p);
        const std::string shown = Renamed(result, "CREATE_");
        std::string id;

        if (PublishResult(result) == 0) {
            s_mgr.pending = ReadEntry(p);
            SortEntry(s_mgr.pending);
            s_mgr.queried.push_back(s_mgr.pending);

            if (s_mgr.category == 0 || s_mgr.category == s_mgr.pending.category) {
                s_mgr.page.push_back(s_mgr.pending);
                ResetFilter();
            }

            s_mgr.owned.push_back(s_mgr.pending.id);
            id = s_mgr.pending.id;
        }

        CoA::Events::Signal("BUILD_CREATOR_CREATE_RESULT", "%s%s", shown.c_str(), id.c_str());
        return 1;
    }

    // SMSG 0x629: on success the saved build becomes the pending one and replaces its queried (or is appended) and
    // category-page copies; on failure a cstring id follows.
    int32_t OnSave(void*, uint32_t, uint32_t, CDataStore* p) {
        const std::string result = CoA::Read::Str(p);
        const std::string shown = Renamed(result, "SAVE_");
        std::string id;

        if (PublishResult(result) == 0) {
            s_mgr.pending = ReadEntry(p);
            SortEntry(s_mgr.pending);
            auto it = FindId(s_mgr.queried, s_mgr.pending.id);

            if (it == s_mgr.queried.end()) {
                s_mgr.queried.push_back(s_mgr.pending);
            } else {
                *it = s_mgr.pending;
            }

            auto pt = FindId(s_mgr.page, s_mgr.pending.id);

            if (pt != s_mgr.page.end()) {
                *pt = s_mgr.pending;
            }

            id = s_mgr.pending.id;
        } else {
            id = CoA::Read::Str(p);
        }

        CoA::Events::Signal("BUILD_CREATOR_SAVE_RESULT", "%s%s", shown.c_str(), id.c_str());
        return 1;
    }

    // SMSG 0x62B (FUN_100fedc0): RATE_BUILD_OK moves the rated id in or out of the upvoted list and adjusts the build's
    // Upvotes.
    int32_t OnRate(void*, uint32_t, uint32_t, CDataStore* p) {
        const std::string result = CoA::Read::Str(p);
        uint32_t index = 0;

        if (!EnumIndex(kRateResults, result, index)) {
            CoA::Log::Printf("Unexpected Value: %s", result.c_str());
        } else if (index == 0) {
            if (!s_mgr.rateUp) {
                Erase(s_mgr.upvoted, s_mgr.ratedId);

                if (Entry* e = FindBuild(s_mgr.ratedId)) {
                    --e->upvotes;
                }
            } else {
                s_mgr.upvoted.push_back(s_mgr.ratedId);

                if (Entry* e = FindBuild(s_mgr.ratedId)) {
                    ++e->upvotes;
                }
            }
        }

        const Entry* e = FindBuild(s_mgr.ratedId);
        CoA::Events::Signal("BUILD_CREATOR_RATE_RESULT", "%s%s%u", result.c_str(), s_mgr.ratedId.c_str(), e ? e->upvotes : 0u);
        return 1;
    }

    // SMSG 0x62D (FUN_100fe6a0): DELETE_BUILD_OK drops the deleted id everywhere (a pending copy of it becomes a fresh
    // build).
    int32_t OnDelete(void*, uint32_t, uint32_t, CDataStore* p) {
        const std::string result = CoA::Read::Str(p);
        uint32_t index = 0;

        if (!EnumIndex(kDeleteResults, result, index)) {
            CoA::Log::Printf("Unexpected Value: %s", result.c_str());
        } else if (index == 0) {
            auto it = FindId(s_mgr.queried, s_mgr.deletedId);

            if (it != s_mgr.queried.end()) {
                s_mgr.queried.erase(it);
            }

            if (s_mgr.pending.id == s_mgr.deletedId) {
                ResetPending();
            }

            ResetFilter();
            auto pt = FindId(s_mgr.page, s_mgr.deletedId);

            if (pt != s_mgr.page.end()) {
                s_mgr.page.erase(pt);
            }

            Erase(s_mgr.owned, s_mgr.deletedId);
            Erase(s_mgr.upvoted, s_mgr.deletedId);
        }

        CoA::Events::Signal("BUILD_CREATOR_DELETE_RESULT", "%s", result.c_str());
        return 1;
    }

    SpecRecord ReadSpecRecord(CDataStore* p) {   // FUN_100fc950
        SpecRecord r;
        r.id = CoA::Read::Str(p);
        r.a = CoA::Read::U8(p);
        r.b = CoA::Read::U8(p);
        return r;
    }

    // SMSG 0x632 (FUN_100fdfd0): u32 spec, that spec's record (the vector grows to hold it).
    int32_t OnActiveBuild(void*, uint32_t, uint32_t, CDataStore* p) {
        const uint32_t spec = CoA::Read::U32(p);
        const SpecRecord r = ReadSpecRecord(p);

        if (spec < kMaxSpec) {
            if (s_mgr.specs.size() <= spec) {
                s_mgr.specs.resize(spec + 1);
            }

            s_mgr.specs[spec] = r;
        }

        CoA::Events::Signal("BUILD_CREATOR_ACTIVE_BUILD_UPDATE", "%u", spec);
        return 1;
    }

    // SMSG 0x633 (FUN_100fe150): u32 n, n records; the list is cleared and resized to n first.
    int32_t OnActiveBuilds(void*, uint32_t, uint32_t, CDataStore* p) {
        s_mgr.specs.clear();

        for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kSpecMinSize; --n) {
            s_mgr.specs.push_back(ReadSpecRecord(p));
        }

        return 1;
    }

    // SMSG 0x634 (FUN_100fec40) / 0x635 (FUN_10100000): u32 n, n cstrings; the list is cleared and resized to n first.
    void ReadIds(CDataStore* p, std::vector<std::string>& out) {
        out.clear();

        for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kIdMinSize; --n) {
            out.push_back(CoA::Read::Str(p));
        }
    }

    int32_t OnOwned(void*, uint32_t, uint32_t, CDataStore* p) {
        ReadIds(p, s_mgr.owned);
        return 1;
    }

    int32_t OnUpvoted(void*, uint32_t, uint32_t, CDataStore* p) {
        ReadIds(p, s_mgr.upvoted);
        return 1;
    }

    int32_t OnActivateResult(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5C3 (FUN_100fde70)
        CoA::Events::Signal("BUILD_CREATOR_ACTIVATE_RESULT", "%s", CoA::Read::Str(p).c_str());
        return 1;
    }

    int32_t OnDeactivateResult(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5C5 (FUN_100fe540)
        CoA::Events::Signal("BUILD_CREATOR_DEACTIVATE_RESULT", "%s", CoA::Read::Str(p).c_str());
        return 1;
    }

    // FUN_100fc070 (OnGlueScreen): every list emptied, a fresh pending entry (FUN_100f7bc0, no class), the filter and
    // the rate / delete memory reset, and the CA editor container (+0x228, FUN_101c9df0).
    void OnGlueScreen() {
        s_mgr.category = 0;
        s_mgr.queried.clear();
        s_mgr.pending = Entry();
        s_mgr.page.clear();
        s_filter.Reset();
        s_mgr.specs.clear();
        s_mgr.owned.clear();
        s_mgr.upvoted.clear();
        s_mgr.ratedId.clear();
        s_mgr.rateUp = false;
        s_mgr.deletedId.clear();
        CoA::CAFilter::ResetEditor();
    }
}

// ---- the filter --------------------------------------------------------------------------------------------------
std::vector<size_t> CoA::BuildCreator::detail::BuildFilter::DoPopulate() {
    std::vector<size_t> v;

    for (size_t i = 0; i < s_mgr.page.size(); ++i) {
        v.push_back(i);
    }

    return v;
}

bool CoA::BuildCreator::detail::BuildFilter::TextMatch(const std::string& lower, const size_t& i) {
    if (lower.empty()) {
        return true;
    }

    if (i >= s_mgr.page.size()) {
        return false;
    }

    const Entry& e = s_mgr.page[i];
    return CoA::Filter::Lower(e.name).find(lower) != std::string::npos || CoA::Filter::Lower(e.description).find(lower) != std::string::npos ||
        CoA::Filter::Lower(e.author).find(lower) != std::string::npos;
}

uint32_t CoA::BuildCreator::detail::BuildFilter::ArgGroup(uint32_t a) {   // jump table 0x1010880C
    if (a == 1) {
        return 1;
    }

    if (a >= 2 && a <= 6) {
        return 2;
    }

    if (a == 7) {
        return 3;
    }

    if (a >= 8 && a <= 0xB) {
        return 4;
    }

    if (a >= 0xC && a <= 0xE) {
        return 5;
    }

    if (a >= 0x10 && a <= 0x2F) {
        return 6;
    }

    return 0;
}

bool CoA::BuildCreator::detail::BuildFilter::ArgMatch(uint32_t a, const size_t& i) {
    if (i >= s_mgr.page.size()) {
        return false;
    }

    const Entry& e = s_mgr.page[i];

    switch (a) {
        case 1: return (e.flags & 1) != 0;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6: return e.difficulty == a - 2;
        case 7: return Contains(s_mgr.owned, e.id);   // FUN_10101690
        case 8: return e.primaryStat == 0;
        case 9: return e.primaryStat == 1;
        case 0xA: return e.primaryStat == 3;
        case 0xB: return e.primaryStat == 4;
        case 0xC: return (e.roles >> 1) & 1;
        case 0xD: return (e.roles >> 2) & 1;
        case 0xE: return (e.roles >> 3) & 1;
        case 0xF:   // Ascension's own builds, or any updated in the last 30 days
            return e.author == "Ascension" || e.updated >= static_cast<uint64_t>(Now() - 0x278D00);
        default: return a >= 0x10 && a <= 0x2F && e.cls == a - 0xF;
    }
}

// Upvotes ascending / descending, updated time ascending / descending, as the original's fixed-width unsigned keys (the
// descending forms are ~upvotes and -updated - 1).
std::string CoA::BuildCreator::detail::BuildFilter::SortKey(uint32_t s, const size_t& i) {
    uint64_t v = 0;

    if (i < s_mgr.page.size()) {
        const Entry& e = s_mgr.page[i];

        switch (s) {
            case 1: v = e.upvotes; break;
            case 2: v = static_cast<uint32_t>(~e.upvotes); break;
            case 3: v = e.updated; break;
            case 4: v = static_cast<uint64_t>(-static_cast<int64_t>(e.updated) - 1); break;
            default: break;
        }
    }

    std::string k;
    CoA::Filter::AppendU64(k, v);
    return k;
}

Mgr& CoA::BuildCreator::detail::M() {
    return s_mgr;
}

BuildFilter& CoA::BuildCreator::detail::Builds() {
    return s_filter;
}

const Entry* CoA::BuildCreator::detail::FilteredAt(size_t i) {
    const std::vector<size_t>& f = s_filter.Filtered();

    if (i >= f.size() || f[i] >= s_mgr.page.size()) {
        return nullptr;
    }

    return &s_mgr.page[f[i]];
}

// ---- the client, through Deps ------------------------------------------------------------------------------------
bool CoA::BuildCreator::detail::HasPlayer() {
    return s_deps.hasPlayer && s_deps.hasPlayer();
}

uint32_t CoA::BuildCreator::detail::PlayerClass() {   // unit type 3 / 4: UNIT_FIELD_BYTES_0 byte 1, else 0
    return HasPlayer() && s_deps.playerClass ? s_deps.playerClass() : 0;
}

const char* CoA::BuildCreator::detail::PlayerName() {
    const char* n = s_deps.playerName ? s_deps.playerName() : nullptr;
    return n ? n : "";
}

uint32_t CoA::BuildCreator::detail::FirstRank(uint32_t spell) {
    return s_deps.firstRank ? s_deps.firstRank(spell) : spell;
}

uint32_t CoA::BuildCreator::detail::RankNumber(uint32_t spell) {
    return s_deps.rankNumber ? s_deps.rankNumber(spell) : 1;
}

const std::vector<uint32_t>& CoA::BuildCreator::detail::Chain(uint32_t firstRank) {
    static const std::vector<uint32_t> none;
    return s_deps.rankChain ? s_deps.rankChain(firstRank) : none;
}

int64_t CoA::BuildCreator::detail::Now() {
    return s_deps.now ? s_deps.now() : 0;
}

uint32_t CoA::BuildCreator::detail::MaxLevel() {
    return CoA::GameEvents::ServerMaxLevel();
}

const uint8_t* CoA::BuildCreator::detail::EnchantRow(uint32_t spell) {
    return CoA::MysticEnchant::RowOfSpell(spell);
}

uint32_t CoA::BuildCreator::detail::EnchantQuality(const uint8_t* row) {
    return CoA::MysticEnchant::QualityOfRow(row);
}

// ---- the entry helpers -------------------------------------------------------------------------------------------
// FUN_10107e00: spells by level (FUN_100eefc0), enchants by level then quality (FUN_100eec40), weapon and armour types
// by type (FUN_100ee930). The original's std::sort, not a stable sort.
void CoA::BuildCreator::detail::SortEntry(Entry& e) {
    std::sort(e.spells.begin(), e.spells.end(), [](const Spell& a, const Spell& b) { return a.level < b.level; });
    std::sort(e.enchants.begin(), e.enchants.end(), [](const Enchant& a, const Enchant& b) {
        if (a.level != b.level) {
            return a.level < b.level;
        }

        const uint8_t* ra = EnchantRow(a.id);
        const uint8_t* rb = EnchantRow(b.id);

        if (!ra || !rb) {
            return false;
        }

        return EnchantQuality(ra) < EnchantQuality(rb);
    });
    auto byType = [](const Typed& a, const Typed& b) { return a.type < b.type; };
    std::sort(e.weapons.begin(), e.weapons.end(), byType);
    std::sort(e.armor.begin(), e.armor.end(), byType);
}

// FUN_100f7bc0 + the class the editor stamps on a fresh build (EditBuild / DiscardPendingBuild / a deleted pending).
void CoA::BuildCreator::detail::ResetPending() {
    s_mgr.pending = Entry();
    s_mgr.pending.cls = PlayerClass();
}

Entry* CoA::BuildCreator::detail::FindBuild(const std::string& id) {
    for (Entry& e : s_mgr.page) {
        if (e.id == id) {
            return &e;
        }
    }

    for (Entry& e : s_mgr.queried) {
        if (e.id == id) {
            return &e;
        }
    }

    return nullptr;
}

bool CoA::BuildCreator::detail::Contains(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

void CoA::BuildCreator::detail::Erase(std::vector<std::string>& v, const std::string& s) {
    auto it = std::find(v.begin(), v.end(), s);

    if (it != v.end()) {
        v.erase(it);
    }
}

void CoA::BuildCreator::detail::ResetFilter() {
    s_filter.Reset();
}

CoA::CA::BuildSeed CoA::BuildCreator::detail::SeedOf(const Entry& e) {
    CoA::CA::BuildSeed s;
    s.classKey = e.cls;
    s.category = e.category;
    s.primaryStat = e.primaryStat;
    s.id = e.id;

    for (const Spell& sp : e.spells) {
        s.spells.emplace_back(sp.spell, sp.level);
    }

    return s;
}

Spell* CoA::BuildCreator::detail::PendingSpell(uint32_t spell) {
    for (Spell& s : s_mgr.pending.spells) {
        if (s.spell == spell) {
            return &s;
        }
    }

    return nullptr;
}

void CoA::BuildCreator::detail::WriteEntry(CoA::Packet& pk, const Entry& e) {
    pk.Str(e.id.c_str()).Str(e.levelingId.c_str()).Str(e.pveId.c_str()).Str(e.pvpId.c_str());
    pk.U32(e.cls);
    pk.Str(e.author.c_str()).Str(e.name.c_str()).Str(e.subtext.c_str()).Str(e.description.c_str()).Str(e.icon.c_str());
    pk.U32(e.upvotes).U64(e.created).U64(e.updated).U32(e.category).U32(e.roles).U32(e.primaryStat);
    pk.U32(static_cast<uint32_t>(e.spells.size()));   // FUN_100eb3c0

    for (const Spell& s : e.spells) {
        pk.U32(s.spell).U32(s.level).U8(s.core).U8(s.optimal).U8(s.empowering).U8(s.synergistic).U32(s.flags).Str(s.comment.c_str());
    }

    pk.U32(static_cast<uint32_t>(e.enchants.size()));   // FUN_100eb1f0

    for (const Enchant& c : e.enchants) {
        pk.U32(c.id).U32(c.stacks).U32(c.level).U32(c.flags).Str(c.comment.c_str());
    }

    for (const std::vector<Typed>* list : { &e.weapons, &e.armor }) {   // FUN_100eb070
        pk.U32(static_cast<uint32_t>(list->size()));

        for (const Typed& t : *list) {
            pk.U32(t.type).Str(t.comment.c_str());
        }
    }

    pk.U8(e.hidden).U32(e.flags).U32(e.difficulty);
}

// ---- the link ----------------------------------------------------------------------------------------------------
// FUN_100fd8f0: ":<first rank>" per ability chain and ":<row>t<rank>" per talent / trait / talent-ability chain
// (highest rank held), "::", then ":<enchant>e<stacks>" each, then ":". Chains are visited in the original's
// unordered_map order.
std::string CoA::BuildCreator::detail::ExportLink(const Entry& e) {
    U32Map<uint32_t> chains;

    for (const Spell& s : e.spells) {
        const uint32_t first = FirstRank(s.spell);
        auto it = chains.find(first);

        if (it == chains.end()) {
            chains.emplace(first, RankNumber(s.spell));
        } else {
            it->second = std::max(it->second, RankNumber(s.spell));
        }
    }

    std::ostringstream os;

    for (const auto& kv : chains) {
        if (Row r = CoA::CA::RowBySpell(kv.first)) {
            const int t = CoA::CA::RowType(r);

            if (t == 1 || t == 2 || t == 3 || t == 4) {
                os << ':';

                if (t == 1) {
                    os << kv.first;
                } else {
                    os << CoA::CA::RowU32(r, 0) << 't' << kv.second;
                }
            }
        }
    }

    os << ':';

    for (const Enchant& c : e.enchants) {
        os << ':' << c.id << 'e' << c.stacks;
    }

    os << ':';
    return CoA::BuildCreator::UrlEncode(CoA::BuildCreator::Base64Encode(CoA::BuildCreator::Deflate(os.str())));
}

// FUN_101001e0 (+ FUN_10100e30): replaces the entry's spells and random enchants with the link's. A malformed number
// throws out of the original's std::stoul and ends the import, logged.
void CoA::BuildCreator::detail::ImportLink(Entry& e, const std::string& url) {
    e.spells.clear();
    e.enchants.clear();
    const std::string text = CoA::BuildCreator::DecodeLink(url);
    std::istringstream is(text);
    std::string tok;

    try {
        while (std::getline(is, tok, ':')) {
            if (tok.empty()) {
                continue;
            }

            const size_t t = tok.find('t');
            const size_t ep = tok.find('e');

            if (t != std::string::npos) {
                const uint32_t rowId = Stoul(tok.substr(0, t));
                const uint32_t rank = Stoul(tok.substr(t + 1));
                const Row r = CoA::CA::FindRow(rowId);

                if (!r) {
                    continue;
                }

                if (!CoA::CA::RowVisible(r)) {
                    ImportLog("Attempted to add talent from incorrect realm", rowId);
                } else if (!CoA::CA::ClassAllowedForPlayer(r)) {
                    ImportLog("Attempted to add talent from incorrect class", rowId);
                } else if (CoA::CA::RowType(r) != 2 && CoA::CA::RowType(r) != 4) {
                    ImportLog("Attempted to add talent that is not a talent", rowId);
                } else {
                    for (uint32_t k = 0; k < rank; ++k) {
                        Spell s;
                        s.spell = CoA::CA::SpellAtRankOf(r, k + 1);
                        s.level = MaxLevel();
                        UpsertSpell(e, s);
                    }
                }
            } else if (ep == std::string::npos) {
                const uint32_t spell = Stoul(tok);
                const Row r = CoA::CA::RowBySpell(spell);

                if (!r) {
                    continue;
                }

                if (!CoA::CA::RowVisible(r)) {
                    ImportLog("Attempted to add spell from incorrect realm", spell);
                } else if (!CoA::CA::ClassAllowedForPlayer(r)) {
                    ImportLog("Attempted to add spell from incorrect class", spell);
                } else if (CoA::CA::RowType(r) != 1) {
                    ImportLog("Attempted to add spell that is not an ability", spell);
                } else {
                    Spell s;
                    s.spell = spell;
                    s.level = MaxLevel();
                    UpsertSpell(e, s);
                }
            } else {
                const uint32_t id = Stoul(tok.substr(0, ep));
                const uint32_t stacks = Stoul(tok.substr(ep + 1));
                const uint8_t* rec = EnchantRow(id);

                if (!rec) {
                    continue;
                }

                // Realm flags +0x20..+0x30 against the realm object's +0x40..+0x44.
                const uint8_t* gates = CoA::RealmInfo::Get().gates;
                bool ok = false;

                for (uint32_t i = 0; i < 5 && !ok; ++i) {
                    ok = CoA::Dbc::Table::U32(rec, 0x20 + i * 4) && gates[i];
                }

                if (!ok) {
                    ImportLog("Attempted to add enchantment from incorrect realm", id);
                    continue;
                }

                Enchant c;
                c.id = id;
                c.stacks = stacks;
                c.level = MaxLevel();
                UpsertEnchant(e, c);
            }
        }
    } catch (const std::exception& ex) {
        CoA::Log::Printf("BuildCreatorEntry::ImportBuildURL: %s", ex.what());
    }
}

// FUN_100fc240: an entry holding every rank of the build's entries and, when asked, the equipped mystic enchants counted
// into stacks.
Entry CoA::BuildCreator::detail::EntryFromBuild(const CoA::CA::Build& b, bool enchants) {
    Entry e;

    for (const CoA::CA::Entry& ce : b.entries) {
        if (Row r = CoA::CA::FindRow(ce.id)) {
            for (uint32_t k = 0; k < ce.rank; ++k) {
                Spell s;
                s.spell = CoA::CA::SpellAtRankOf(r, k + 1);
                s.level = MaxLevel();
                UpsertSpell(e, s);
            }
        }
    }

    if (!enchants) {
        return e;
    }

    // FUN_102e64e0 counts the non-empty slots of 17, then slots 0..count-1 are read.
    uint32_t count = 0;

    for (uint32_t i = 0; i < 0x11; ++i) {
        count += CoA::MysticEnchant::SlotEnchant(i) != 0;
    }

    U32Map<uint32_t> stacks;

    for (uint32_t i = 0; i < count; ++i) {
        if (const uint32_t spell = CoA::MysticEnchant::SlotEnchant(i)) {
            ++stacks[spell];
        }
    }

    for (const auto& kv : stacks) {
        Enchant c;
        c.id = kv.first;
        c.stacks = kv.second;
        c.level = MaxLevel();
        UpsertEnchant(e, c);
    }

    return e;
}

// ---- the validators ----------------------------------------------------------------------------------------------
namespace {
    // FUN_100fde10: the highest rank number among the pending spells of this spell's chain (0 if none).
    uint32_t HighestPendingRank(uint32_t spell) {
        const uint32_t first = FirstRank(spell);
        uint32_t best = 0;

        for (const Spell& s : s_mgr.pending.spells) {
            if (FirstRank(s.spell) == first) {
                best = std::max(best, RankNumber(s.spell));
            }
        }

        return best;
    }
}

std::vector<uint32_t> CoA::BuildCreator::detail::CanAddSpell(const Spell& s) {
    std::vector<uint32_t> errors;
    const Row row = CoA::CA::RowBySpell(s.spell);

    if (!row) {
        errors.push_back(1);
    } else {
        CoA::CA::Build b(SeedOf(s_mgr.pending), s.level, false);
        const uint32_t id = CoA::CA::RowU32(row, 0);

        if (b.ValidateLearn(0, id, { 0x26 }, {})) {
            errors.push_back(4);
        }

        if (b.ValidateLearn(0, id, { 0x27 }, {})) {
            errors.push_back(5);
        }

        if (s.level < CoA::CA::RequiredLevelOf(b, row)) {
            errors.push_back(7);
        }
    }

    if (PendingSpell(s.spell)) {
        errors.push_back(2);
    }

    if (RankNumber(s.spell) - 1 != HighestPendingRank(s.spell)) {
        errors.push_back(3);
    }

    if (s.level > MaxLevel()) {
        errors.push_back(6);
    }

    return errors;
}

std::vector<uint32_t> CoA::BuildCreator::detail::CanRemoveSpell(uint32_t spell) {
    std::vector<uint32_t> errors;

    if (!PendingSpell(spell)) {
        errors.push_back(1);
        return errors;
    }

    if (HighestPendingRank(spell) != RankNumber(spell)) {
        errors.push_back(2);
    }

    if (const Row row = CoA::CA::RowBySpell(spell)) {
        CoA::CA::Build b(SeedOf(s_mgr.pending), 0, false);
        const uint32_t id = CoA::CA::RowU32(row, 0);

        if (b.ValidateUnlearn(0, id, { 0x11 }, {})) {
            errors.push_back(3);
        }

        if (b.ValidateUnlearn(0, id, { 0x12 }, {})) {
            errors.push_back(4);
        }
    }

    return errors;
}

std::vector<uint32_t> CoA::BuildCreator::detail::CanSetSpellLevel(uint32_t spell, uint32_t level) {
    std::vector<uint32_t> errors;

    if (const Spell* s = PendingSpell(spell)) {
        if (s->level == level) {
            errors.push_back(1);
        }
    } else {
        errors.push_back(2);
    }

    const Row row = CoA::CA::RowBySpell(spell);

    if (!row) {
        errors.push_back(4);
    } else {
        CoA::CA::Build b(SeedOf(s_mgr.pending), level, true);

        if (level < CoA::CA::RequiredLevelOf(b, row)) {
            errors.push_back(3);
        }
    }

    const uint32_t max = MaxLevel();

    if (max < level) {
        errors.push_back(5);
    }

    // The lower ranks present must not sit above this level, nor the higher ranks below it.
    const std::vector<uint32_t>& chain = Chain(FirstRank(spell));
    uint32_t prev = 0;

    for (uint32_t c : chain) {
        if (const Spell* p = PendingSpell(c)) {
            if (p->spell == spell) {
                if (level < prev) {
                    errors.push_back(6);
                }

                break;
            }

            prev = p->level;
        }
    }

    uint32_t next = max + 1;

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        if (const Spell* p = PendingSpell(*it)) {
            if (p->spell == spell) {
                if (next < level) {
                    errors.push_back(6);
                }

                break;
            }

            next = p->level;
        }
    }

    return errors;
}

// 0 or a PUBLISH_BUILD_* index.
uint32_t CoA::BuildCreator::detail::ValidatePublish(bool isNew) {
    const Entry& e = s_mgr.pending;
    const bool player = HasPlayer();

    if (player && (e.category == 8 || e.category == 10 || e.category == 11) && CoA::Account::GmLevel() == 0) {
        return 0x15;
    }

    if (e.spells.empty()) {
        return 3;
    }

    if (e.weapons.empty()) {
        return 4;
    }

    if (e.armor.empty()) {
        return 5;
    }

    if (e.name.empty()) {
        return 6;
    }

    if (e.name == "Build Name") {
        return 7;
    }

    if (e.name.size() > 0x80) {
        return 8;
    }

    if (e.icon.empty()) {
        return 9;
    }

    if (e.icon == "inv_misc_questionmark") {
        return 0xA;
    }

    if (e.icon.size() > 0x80) {
        return 0xB;
    }

    if (e.description.empty()) {
        return 0xC;
    }

    if (e.description == "There is no description yet.") {
        return 0xD;
    }

    if (e.description.size() > 0x4000) {
        return 0xE;
    }

    const Entry* existing = FindBuild(e.id);

    if (!isNew) {
        if (!existing) {
            return 0x24;
        }

        if (player && PlayerClass() != existing->cls) {
            return 0x26;
        }
    } else if (existing) {
        return 0x10;
    }

    uint32_t id = 0;
    uint32_t rank = 0;
    uint32_t result = 0;
    uint32_t top = 0;

    for (const Spell& s : e.spells) {
        top = std::max(top, s.level);
    }

    for (uint32_t lvl = 1; lvl <= top; ++lvl) {
        CoA::CA::Build b(SeedOf(e), lvl, true);
        b.Reorder(true);

        if (!b.ValidateAll(true, { 5, 0x1C }, {}, false, id, rank, result)) {
            return 0xF;
        }
    }

    CoA::CA::Build b(SeedOf(e), MaxLevel(), true);
    b.Reorder(true);
    const uint32_t quality[4][2] = { { 0x15, 0x11 }, { 0x16, 0x12 }, { 0x17, 0x13 }, { 0x18, 0x14 } };

    for (const auto& q : quality) {
        if (!b.ValidateAll(true, { q[0] }, {}, false, id, rank, result)) {
            return q[1];
        }
    }

    switch (e.category) {
        case 2:
        case 3:
        case 4:
            if (b.GlobalAE(0) < 30) {
                return 0x16;
            }

            if (b.GlobalTE(0) < 30) {
                return 0x17;
            }

            break;
        case 5:
        case 6:
        case 7:
            if (b.GlobalAE(0) < 35) {
                return 0x16;
            }

            if (b.GlobalTE(0) < 35) {
                return 0x17;
            }

            break;
        default:
            break;
    }

    for (const Spell& s : e.spells) {
        if (s.comment.size() > 0x80) {
            return 0x18;
        }

        if ((s.synergistic != 0) + (s.empowering != 0) + (s.optimal != 0) + (s.core != 0) > 1) {
            return 0x22;
        }
    }

    for (const Enchant& c : e.enchants) {
        if (c.comment.size() > 0x80) {
            return 0x19;
        }
    }

    for (const Typed& t : e.weapons) {
        if (t.comment.size() > 0x80) {
            return 0x1A;
        }
    }

    for (const Typed& t : e.armor) {
        if (t.comment.size() > 0x80) {
            return 0x1B;
        }
    }

    if (!b.ValidateAll(true, {}, { 0x2C }, false, id, rank, result)) {
        return 0x23;
    }

    return 0;
}

// ---- the link pieces ---------------------------------------------------------------------------------------------
// The original compresses with Boost.Beast's zlib port (raw deflate, level 6, window 15, memLevel 8) and inflates with
// its inflate_stream; the client's zlib 1.2.2 (ThirdParty/zlib, the same version) runs the same algorithm.
//
// FUN_101094a0, first half: one Z_FINISH pass into a buffer the size of the input; whatever does not fit is dropped
// (only tiny inputs come out larger than they went in).
std::string CoA::BuildCreator::Deflate(const std::string& in) {
    if (in.empty()) {
        return std::string();
    }

    std::string out(in.size(), '\0');
    z_stream z;
    memset(&z, 0, sizeof(z));
    z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
    z.avail_in = static_cast<uInt>(in.size());
    z.next_out = reinterpret_cast<Bytef*>(&out[0]);
    z.avail_out = static_cast<uInt>(out.size());

    if (deflateInit2(&z, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return std::string();
    }

    deflate(&z, Z_FINISH);
    out.resize(out.size() - z.avail_out);
    deflateEnd(&z);
    return out;
}

// FUN_10109ba0, second half: into ten times the input; a stream error yields "".
std::string CoA::BuildCreator::Inflate(const std::string& in) {
    if (in.empty()) {
        return std::string();
    }

    std::string out(in.size() * 10, '\0');
    z_stream z;
    memset(&z, 0, sizeof(z));
    z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
    z.avail_in = static_cast<uInt>(in.size());
    z.next_out = reinterpret_cast<Bytef*>(&out[0]);
    z.avail_out = static_cast<uInt>(out.size());

    if (inflateInit2(&z, -15) != Z_OK) {
        return std::string();
    }

    const int r = inflate(&z, Z_SYNC_FLUSH);
    inflateEnd(&z);

    if (r != Z_OK && r != Z_STREAM_END && r != Z_BUF_ERROR) {
        return std::string();
    }

    out.resize(out.size() - z.avail_out);
    return out;
}

std::string CoA::BuildCreator::Base64Encode(const std::string& in) {   // FUN_101094a0, second half
    std::string out;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(in.data());
    size_t n = in.size();

    for (; n >= 3; n -= 3, p += 3) {
        out += kBase64[p[0] >> 2];
        out += kBase64[(p[0] & 3) * 0x10 + (p[1] >> 4)];
        out += kBase64[(p[1] & 0xF) * 4 + (p[2] >> 6)];
        out += kBase64[p[2] & 0x3F];
    }

    if (n == 1) {
        out += kBase64[p[0] >> 2];
        out += kBase64[(p[0] & 3) * 0x10];
        out += "==";
    } else if (n == 2) {
        out += kBase64[p[0] >> 2];
        out += kBase64[(p[0] & 3) * 0x10 + (p[1] >> 4)];
        out += kBase64[(p[1] & 0xF) * 4];
        out += '=';
    }

    return out;
}

// FUN_10109ba0, first half (table 0x10B2E518): stops at '=' or the first byte outside the alphabet; a trailing partial
// group yields its whole bytes.
std::string CoA::BuildCreator::Base64Decode(const std::string& in) {
    auto value = [](uint8_t c) -> int {
        const char* f = strchr(kBase64, c);
        return (c && f) ? static_cast<int>(f - kBase64) : -1;
    };

    std::string out;
    uint8_t q[4] = {};
    int k = 0;

    for (char ch : in) {
        const uint8_t c = static_cast<uint8_t>(ch);

        if (c == '=' || value(c) < 0) {
            break;
        }

        q[k++] = static_cast<uint8_t>(value(c));

        if (k == 4) {
            out += static_cast<char>((q[0] << 2) | ((q[1] >> 4) & 3));
            out += static_cast<char>((q[1] << 4) | ((q[2] >> 2) & 0xF));
            out += static_cast<char>((q[2] << 6) + q[3]);
            k = 0;
        }
    }

    if (k) {
        for (int i = k; i < 4; ++i) {
            q[i] = 0;
        }

        const char b[3] = { static_cast<char>((q[0] << 2) | ((q[1] >> 4) & 3)), static_cast<char>((q[1] << 4) | ((q[2] >> 2) & 0xF)),
            static_cast<char>((q[2] << 6) + q[3]) };
        out.append(b, static_cast<size_t>(k - 1));
    }

    return out;
}

// FUN_1010faf0: RFC 3986 unreserved characters pass, everything else becomes %XX (uppercase hex).
std::string CoA::BuildCreator::UrlEncode(const std::string& in) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;

    for (char ch : in) {
        const unsigned char c = static_cast<unsigned char>(ch);

        if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }

    return out;
}

// ---- the exported pieces -----------------------------------------------------------------------------------------
CoA::BuildCreator::Deps& CoA::BuildCreator::Dependencies() {
    return s_deps;
}

std::string CoA::BuildCreator::UrlDecode(const std::string& in) {
    return UrlDecodeImpl(in);
}

std::string CoA::BuildCreator::DecodeLink(const std::string& url) {
    return Inflate(Base64Decode(UrlDecodeImpl(url)));
}

CoA::CA::BuildSeed CoA::BuildCreator::SeedFromLink(const std::string& url) {
    Entry e;
    ImportLink(e, url);
    return SeedOf(e);
}

bool CoA::BuildCreator::SeedOfBuild(const std::string& id, CoA::CA::BuildSeed& out) {
    const Entry* e = FindBuild(id);

    if (!e) {
        return false;
    }

    out = SeedOf(*e);
    return true;
}

bool CoA::BuildCreator::SpecHasActiveBuild(int32_t spec) {
    return spec >= 0 && static_cast<size_t>(spec) < s_mgr.specs.size() && !s_mgr.specs[static_cast<size_t>(spec)].id.empty();
}

std::string CoA::BuildCreator::ExportBuildOf(const CoA::CA::Build& b, bool enchants) {
    return ExportLink(EntryFromBuild(b, enchants));
}

bool CoA::BuildCreator::PendingCanAddSpell(uint32_t spell) {
    Spell s;
    s.spell = spell;
    s.level = MaxLevel();
    return CanAddSpell(s).empty();
}

bool CoA::BuildCreator::PendingCanRemoveSpell(uint32_t spell) {
    return CanRemoveSpell(spell).empty();
}

bool CoA::BuildCreator::PendingHasSpell(uint32_t spell) {
    return PendingSpell(spell) != nullptr;
}

void CoA::BuildCreator::Init() {   // 0x10101220
    CoA::Lifecycle::OnGlueScreen(OnGlueScreen);
    InitNatives();
    COA_PACKET(0x5C3, OnActivateResult);
    COA_PACKET(0x5C5, OnDeactivateResult);
    COA_PACKET(0x627, OnCreate);
    COA_PACKET(0x629, OnSave);
    COA_PACKET(0x62B, OnRate);
    COA_PACKET(0x62D, OnDelete);
    COA_PACKET(0x62F, OnCategoryPage);
    COA_PACKET(0x631, OnBuild);
    COA_PACKET(0x632, OnActiveBuild);
    COA_PACKET(0x633, OnActiveBuilds);
    COA_PACKET(0x634, OnOwned);
    COA_PACKET(0x635, OnUpvoted);

    // What CharacterAdvancementMgr asks of this container (the CA side's Deps, Plan 7).
    CoA::CA::Deps& d = CoA::CA::Dependencies();
    d.specHasCreatorBuild = CoA::BuildCreator::SpecHasActiveBuild;
    d.exportBuild = CoA::BuildCreator::ExportBuildOf;
    d.decodeLink = CoA::BuildCreator::DecodeLink;
    d.seedFromLink = CoA::BuildCreator::SeedFromLink;
    d.seedOfBuild = CoA::BuildCreator::SeedOfBuild;
    d.creatorCanAddSpell = CoA::BuildCreator::PendingCanAddSpell;
    d.creatorCanRemoveSpell = CoA::BuildCreator::PendingCanRemoveSpell;
    d.creatorHasSpell = CoA::BuildCreator::PendingHasSpell;

    COA_EVENT("BUILD_CREATOR_BUILD_RESULT");
    COA_EVENT("BUILD_CREATOR_CATEGORY_RESULT");
    COA_EVENT("BUILD_CREATOR_CREATE_RESULT");
    COA_EVENT("BUILD_CREATOR_SAVE_RESULT");
    COA_EVENT("BUILD_CREATOR_RATE_RESULT");
    COA_EVENT("BUILD_CREATOR_DELETE_RESULT");
    COA_EVENT("BUILD_CREATOR_ACTIVE_BUILD_UPDATE");
    COA_EVENT("BUILD_CREATOR_ACTIVATE_RESULT");
    COA_EVENT("BUILD_CREATOR_DEACTIVATE_RESULT");
}

void CoA::BuildCreator::Reset() {
    OnGlueScreen();
    s_deps = Deps();
}
