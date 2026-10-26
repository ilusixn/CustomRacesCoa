#include "CoA/Source/Runtime/DbcPatch.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
    struct Spec {
        const char* table;
        uint32_t structSize;
        uint64_t stringMask;
        CoA::DbcPatch::AfterPatch after;
        bool sizedStrings;          // FUN_100d3680: u32 length + bytes instead of a C string
        bool afterOnInsert = false; // after only when the row did not exist yet
    };

    struct Generated {
        uint32_t opcode;
        const char* table;
        uint32_t size;
        uint64_t mask;
    };

    const Generated s_generated[] = {
#include "CoA/Source/Runtime/DbcPatch.generated.inc"
    };

    std::map<uint32_t, Spec>& Specs() {
        static std::map<uint32_t, Spec> specs;
        return specs;
    }

    std::map<uint32_t, CoA::DbcPatch::Validator>& Validators() {
        static std::map<uint32_t, CoA::DbcPatch::Validator> validators;
        return validators;
    }

    int32_t OnPatch(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        auto it = Specs().find(opcode);

        if (it == Specs().end()) {
            return 1;
        }

        const Spec& spec = it->second;

        if (CoA::Read::Remaining(msg) < spec.structSize) {
            return 1;
        }

        std::vector<uint8_t> row(spec.structSize);
        CoA::Read::Data(msg, row.data(), spec.structSize);
        CoA::Dbc::Table& table = CoA::Dbc::Get(spec.table);
        std::map<uint32_t, std::string> strings;

        for (uint32_t f = 0; f < spec.structSize / 4 && f < 64; ++f) {
            if (spec.stringMask & (1ull << f)) {
                strings[f] = spec.sizedStrings ? CoA::Read::Name(msg) : CoA::Read::Str(msg);
            }
        }

        // CoA: the reconstruction's AscDbcPatch validator (hot-patch build) -- a refused row is not stored.
        auto validator = Validators().find(opcode);

        if (validator != Validators().end() && !validator->second(opcode, strings)) {
            return 1;
        }

        for (const auto& [f, value] : strings) {
            uint32_t offset = table.AddString(value.c_str());
            memcpy(&row[f * 4], &offset, 4);
        }

        uint32_t id;
        memcpy(&id, row.data(), 4);
        bool existed = table.Row(id) != nullptr;
        table.Upsert(id, row);

        if (spec.after && !(spec.afterOnInsert && existed)) {
            spec.after(table.Row(id));
        }

        return 1;
    }
}

void CoA::DbcPatch::Register(uint32_t opcode, const char* table, uint32_t structSize, uint64_t stringMask, CoA::DbcPatch::AfterPatch after) {
    Specs()[opcode] = Spec { table, structSize, stringMask, after, false };
    COA_PACKET(opcode, OnPatch);
}

void CoA::DbcPatch::RegisterInsertHook(uint32_t opcode, const char* table, uint32_t structSize, uint64_t stringMask, CoA::DbcPatch::AfterPatch after) {
    CoA::DbcPatch::Register(opcode, table, structSize, stringMask, after);
    Specs()[opcode].afterOnInsert = true;
}

void CoA::DbcPatch::RegisterSized(uint32_t opcode, const char* table, uint32_t structSize, uint64_t stringMask, CoA::DbcPatch::AfterPatch after) {
    Specs()[opcode] = Spec { table, structSize, stringMask, after, true };
    COA_PACKET(opcode, OnPatch);
}

void CoA::DbcPatch::SetValidator(uint32_t opcode, CoA::DbcPatch::Validator validate) {
    if (validate) {
        Validators()[opcode] = validate;
    } else {
        Validators().erase(opcode);
    }
}

void CoA::DbcPatch::Init() {
    for (const Generated& g : s_generated) {
        CoA::DbcPatch::Register(g.opcode, g.table, g.size, g.mask, nullptr);
    }
}
