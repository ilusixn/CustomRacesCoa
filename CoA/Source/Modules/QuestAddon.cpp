// AscQuestAddon.cpp (see QuestAddon.h): the quest addon cache behind CMSG 0x731 / SMSG 0x732.
#include "CoA/Source/Modules/QuestAddon.h"
#include "CoA/Source/Modules/AttachLua.h"
#include "CoA/Source/Modules/Callboard.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/TemporalContracts.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include <iterator>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
    struct Entry {
        CoA::QuestAddon::Record record;                                            // +0x0C
        std::vector<std::pair<CoA::QuestAddon::Callback, void*>> waiting;          // +0x1C
        bool loaded = false;                                                       // +0x24
    };

    std::unordered_map<uint32_t, Entry> s_entries;   // 0x10BDF3A8
    uint32_t s_version = 0;                          // 0x10BDF3CC

    // The consumers' questAddonPoints: the loaded record's points (an unknown quest is requested).
    bool Points(uint32_t quest, uint32_t& points) {
        const CoA::QuestAddon::Record* r = CoA::QuestAddon::Get(quest);

        if (!r) {
            return false;
        }

        points = r->points;
        return true;
    }
}

const CoA::QuestAddon::Record* CoA::QuestAddon::Get(uint32_t quest, Callback cb, void* arg) {
    if (quest == 0) {
        return nullptr;
    }

    auto it = s_entries.find(quest);

    if (it == s_entries.end()) {
        Entry& e = s_entries[quest];

        if (cb) {
            e.waiting.emplace_back(cb, arg);
        }

        CoA::Packet(0x731).U32(quest).Send();
        return nullptr;
    }

    if (it->second.loaded) {
        return &it->second.record;
    }

    if (cb) {
        it->second.waiting.emplace_back(cb, arg);
    }

    return nullptr;
}

// SMSG 0x732 (FUN_101af9f0): three u32 read unchecked (a short packet reads 0 -- CoA::Read).
int32_t CoA::QuestAddon::OnResponse(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    const uint32_t quest = CoA::Read::U32(msg);
    const uint32_t points = CoA::Read::U32(msg);
    const uint32_t unk = CoA::Read::U32(msg);
    auto it = s_entries.find(quest);

    if (it == s_entries.end()) {
        return 1;
    }

    Entry& e = it->second;
    e.record = { quest, points, unk, quest };
    e.loaded = true;
    const std::vector<std::pair<Callback, void*>> waiting = std::move(e.waiting);
    e.waiting.clear();

    for (const auto& w : waiting) {
        w.first(quest, w.second);
    }

    return 1;
}

void CoA::QuestAddon::CacheVersion(uint32_t version) {
    if (s_version != version) {
        s_entries.clear();
        s_version = version;
    }
}

uint32_t& CoA::QuestAddon::Version() {
    return s_version;
}

size_t CoA::QuestAddon::Count() {
    return s_entries.size();
}

void CoA::QuestAddon::ForEachRecord(void (*cb)(const Record& record, void* ctx), void* ctx) {
    for (auto& kv : s_entries) {
        cb(kv.second.record, ctx);
    }
}

void CoA::QuestAddon::PruneUnloaded() {
    for (auto it = s_entries.begin(); it != s_entries.end();) {
        it = it->second.loaded ? std::next(it) : s_entries.erase(it);
    }
}

void CoA::QuestAddon::ClearStore() {
    s_entries.clear();
}

void CoA::QuestAddon::Init() {
    COA_PACKET(0x732, CoA::QuestAddon::OnResponse);
    CoA::AttachLua::clientCacheVersion.Add(CoA::QuestAddon::CacheVersion);
    CoA::CallboardCache::Dependencies().questAddonPoints = Points;
    CoA::TemporalContracts::Dependencies().questAddonPoints = Points;
}

void CoA::QuestAddon::Reset() {
    s_entries.clear();
    s_version = 0;
}
