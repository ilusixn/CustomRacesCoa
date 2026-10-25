// AscObjectAddon.cpp: SMSG_UPDATE_OBJECT_ADDON 0x578 (handler_0x0578, FUN_102ce0c0) and the per-GUID field stores.
//
// Packet: u64 guid, then {u32 index, u32 value} pairs to the end. The GUID's high word picks the store: high <
// 0x10000000 or kind 0xF130 / 0xF140 / 0xF150 -> units (0x58 fields); 0x4000 -> items (1 field); 0xF100 / 0xF101 /
// 0xF110 / 0x1FC0 -> their own (empty) stores; 0x1F50 and anything else -> ignored. Values are written only for
// indices inside the record; then, for each written index, the field's own callback and the kind's registered
// callback run.
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include <map>
#include <unordered_map>
#include <utility>

namespace {
    using Store = std::unordered_map<uint64_t, std::vector<CoA::ObjectAddon::Field>>;
    Store s_units;   // 0x10BE3648, 0x58 fields
    Store s_items;   // 0x10BE3608, 1 field
    Store s_f110;    // 0x10BE3668 (0xF110 / 0x1FC0)
    Store s_f100;    // 0x10BE3688
    Store s_f101;    // 0x10BE36A8
    std::map<std::pair<uint32_t, uint32_t>, std::pair<CoA::ObjectAddon::TypeCallback, void*>> s_typeCallbacks;   // 0x10BE36CC

    std::vector<CoA::ObjectAddon::Field>& Record(Store& store, uint64_t guid, uint32_t fields) {
        auto it = store.find(guid);

        if (it == store.end()) {
            it = store.emplace(guid, std::vector<CoA::ObjectAddon::Field>()).first;
            it->second.resize(fields);
        }

        return it->second;
    }

    int32_t OnObjectAddon(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint64_t guid = CoA::Read::U64(msg);
        const uint32_t hi = static_cast<uint32_t>(guid >> 32);
        const uint32_t k = hi >> 16;
        std::vector<CoA::ObjectAddon::Field>* rec = nullptr;

        if (hi < 0x10000000 || k == 0 || k == 0xF130 || k == 0xF140 || k == 0xF150) {
            rec = &CoA::ObjectAddon::Unit(guid);
        } else if (k == 0x4000) {
            rec = &CoA::ObjectAddon::Item(guid);
        } else if (k == 0xF100) {
            rec = &Record(s_f100, guid, 0);
        } else if (k == 0xF101) {
            rec = &Record(s_f101, guid, 0);
        } else if (k == 0xF110 || k == 0x1FC0) {
            rec = &Record(s_f110, guid, 0);
        } else {
            return 1;
        }

        // (The original loops while its read position differs from the size, reading past the packet on a trailing
        // half pair; this stops at the end.)
        std::vector<uint32_t> changed;

        while (CoA::Read::Remaining(msg) >= 8) {
            const uint32_t index = CoA::Read::U32(msg);
            const uint32_t value = CoA::Read::U32(msg);

            if (index < rec->size()) {
                (*rec)[index].value = value;
                changed.push_back(index);
            }
        }

        const uint32_t kind = CoA::ObjectAddon::Kind(guid);

        for (uint32_t index : changed) {
            CoA::ObjectAddon::Field& f = (*rec)[index];

            if (f.callback) {
                f.callback(guid, index, f.arg, &f);
            }

            const auto cb = s_typeCallbacks.find({ kind, index });

            if (cb != s_typeCallbacks.end() && cb->second.first) {
                cb->second.first(guid, index, cb->second.second, &f);
            }
        }

        return 1;
    }
}

std::vector<CoA::ObjectAddon::Field>& CoA::ObjectAddon::Unit(uint64_t guid) {
    return Record(s_units, guid, 0x58);
}

std::vector<CoA::ObjectAddon::Field>& CoA::ObjectAddon::Item(uint64_t guid) {
    return Record(s_items, guid, 1);
}

void CoA::ObjectAddon::RegisterTypeCallback(uint32_t kind, uint32_t index, TypeCallback fn, void* arg) {
    s_typeCallbacks[{ kind, index }] = { fn, arg };
}

uint32_t CoA::ObjectAddon::Kind(uint64_t guid) {
    const uint32_t high = static_cast<uint32_t>(guid >> 32);

    if ((high >> 28) == 0) {
        return 4;
    }

    switch (high >> 16) {
        case 0xF100:
            return 6;
        case 0xF101:
            return 7;
        case 0xF110:
        case 0x1FC0:
            return 5;
        case 0x4000:
            return 1;
        case 0xF130:
        case 0xF140:
        case 0xF150:
            return 3;
        default:
            return 0;
    }
}

void CoA::ObjectAddon::DropUnit(uint64_t guid) {
    s_units.erase(guid);
}

void CoA::ObjectAddon::DropAllUnitsExcept(uint64_t keep) {
    std::vector<Field> kept;
    bool had = false;

    if (keep) {
        const auto it = s_units.find(keep);

        if (it != s_units.end()) {
            kept = it->second;
            had = true;
        }
    }

    s_units.clear();

    if (had) {
        s_units.emplace(keep, kept);
    }
}

void CoA::ObjectAddon::Init() {
    COA_PACKET(0x578, OnObjectAddon);
}

void CoA::ObjectAddon::Reset() {
    s_units.clear();
    s_items.clear();
    s_f110.clear();
    s_f100.clear();
    s_f101.clear();
    s_typeCallbacks.clear();
}

size_t CoA::ObjectAddon::UnitCount() {
    return s_units.size();
}
