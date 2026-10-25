#ifndef COA_SOURCE_MODULES_CURSOR_H
#define COA_SOURCE_MODULES_CURSOR_H

#include <cstdint>

struct CVar;

// The cursor module's glue CVar (AscCursor.cpp, installer FUN_10a1e950): cursorSizePreferred's size and texture
// variant. The software cursor that draws with them comes with the cursor module.
namespace CoA::Cursor {
    // FUN_10a1e320: "1" / "2" / "3" / "4" pick 48 / 64 / 96 / 128 px, anything else 32 (variant 0). The variant becomes
    // 2 ("1".."3") or 4 ("4") only when Data\Patch-Q.mpq opens; otherwise a non-zero size keeps the previous one.
    void SizePreferred(const char* value, bool hdAvailable);
    bool OnSizePreferred(CVar* var, const char* oldValue, const char* value, void* arg);
    uint32_t Size();
    uint32_t Variant();
}

#endif
