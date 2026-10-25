// AscClientDbcPatch.cpp's wire reader (its ReadValues, recovered by running every handler in Unicorn: the item order
// and which wire value lands in which row dword are measured). The handlers are in ClientDbcPatchClient.cpp.
#include "CoA/Source/Modules/ClientDbcPatch.h"
#include "CoA/Source/Runtime/Read.h"
#include <cstdlib>
#include <string>

namespace {
    // FUN_10ae66d0 + strcpy_s: the row keeps a pointer to its own copy.
    uint64_t CopyString(const std::string& s) {
        char* p = static_cast<char*>(malloc(s.size() + 1));
        memcpy(p, s.data(), s.size());
        p[s.size()] = 0;
        return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
    }
}

CoA::ClientDbcPatch::Deps& CoA::ClientDbcPatch::Dependencies() {
    static Deps deps;
    return deps;
}

std::vector<uint64_t> CoA::ClientDbcPatch::ReadValues(const char* program, CDataStore* msg) {
    std::vector<uint64_t> v;
    uint32_t packed = 0;
    uint32_t bytePos = 0;

    for (const char* c = program; *c; ++c) {
        switch (*c) {
            case 'b':
                v.push_back(CoA::Read::U8(msg));
                break;
            case 'h':
                v.push_back(CoA::Read::U16(msg));
                break;
            case 'd':
                v.push_back(CoA::Read::U32(msg));
                break;
            case 'q':
                v.push_back(CoA::Read::U32(msg));
                v.push_back(CoA::Read::U32(msg));
                break;
            case 'x':
                for (int i = 0; i < 4; ++i) {
                    v.push_back(CoA::Read::U32(msg));
                }

                break;
            case '_':   // a byte the handler skips (e.g. an in-row string slot)
                CoA::Read::U8(msg);
                break;
            case 'L': {   // a 16-locale string, each u32 length + bytes; slot 0 is kept
                uint64_t first = 0;

                for (int i = 0; i < 16; ++i) {
                    const std::string s = CoA::Read::Name(msg);

                    if (i == 0) {
                        first = CopyString(s);
                    }
                }

                v.push_back(first);
                break;
            }
            case 'p':   // a u8 packed into the current value ('|' closes it)
                if (bytePos == 0) {
                    v.push_back(0);
                }

                v.back() |= static_cast<uint64_t>(CoA::Read::U8(msg)) << (8 * bytePos);
                ++bytePos;
                break;
            case '|':
                bytePos = 0;
                break;
            case 'B': {   // a u32 of which only the low byte is kept; four pack into one value
                const uint32_t b = CoA::Read::U32(msg) & 0xFF;

                if (packed == 0) {
                    v.push_back(0);
                }

                v.back() |= static_cast<uint64_t>(b) << (8 * packed);
                packed = (packed + 1) & 3;
                break;
            }
            case 's':
                v.push_back(CopyString(CoA::Read::Str(msg)));
                break;
            case 'z':
                v.push_back(CopyString(CoA::Read::Name(msg)));
                break;
        }
    }

    return v;
}
