#ifndef COA_SOURCE_MODULES_TOKEN_H
#define COA_SOURCE_MODULES_TOKEN_H

#include <cstdint>

// AscToken.cpp: C_Token (FUN_1033cf10 / FUN_1033d040 / FUN_1033d2d0) and the token manager behind it.
// SMSG_TOKEN_LIST 0x66F (FUN_1033c8a0): u32 n, then n x {str name, u32 amount, u32 extra}; unknown names are skipped
// without consuming their two dwords (as the original does). SMSG_TOKEN_UPDATE 0x670 (FUN_1033cb60): str name, u32
// amount, u32 extra; TOKEN_UPDATED("%s%u%u", name, old amount, new amount) when either dword changed. Every glue screen
// zeroes the amounts (FUN_1033c260).
namespace CoA::Token {
    constexpr uint32_t kCount = 0x3D;
    bool TypeIndex(const char* name, uint32_t& index);   // FUN_102fe780 (62 names incl. TOKEN_TYPE_MAX)
    const char* TypeName(uint32_t index);                // PTR_s_TOKEN_TYPE_..._10b58850[index], nullptr >= 62
    int32_t Amount(uint32_t index);                      // 0 (DAT_10b5dbb0) when out of range
    uint64_t Amount64(uint32_t index);                   // FUN_1033c290: {extra:amount}
    // FUN_1033cd90: the entry's {extra:amount} as a u64 is >= {hi:lo}; out of range reads zeros.
    bool AtLeast(uint32_t index, uint32_t lo, uint32_t hi);

    void Init();
    // Tests only.
    void Reset();
}

#endif
