#ifndef COA_SOURCE_MODULES_CONFIG_H
#define COA_SOURCE_MODULES_CONFIG_H

#include <cstdint>
#include <vector>

class CDataStore;

// The server config store (FUN_101982d0 -> static 0x10BDEC20), filled by SMSG_UPDATE_CONFIGS 0x58D; C_Config
// (FUN_10198370 .. FUN_10198940). Port of AscConfig.cpp. Lookups return nullptr for an unknown name.
//
// The packet replaces the int, bool, float and rate maps wholesale:
//   u32 n x {u32 len, name[len], i32}   ints
//   u32 n x {u32 len, name[len], u8}    bools
//   u32 n x {u32 len, name[len], f32}   floats
//   u32 n x {u32 len, name[len], f32}   rates
// and, only if at least 8 bytes remain, merges into the vector maps:
//   u32 n x {u32 len, name[len], u32 k, i32[k]}   int vectors
//   u32 n x {u32 len, name[len], u32 k, f32[k]}   float vectors
// While in world (CGGameUI::s_inWorld) each scalar whose value differs from the previous packet's (a missing
// previous value counts as 0 / false) fires INT_/BOOL_/FLOAT_/RATE_CONFIG_UPDATED(name, old, new).
namespace CoA::Config {
    const int32_t* Int(const char* name);                      // FUN_10196ad0
    const bool* Bool(const char* name);                        // FUN_10196910
    const float* Float(const char* name);                      // FUN_101969b0
    const float* Rate(const char* name);                       // FUN_10196c10
    const std::vector<int32_t>* IntVector(const char* name);   // FUN_10196b50
    const std::vector<float>* FloatVector(const char* name);   // FUN_10196a30

    int32_t OnUpdateConfigs(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);

    // Tests only.
    void Reset();
}

#endif
