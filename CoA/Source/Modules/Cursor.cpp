#include "CoA/Source/Modules/Cursor.h"
#include <cstring>
#include <fstream>
#include <share.h>

namespace {
    uint32_t s_size = 32;      // the pixel size behind 0x10D3CBCC / 0x10D3CBD0
    uint32_t s_variant = 0;    // 0x10D3CBD4
}

void CoA::Cursor::SizePreferred(const char* value, bool hdAvailable) {
    if (value && strcmp(value, "0") != 0) {
        if (strcmp(value, "1") == 0 || strcmp(value, "2") == 0 || strcmp(value, "3") == 0) {
            s_size = value[0] == '1' ? 48 : value[0] == '2' ? 64 : 96;

            if (hdAvailable) {
                s_variant = 2;
            }

            return;
        }

        if (strcmp(value, "4") == 0) {
            s_size = 128;

            if (hdAvailable) {
                s_variant = 4;
            }

            return;
        }
    }

    s_size = 32;
    s_variant = 0;
}

bool CoA::Cursor::OnSizePreferred(CVar*, const char*, const char* value, void*) {
    // The original probes the HD archive with a shared-read ifstream.
    std::ifstream probe("Data\\Patch-Q.mpq", std::ios::in, _SH_DENYNO);
    const bool hd = probe.is_open();
    probe.close();
    CoA::Cursor::SizePreferred(value, hd);
    return true;
}

uint32_t CoA::Cursor::Size() {
    return s_size;
}

uint32_t CoA::Cursor::Variant() {
    return s_variant;
}
