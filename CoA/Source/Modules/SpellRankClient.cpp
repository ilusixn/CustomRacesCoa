// AscSpellRank.cpp (see SpellRank.h): the maps over SpellRank.dbc and the client's Talent.dbc.
#include "CoA/Source/Modules/SpellRank.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include <unordered_map>

namespace {
    CoA::Dbc::Table& Ranks() {
        return CoA::Dbc::Get("DBFilesClient\\SpellRank.dbc");
    }

    // Every Talent.dbc rank chain (9 rank columns), in id order: fn(spell, rank index 0..8, the talent's first rank).
    template <class Fn>
    void ForEachTalentRank(Fn fn) {
        for (int32_t id = g_talentDB.m_minID; id < g_talentDB.m_maxID + 1; ++id) {
            const TalentRec* talent = g_talentDB.GetRecord(id);

            if (!talent) {
                continue;
            }

            for (uint32_t r = 0; r < 9; ++r) {
                if (const uint32_t spell = static_cast<uint32_t>(talent->m_spellRank[r])) {
                    fn(spell, r, static_cast<uint32_t>(talent->m_spellRank[0]));
                }
            }
        }
    }

    // spell -> first rank: SpellRank.dbc rows, then each Talent.dbc rank.
    std::unordered_map<uint32_t, uint32_t>& FirstRanks() {
        static std::unordered_map<uint32_t, uint32_t> m;
        static bool built = false;

        if (!built) {
            built = true;
            CoA::Dbc::Table& ranks = Ranks();

            for (uint32_t id = ranks.MinId(); ranks.Loaded() && id <= ranks.MaxId(); ++id) {
                if (const uint8_t* row = ranks.Row(id)) {
                    m[CoA::Dbc::Table::U32(row, 8)] = CoA::Dbc::Table::U32(row, 4);
                }
            }

            ForEachTalentRank([](uint32_t spell, uint32_t, uint32_t first) { m[spell] = first; });
        }

        return m;
    }

    // spell -> rank number: SpellRank.dbc +0xC, then each Talent.dbc rank's 1-based position.
    std::unordered_map<uint32_t, uint32_t>& RankNumbers() {
        static std::unordered_map<uint32_t, uint32_t> m;
        static bool built = false;

        if (!built) {
            built = true;
            CoA::Dbc::Table& ranks = Ranks();

            for (uint32_t id = ranks.MinId(); ranks.Loaded() && id <= ranks.MaxId(); ++id) {
                if (const uint8_t* row = ranks.Row(id)) {
                    m[CoA::Dbc::Table::U32(row, 8)] = CoA::Dbc::Table::U32(row, 0xC);
                }
            }

            ForEachTalentRank([](uint32_t spell, uint32_t r, uint32_t) { m[spell] = r + 1; });
        }

        return m;
    }
}

uint32_t CoA::SpellRank::FirstRank(uint32_t spell) {
    auto& m = FirstRanks();
    auto it = m.find(spell);
    return it != m.end() ? it->second : spell;
}

uint32_t CoA::SpellRank::RankNumber(uint32_t spell) {
    auto& m = RankNumbers();
    auto it = m.find(spell);
    return it != m.end() ? it->second : 1;
}

const std::vector<uint32_t>& CoA::SpellRank::Chain(uint32_t firstRank) {
    static std::unordered_map<uint32_t, std::vector<uint32_t>> chains;
    static bool built = false;
    static const std::vector<uint32_t> none;

    if (!built) {
        built = true;
        CoA::Dbc::Table& ranks = Ranks();

        for (uint32_t id = ranks.MinId(); ranks.Loaded() && id <= ranks.MaxId(); ++id) {
            if (const uint8_t* row = ranks.Row(id)) {
                chains[CoA::Dbc::Table::U32(row, 4)].push_back(CoA::Dbc::Table::U32(row, 8));
            }
        }

        ForEachTalentRank([](uint32_t spell, uint32_t, uint32_t first) { chains[first].push_back(spell); });
    }

    auto it = chains.find(firstRank);
    return it == chains.end() ? none : it->second;
}
