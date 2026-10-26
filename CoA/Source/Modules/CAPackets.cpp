// AscCAMgr.cpp's packets, the once-a-second update (CharacterAdvancementMgr::Update, FUN_10182780), the glue reset, the
// spell-packet hook logic and the module's init (see CA.h).
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/CAFilter.h"
#include "CoA/Source/Modules/CAInternal.h"
#include "CoA/Source/Modules/CANativesInternal.h"
#include "CoA/Source/Modules/AttachPatches.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/MysticEnchant.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include <ctime>
#include <string>

namespace {
#include "CoA/Source/Modules/CAEnums.generated.inc"

    using namespace CoA::CA::detail;
    using CoA::CA::Build;
    using CoA::CA::Entry;
    using CoA::CA::Row;
    using CoA::CA::RowU32;

    Suggestions s_sugg;

    void EnsurePlayer() {
        Mgr& m = M();

        if (m.player) {
            return;
        }

        Player* p = new Player(ActivePlayerGuid());
        Player* old = m.player;
        m.player = p;
        delete old;
    }

    // FUN_10171d90: SMSG 0x725 -- u32 active spec, u32 spec count.
    int32_t OnActiveSpec(void*, uint32_t, uint32_t, CDataStore* msg) {
        Mgr& m = M();

        if (!m.player) {
            EnsurePlayer();
            CoA::CA::ResetPendingBuild();
        }

        const uint32_t spec = CoA::Read::U32(msg);
        const uint32_t count = CoA::Read::U32(msg);
        const uint32_t old = m.player->spec;
        m.player->spec = spec;
        m.player->count = count;

        if (old != spec) {
            Fire(m.onSpecChanged, old & 0xFF, spec & 0xFF);
        }

        CoA::Events::Signal("ASCENSION_CA_SPECIALIZATION_ACTIVE_ID_CHANGED", "%u", spec + 1);
        return 1;
    }

    // FUN_10180520: the active build picks the four credit categories up.
    void RefreshCredits() {
        Mgr& m = M();

        if (!m.player) {
            return;
        }

        if (Build* b = m.player->Get(m.player->spec)) {
            for (uint8_t c = 1; c <= 4; ++c) {
                auto it = m.credits.find(c);
                b->credit[c - 1] = it == m.credits.end() ? 0 : it->second;
            }
        }
    }

    // FUN_101a1b70 (every glue screen): the credit map emptied (FUN_101a1c90 -> FUN_101092d0).
    void ClearCredits() {
        M().credits.clear();
    }

    // FUN_101a1c00: SMSG 0x926 -- u8 category, u32 amount.
    int32_t OnCredit(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint8_t category = CoA::Read::U8(msg);
        M().credits[category] = CoA::Read::U32(msg);
        RefreshCredits();
        return 1;
    }

    // FUN_1014e4d0: the build parameters a unit implies, as FUN_10158810 compares and copies them.
    void ParamsFromUnit(Build& out, uint64_t unit, int32_t spec) {
        out.guid = unit;
        out.spec = spec;
        const uint8_t cls = UnitClass(unit);
        out.classKey = cls;
        const uint32_t mode = CoA::GameMode::Mode();
        out.draft = (mode >> 3) & 1;
        out.wildcard = (mode >> 6) & 1;
        out.specDraft = CoA::GameMode::SpecBuildDraft(spec);
        out.qualities = (ConfigBool("CONFIG_CHARACTER_ADVANCEMENT_QUALITIES_ENABLED") && !((mode >> 3) & 1) && !((mode >> 6) & 1) && (mode & 0x400)) ? 1 : 0;
        out.hero = cls == 10;
        out.coa = cls >= 12 && cls <= 32;
        out.stock = StockClass(cls);
        out.u2c = CoA::ObjectAddon::Unit(ActivePlayerGuid())[2].value;   // FUN_102d9f00
        out.level = UnitLevel(unit);
        out.name.clear();
        out.u64 = 0;
        out.u65 = 0;
    }

    // FUN_10158810: copy the parameters that differ; true when any did.
    bool SyncParams(Build& b, const Build& p) {
        bool changed = false;
        auto sync = [&changed](auto& dst, const auto& src) {
            if (dst != src) {
                dst = src;
                changed = true;
            }
        };
        sync(b.guid, p.guid);
        sync(b.spec, p.spec);
        sync(b.classKey, p.classKey);
        sync(b.draft, p.draft);
        sync(b.wildcard, p.wildcard);
        sync(b.specDraft, p.specDraft);
        sync(b.qualities, p.qualities);
        sync(b.hero, p.hero);
        sync(b.coa, p.coa);
        sync(b.stock, p.stock);
        sync(b.u2c, p.u2c);
        sync(b.level, p.level);
        sync(b.name, p.name);
        sync(b.u64, p.u64);
        sync(b.u65, p.u65);
        return changed;
    }

    // FUN_10171260: SMSG 0x726 -- the known entries of the active spec's build, diffed into events.
    int32_t OnKnownEntries(void*, uint32_t, uint32_t, CDataStore* msg) {
        Mgr& m = M();
        bool created = false;

        if (!m.player) {
            EnsurePlayer();
            created = true;
        }

        if (!m.player->Get(m.player->spec)) {
            m.player->Ensure(m.player->spec);
            created = true;
        }

        Build* build = m.player->Get(m.player->spec);

        if (!build) {
            // The packet is left unread here, as the original does.
            if (created) {
                CoA::CA::ResetPendingBuild();
            }

            return 1;
        }

        const uint64_t unit = ActivePlayerGuid();

        if (UnitExists(unit)) {
            Build params;
            ParamsFromUnit(params, unit, CoA::CA::ActiveSpecIndex());

            if (SyncParams(*build, params)) {
                build->Prepare();
            }
        }

        Build old(*build, true);
        std::vector<Entry> fresh;   // FUN_10166760
        const uint32_t n = CoA::Read::U32(msg);

        if (CoA::Read::Failed(msg) || n > CoA::Read::Remaining(msg) / 21) {
            CoA::CA::ResetPendingBuild();   // the original reaches its reset whatever the packet held
            return 1;
        }

        fresh.reserve(n);

        for (uint32_t i = 0; i < n; ++i) {
            Entry e;
            e.id = CoA::Read::U32(msg);
            e.rank = CoA::Read::U32(msg);
            e.u0c = CoA::Read::U32(msg);
            e.locked = CoA::Read::U8(msg);
            e.u18 = CoA::Read::U32(msg);
            e.u1c = CoA::Read::U32(msg);
            fresh.push_back(e);
        }

        std::vector<std::pair<uint32_t, uint32_t>> changed;
        std::vector<std::pair<uint32_t, uint32_t>> learned;
        std::vector<std::pair<uint32_t, uint32_t>> rankChanged;
        std::vector<std::pair<uint32_t, uint32_t>> unlearned;
        std::vector<uint32_t> added;
        std::vector<uint32_t> removed;

        for (const Entry& e : fresh) {
            if (!old.Has(e.id)) {
                added.push_back(e.id);
                learned.emplace_back(e.id, e.rank);
            } else if (e.rank != old.RankOf(e.id) || (e.locked != 0) != old.LockedOf(e.id)) {
                changed.emplace_back(e.id, e.rank);

                if (e.rank != old.RankOf(e.id)) {
                    rankChanged.emplace_back(e.id, e.rank);
                }
            }
        }

        for (const Entry& e : old.entries) {
            bool still = false;

            for (const Entry& f : fresh) {
                still = still || f.id == e.id;
            }

            if (!still) {
                removed.push_back(e.id);
                unlearned.emplace_back(e.id, e.rank);
            }
        }

        build->entries = fresh;   // FUN_10158010 + FUN_10158e40(0)
        build->UpdatePointers(false);

        for (auto& c : changed) {
            CoA::Events::Signal("ASCENSION_KNOWN_ENTRY_UPDATED", "%u%u", c.first, c.second);
        }

        for (uint32_t id : added) {
            CoA::Events::Signal("ASCENSION_ENTRY_UPDATED", "%u", id);
        }

        for (uint32_t id : removed) {
            CoA::Events::Signal("ASCENSION_KNOWN_ENTRY_REMOVED", "%u", id);
        }

        for (auto& x : learned) {
            Fire(m.onLearned, x.first, x.second);
        }

        for (auto& x : rankChanged) {
            Fire(m.onRankChanged, x.first, x.second);
        }

        for (auto& x : unlearned) {
            Fire(m.onUnlearned, x.first, x.second);
        }

        if (!changed.empty() || !added.empty() || !removed.empty()) {
            CoA::Events::Signal("ASCENSION_KNOWN_ENTRIES_UPDATED", nullptr);
        }

        RefreshCredits();
        CoA::CA::ResetPendingBuild();
        return 1;
    }

    // FUN_10170360 / FUN_10170b50 / FUN_10170750: a result name, looked up in its enum only to decide success (index 0);
    // the event carries the name as sent.
    uint32_t ResultIndex(const std::string& s, const char* const* names, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            if (s == names[i]) {
                return i;
            }
        }

        return 1;
    }

    int32_t OnLearnResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x728
        const std::string result = CoA::Read::Str(msg);
        const uint32_t a = CoA::Read::U32(msg);
        const uint32_t b = CoA::Read::U32(msg);
        const bool ok = ResultIndex(result, kLearnResults, 0x2E) == 0;
        CoA::Events::Signal("CHARACTER_ADVANCEMENT_LEARN_RESULT", "%b%s%u%u", ok, result.c_str(), a, b);
        return 1;
    }

    int32_t OnUnlearnResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x729
        const std::string result = CoA::Read::Str(msg);
        const uint32_t a = CoA::Read::U32(msg);
        const bool ok = ResultIndex(result, kUnlearnResults, 0x17) == 0;
        CoA::Events::Signal("CHARACTER_ADVANCEMENT_UNLEARN_RESULT", "%b%s%u", ok, result.c_str(), a);
        return 1;
    }

    int32_t OnPurgeResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x72A
        const std::string result = CoA::Read::Str(msg);
        const bool ok = ResultIndex(result, kPurgeResults, 10) == 0;
        CoA::Events::Signal("CHARACTER_ADVANCEMENT_PURGE_ABILITIES_RESULT", "%b%s", ok, result.c_str());
        return 1;
    }

    // SMSG 0x72B (FUN_10170950): CHARACTER_ADVANCEMENT_PURGE_TALENTS_RESULT("%b%s", ok, result). ok is meant to be
    // "result is CA_PURGE_TALENTS_OK", but the original pushes a dword whose low byte alone holds it and whose upper bytes
    // are a leftover pointer -- it is always true.
    int32_t OnPurgeTalentsResult(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string result = CoA::Read::Str(msg);
        CoA::Events::Signal("CHARACTER_ADVANCEMENT_PURGE_TALENTS_RESULT", "%b%s", 1, result.c_str());
        return 1;
    }

    // SMSG 0x72C (FUN_10170f30): C string result, C string, u32, u32.
    int32_t OnUpdateEntriesResult(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string result = CoA::Read::Str(msg);
        const std::string second = CoA::Read::Str(msg);
        const uint32_t a = CoA::Read::U32(msg);
        const uint32_t b = CoA::Read::U32(msg);
        CoA::Events::Signal("CHARACTER_ADVANCEMENT_UPDATE_ENTRIES_RESULT", "%b%s%s%u%u", result == "CA_UPDATE_ENTRIES_OK" ? 1 : 0,
            result.c_str(), second.c_str(), a, b);
        return 1;
    }

    // SMSG 0x659 / 0x65B (FUN_101705e0 / FUN_10170dc0): u32 entry, C string result.
    void EntryResult(const char* event, CDataStore* msg) {
        const uint32_t entry = CoA::Read::U32(msg);
        const std::string result = CoA::Read::Str(msg);
        CoA::Events::Signal(event, "%s%u", result.c_str(), entry);
    }

    int32_t OnLockEntryResult(void*, uint32_t, uint32_t, CDataStore* msg) {
        EntryResult("CHARACTER_ADVANCEMENT_LOCK_ENTRY_RESULT", msg);
        return 1;
    }

    int32_t OnUnlockEntryResult(void*, uint32_t, uint32_t, CDataStore* msg) {
        EntryResult("CHARACTER_ADVANCEMENT_UNLOCK_ENTRY_RESULT", msg);
        return 1;
    }

    // SMSG 0x64A (FUN_101dd650): one CharacterAdvancement.dbc row. The wire follows the row's field order with each
    // string field as a C string in place and the byte fields (+0xCC/+0xCD, +0xE0, +0x128..+0x12C) as single bytes; the
    // rest of those dwords is whatever the stack held (zero here). An existing row is rewritten in place and re-indexed;
    // a new one is inserted, the whole index rebuilt and every browser filter reset.
    int32_t OnPatchEntry(void*, uint32_t, uint32_t, CDataStore* msg) {
        CoA::Dbc::Table& t = Rows();
        std::vector<uint8_t> row(0x1FC, 0);
        auto u32s = [&](uint32_t from, uint32_t to) {
            for (uint32_t off = from; off <= to; off += 4) {
                const uint32_t v = CoA::Read::U32(msg);
                memcpy(&row[off], &v, 4);
            }
        };
        auto str = [&](uint32_t off) {
            const uint32_t v = t.AddString(CoA::Read::Str(msg).c_str());
            memcpy(&row[off], &v, 4);
        };
        auto u8 = [&](uint32_t off) { row[off] = CoA::Read::U8(msg); };
        u32s(0x00, 0x00);
        str(0x04);
        u32s(0x08, 0x3C);
        str(0x40);
        u32s(0x44, 0x4C);
        str(0x50);
        u32s(0x54, 0x5C);
        str(0x60);
        u32s(0x64, 0xB8);

        for (uint32_t off = 0xBC; off <= 0xC8; off += 4) {
            str(off);
        }

        u8(0xCC);
        u8(0xCD);
        u32s(0xD0, 0xDC);
        u8(0xE0);
        u32s(0xE4, 0x11C);
        str(0x120);
        u32s(0x124, 0x124);

        for (uint32_t off = 0x128; off <= 0x12C; ++off) {
            u8(off);
        }

        u32s(0x130, 0x1A0);

        for (uint32_t off = 0x1A4; off <= 0x1AC; off += 4) {
            str(off);
        }

        u32s(0x1B0, 0x1F8);

        if (CoA::Read::Failed(msg)) {
            return 1;
        }

        uint32_t id;
        uint32_t cls;
        uint32_t tab;
        memcpy(&id, &row[0], 4);
        memcpy(&cls, &row[0x80], 4);
        memcpy(&tab, &row[0x84], 4);

        if (cls - 1 > 0x2E) {
            cls = 1;
        }

        if (tab - 1 > 0x5E) {
            tab = 1;
        }

        const bool inserted = !CoA::CA::FindRow(id);

        if (!inserted) {
            Row r = CoA::CA::FindRow(id);
            UnindexRow(r);                                            // FUN_1014a330(row, 0)
            UpdateAllPointers(true);
            memcpy(const_cast<uint8_t*>(r), row.data(), row.size());  // rewritten where it lies
            IndexRow(r);                                              // FUN_1014a330(row, 1)
            UpdateAllPointers(true);
            s_sugg.statsDirty = s_sugg.spellsDirty = s_sugg.dirty = true;   // FUN_103339e0/d0/f0(1)
        } else {
            t.Upsert(id, row);                                        // FUN_10133ac0
            BuildIndex();                                             // FUN_1014a530
            CoA::CAFilter::ResetEditor();                             // BuildCreator +0x228
            s_sugg.statsDirty = s_sugg.spellsDirty = s_sugg.dirty = true;
            CoA::CAFilter::ResetEntries();                            // mgr +0x2F8
            CoA::CAFilter::ResetCategories(false);                    // mgr +0x28
            CoA::CAFilter::ResetCategories(true);                     // mgr +0x190
            CoA::CAFilter::ResetEditor();                             // a second time, as the original does
        }

        Build* active = CoA::CA::ActiveBuild();

        if (active->Has(id)) {   // FUN_10155060
            CoA::Events::Signal("ASCENSION_KNOWN_ENTRY_UPDATED", "%u%u", id, active->RankOf(id));
        } else {
            CoA::Events::Signal("ASCENSION_ENTRY_UPDATED", "%u", id);
        }

        CoA::Events::Signal("CHARACTER_ADVANCEMENT_ENTRY_PATCHED", "%b%s%s%u", inserted, kClassTypes[cls - 1], kTabTypes[tab - 1], id);
        return 1;
    }

    // ---- CharacterAdvancementMgr::Update (FUN_10182780), once a second ------------------------------------------
    void Update() {
        Mgr& m = M();
        bool resetPending = false;
        bool levelUpdated = false;
        const int64_t now = static_cast<int64_t>(time(nullptr));
        const uint64_t unit = ActivePlayerGuid();

        if (now != m.lastUpdate && UnitExists(unit)) {
            m.lastUpdate = now;

            if (Player* player = FindPlayer(unit)) {
                if (player->guid != unit) {
                    player->guid = unit;
                }

                if (Build* b = player->Get(player->spec)) {
                    Build old(*b, true);
                    Build params;
                    ParamsFromUnit(params, unit, CoA::CA::ActiveSpecIndex());

                    if (SyncParams(*b, params)) {
                        b->Prepare();   // FUN_10157450 (SyncParams' second argument)

                        if (m.pending) {
                            m.pending->level = b->level;
                            m.pending->u2c = b->u2c;
                        }

                        resetPending = b->classKey != old.classKey || b->draft != old.draft || b->wildcard != old.wildcard ||
                            b->specDraft != old.specDraft || b->qualities != old.qualities || b->hero != old.hero || b->coa != old.coa ||
                            b->stock != old.stock || b->name != old.name || b->u64 != old.u64 || b->u65 != old.u65;
                        levelUpdated = b->level != old.level || b->u2c != old.u2c;
                    }
                }
            }
        }

        bool autoLearned = false;

        if (!resetPending && m.autoLearn) {
            // Every free 0x100000 row the pending build can take, one rank at a time.
            m.autoLearn = false;

            if (Build* p = m.pending) {
                ForEachRow([&](Row r) {
                    if (!CoA::CA::RowHasFlag(r, 0x100000)) {
                        return;
                    }

                    const uint32_t id = RowU32(r, 0);

                    while (p->AECost(r, 1) == 0 && p->TECost(r, 1) == 0 && p->ValidateLearn(0, id, {}, {}) == 0) {
                        p->AddRank(id);
                        autoLearned = true;
                    }
                });
            }
        }

        if (!resetPending && !autoLearned && m.pendingVersion != m.pendingSeen && m.pending) {
            m.pendingSeen = m.pendingVersion;
            // Update(void)::`48'::<lambda_1> (FUN_10183220), on the active build then the pending one: +0x44 counts the
            // ability rows it holds (FUN_101c6770), +0x48 the talents (FUN_101c6e80).
            auto count = [](Build* b) {
                b->u44 = 0;
                b->u48 = 0;

                for (const Entry& e : b->entries) {
                    if (Row r = CoA::CA::FindRow(e.id)) {
                        const int type = CoA::CA::RowType(r);

                        if (type == 1 || type == 4) {
                            ++b->u44;
                        } else if (type == 2) {
                            ++b->u48;
                        }
                    }
                }
            };

            if (Build* active = CoA::CA::ActiveBuild()) {
                count(active);
            }

            count(m.pending);
            CoA::CAFilter::RebuildRecentSets();   // FUN_101ca1d0, FUN_101c4360, FUN_101ca500 on mgr +0x190
            m.pendingSignal = true;
        }

        // CHARACTER_ADVANCEMENT_PENDING_BUILD_UPDATED waits until no learn/unlearn request is in flight.
        if (m.pendingSignal && m.requests.empty()) {
            m.pendingSignal = false;
            CoA::Events::Signal("CHARACTER_ADVANCEMENT_PENDING_BUILD_UPDATED", nullptr);
        }

        if (resetPending) {
            CoA::CA::ResetPendingBuild();
        }

        if (levelUpdated) {
            CoA::Events::Signal("CHARACTER_ADVANCEMENT_BUILD_LEVEL_UPDATED", nullptr);
        }
    }

    void OnGlueScreen() {   // FUN_1016ec70 (the parts this module owns)
        Mgr& m = M();

        for (auto& kv : m.inspect) {
            delete kv.second;
        }

        m.inspect.clear();
        KnownSpellSet().clear();     // +0x3E8 (FUN_100df440)
        RuneSpellSet().clear();      // +0x408
        CoA::AttachPatches::SetRuneUiClass(6);   // 0x728AA3 = 6 (FUN_10173330)
        KnownTagSet().clear();       // +0x428
        m.suggestionOverrides.clear();   // +0x44C
        m.pendingSignal = false;         // DAT_10bde424
        m.autoLearn = false;             // +0x458
        ++m.pendingVersion;              // DAT_10bde3d8
        // +0x00 (FUN_1016d170): the player's record goes too, so the next character's first 0x725 makes a fresh one.
        Player* old = m.player;
        m.player = nullptr;
        delete old;
    }

    // MysticEnchant's AE / TE investment requirement (FUN_102113B0 / FUN_10211440): the active build's spend for the
    // requirement's class / tab (class 1 "None" = global, tab 1 = the whole class).
    uint32_t Investment(uint32_t cls, uint32_t tab, bool te) {
        const Build* b = CoA::CA::ActiveBuild();

        if (cls == 1) {
            return te ? b->GlobalTE(0) : b->GlobalAE(0);
        }

        if (tab == 1) {
            return te ? b->ClassTE(cls, 0) : b->ClassAE(cls, 0);
        }

        return te ? b->TabTE(cls, tab, 0) : b->TabAE(cls, tab, 0);
    }
}

CoA::CA::detail::Suggestions& CoA::CA::detail::Sugg() {
    return s_sugg;
}

// ---- the spell-packet hooks (CAClient.cpp calls them around the client's handling) ---------------------------------
// FUN_10172660 on 0x6DF050 (SMSG_INITIAL_SPELLS), after the client's handling.
void CoA::CA::OnInitialSpells(const std::vector<uint32_t>& spells) {
    for (uint32_t spell : spells) {
        OnSpellLearned(spell, true);
    }

    if (!spells.empty()) {
        CoA::Events::Signal("ASCENSION_SPELLS_UPDATED", nullptr);
    }
}

// FUN_10172850 on 0x6E7D60: the client's learn messages are NOPed (FUN_10308830) for a flagged spell or a CA entry's.
bool CoA::CA::LearnedSpellIsQuiet(uint32_t spell) {
    return CustomAttrB(spell, 0x40000) || RowBySpell(FirstRank(spell)) != nullptr;
}

void CoA::CA::OnLearnedSpell(uint32_t spell) {
    OnSpellLearned(spell, false);
}

// FUN_101729d0 on 0x6E2240, after the client's handling.
void CoA::CA::OnRemovedSpells(const std::vector<uint32_t>& spells) {
    for (uint32_t spell : spells) {
        OnSpellUnlearned(spell);
    }
}

// FUN_101728e0 on 0x6E7840: a spell removed outside CharacterAdvancement leaves the known set first.
bool CoA::CA::RemovedSpellLeavesKnownSet(uint32_t spell) {
    const bool flag = CustomAttrB(spell, 0x40000);
    const bool entry = RowBySpell(FirstRank(spell)) != nullptr;
    return !detail::Wildcard() && !flag && !entry;   // detail::, not the CoA::Wildcard module
}

// FUN_10172b50 on 0x6E7E00: a rank replaced -- quietly when either rank is a CA entry or on a wildcard realm.
bool CoA::CA::SupersedeIsQuiet(uint32_t oldSpell, uint32_t newSpell) {
    const bool entry = RowBySpell(FirstRank(oldSpell)) != nullptr || RowBySpell(FirstRank(newSpell)) != nullptr;
    return detail::Wildcard() || entry;
}

void CoA::CA::OnSupersededSpell(uint32_t oldSpell, uint32_t newSpell) {
    OnSpellUnlearned(oldSpell);
    OnSpellLearned(newSpell, false);
}

// 0x1016FDE0 (FUN_10171f60 -> list 0x10BE2DD8, after 0x80B5D0): for a player casting a spell whose CA entry (FUN_101498d0
// over the spell's first rank) lists point requirements (+0x1D8 class, +0x1E4 tab, +0x1F0 points, up to three, the first
// zero ends them), the active non-wildcard build must meet each (FUN_101552e0 with no minimum). The caller checks the
// caster is the active player.
bool CoA::CA::CastRequirementsMet(uint32_t spell) {
    Build* build = ActiveBuild();

    if (!build || build->wildcard) {
        return true;
    }

    Row row = RowBySpell(FirstRank(spell));

    if (!row) {
        return true;
    }

    for (uint32_t i = 0; i < 3; ++i) {
        const uint32_t points = RowU32(row, 0x1F0 + i * 4);

        if (points == 0) {
            return true;
        }

        if (!build->MeetsPoints(RowU32(row, 0x1D8 + i * 4), RowU32(row, 0x1E4 + i * 4), points, 0)) {
            return false;
        }
    }

    return true;
}

void CoA::CA::EnterWorld() {   // 0x10172650 -> FUN_1014a530
    BuildIndex();
}

uint32_t CoA::CA::Credit(uint8_t category) {
    auto it = M().credits.find(category);
    return it == M().credits.end() ? 0 : it->second;
}

void CoA::CA::Init() {
    COA_PACKET(0x725, OnActiveSpec);
    COA_PACKET(0x659, OnLockEntryResult);
    COA_PACKET(0x72B, OnPurgeTalentsResult);
    COA_PACKET(0x72C, OnUpdateEntriesResult);
    COA_PACKET(0x65B, OnUnlockEntryResult);
    COA_PACKET(0x726, OnKnownEntries);
    COA_PACKET(0x728, OnLearnResult);
    COA_PACKET(0x729, OnUnlearnResult);
    COA_PACKET(0x72A, OnPurgeResult);
    COA_PACKET(0x926, OnCredit);
    CoA::Lifecycle::OnGlueScreen(ClearCredits);   // FUN_101a1b70
    COA_PACKET(0x64A, OnPatchEntry);
    CoA::Lifecycle::OnGlueScreen(OnGlueScreen);
    CoA::Lifecycle::OnAfterIdle(Update);
    CoA::GameMode::Dependencies().activeSpecIndex = CoA::CA::ActiveSpecIndex;
    CoA::MysticEnchant::Dependencies().investment = Investment;
    InitNatives();
    InitEdit();
    InitSuggest();
    InitLinks();
    CoA::CAFilter::Init();
    COA_EVENT("ASCENSION_CA_SPECIALIZATION_ACTIVE_ID_CHANGED");
    COA_EVENT("ASCENSION_KNOWN_ENTRY_UPDATED");
    COA_EVENT("ASCENSION_ENTRY_UPDATED");
    COA_EVENT("ASCENSION_KNOWN_ENTRY_REMOVED");
    COA_EVENT("ASCENSION_KNOWN_ENTRIES_UPDATED");
    COA_EVENT("CHARACTER_ADVANCEMENT_LEARN_RESULT");
    COA_EVENT("CHARACTER_ADVANCEMENT_UNLEARN_RESULT");
    COA_EVENT("CHARACTER_ADVANCEMENT_PURGE_ABILITIES_RESULT");
    COA_EVENT("CHARACTER_ADVANCEMENT_PURGE_TALENTS_RESULT");
    COA_EVENT("CHARACTER_ADVANCEMENT_UPDATE_ENTRIES_RESULT");
    COA_EVENT("CHARACTER_ADVANCEMENT_LOCK_ENTRY_RESULT");
    COA_EVENT("CHARACTER_ADVANCEMENT_UNLOCK_ENTRY_RESULT");
    COA_EVENT("CHARACTER_ADVANCEMENT_ENTRY_PATCHED");
    COA_EVENT("CHARACTER_ADVANCEMENT_PENDING_BUILD_UPDATED");
    COA_EVENT("CHARACTER_ADVANCEMENT_BUILD_LEVEL_UPDATED");
    COA_EVENT("ASCENSION_SPELLS_UPDATED");
    COA_EVENT("NOTABLE_SPELL_LEARNED");
    COA_EVENT("ASCENSION_HIDDEN_SPELL_LEARNED");
    COA_EVENT("ASCENSION_HIDDEN_SPELL_UNLEARNED");
}
