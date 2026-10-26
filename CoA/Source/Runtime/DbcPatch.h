#ifndef COA_SOURCE_RUNTIME_DBCPATCH_H
#define COA_SOURCE_RUNTIME_DBCPATCH_H

#include <cstdint>
#include <map>
#include <string>

// SMSG_PATCH_* -- the server hot-patching the DLL's DBC tables (port of AscDbcPatch; module init FUN_101e5880).
//
// Wire format, identical across handlers: the raw struct image (structSize bytes; string-pointer fields carry
// garbage) followed by one C string per string field in struct order (stringMask bit n = field n). The row with the
// struct's first dword as id is overwritten, or inserted (FUN_10133ac0). An image the packet does not hold is
// ignored. Pure patches come from DbcPatch.generated.inc; handlers with side effects register with a callback run
// after the row is stored.
namespace CoA::DbcPatch {
    using AfterPatch = void (*)(const uint8_t* row);

    void Register(uint32_t opcode, const char* table, uint32_t structSize, uint64_t stringMask, AfterPatch after);
    // after runs only when the row was inserted (not when overwritten).
    void RegisterInsertHook(uint32_t opcode, const char* table, uint32_t structSize, uint64_t stringMask, AfterPatch after);
    // Strings arrive as u32 length + bytes (FUN_100d3680).
    void RegisterSized(uint32_t opcode, const char* table, uint32_t structSize, uint64_t stringMask, AfterPatch after);

    // The reconstruction's AscDbcPatch::SetValidator (hot-patch build, not in the original): a check run on a row's
    // string fields (field index -> text) before it is stored. Returning false drops the row; the client keeps the row
    // it had. HotPatch uses it for 0x5BC.
    using Validator = bool (*)(uint32_t opcode, const std::map<uint32_t, std::string>& strings);
    void SetValidator(uint32_t opcode, Validator validate);

    // The generated pure patches.
    void Init();
}

#endif
