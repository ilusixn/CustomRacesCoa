#ifndef COA_SOURCE_MODULES_POI_H
#define COA_SOURCE_MODULES_POI_H

#include <cstdint>

class CDataStore;

// AscPOI.cpp: POIMgr (FUN_102cf0d0 -> static 0x10BE36E8), an unordered_map<string, POI> keyed by the POI's first
// string; GetPOIInfo(index) walks it in its (MSVC unordered_map) iteration order. Strings on the wire are u32 length +
// bytes (FUN_100d3680). Checked against the genuine DLL.
//   SMSG_POI_ADD 0x90F (FUN_102cf1f0): str key, str text, u32 a, u32 b, u32 c, f32 d, f32 e; ASCENSION_POI_ADD
//     ("%s%s%u%u%u%f%f") first, then inserted or overwritten.
//   SMSG_POI_REMOVE 0x910 (FUN_102cf7e0): str key; ASCENSION_POI_REMOVE("%s"), then erased.
//   SMSG_POI_CLEAR 0x911 (FUN_102cf6a0): ASCENSION_POI_CLEAR, then cleared.
// #Minimap.SetPOIMaxRange lives in WidgetClient.cpp. Nothing resets the map.
namespace CoA::POI {
    int32_t OnAdd(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    int32_t OnRemove(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    int32_t OnClear(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);

    void Init();
    // Tests only: the map empty.
    void Reset();
}

#endif
