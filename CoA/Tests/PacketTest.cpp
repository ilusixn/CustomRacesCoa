#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>

namespace {
    bool BytesAre(CDataStore* store, const uint8_t* expected, uint32_t size) {
        return store->m_size == size && std::memcmp(store->m_data, expected, size) == 0;
    }
}

COA_TEST(Packet_LayoutIsLittleEndianWithOpcodeFirst) {
    CoA::Packet p(0x55A);
    p.U8(0x11).U16(0x2233).U32(0x44556677).F32(1.0f);
    const uint8_t expected[] = {
        0x5A, 0x05, 0x00, 0x00,
        0x11,
        0x33, 0x22,
        0x77, 0x66, 0x55, 0x44,
        0x00, 0x00, 0x80, 0x3F,
    };
    CHECK(BytesAre(p.Store(), expected, sizeof(expected)));
}

COA_TEST(Packet_LengthPrefixedDataHasNoTerminator) {
    CoA::Packet p(0x55A);
    p.U32(3).Data("abc", 3);
    const uint8_t expected[] = { 0x5A, 0x05, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 'a', 'b', 'c' };
    CHECK(BytesAre(p.Store(), expected, sizeof(expected)));
}

COA_TEST(Packet_StrIncludesTerminatorAndNullIsEmpty) {
    CoA::Packet p(0x55A);
    p.Str("hi").Str(nullptr);
    const uint8_t expected[] = { 0x5A, 0x05, 0x00, 0x00, 'h', 'i', 0x00, 0x00 };
    CHECK(BytesAre(p.Store(), expected, sizeof(expected)));
}

COA_TEST(Packet_PackedGuidMasksNonZeroBytesLowFirst) {
    CoA::Packet p(0x55A);
    p.PackedGuid(0xF130000000AB00CDull).PackedGuid(0);
    const uint8_t expected[] = { 0x5A, 0x05, 0x00, 0x00, 0xC5, 0xCD, 0xAB, 0x30, 0xF1, 0x00 };
    CHECK(BytesAre(p.Store(), expected, sizeof(expected)));
}

COA_TEST(Packet_U64) {
    CoA::Packet p(0x55A);
    p.U64(0x0102030405060708ull);
    const uint8_t expected[] = { 0x5A, 0x05, 0x00, 0x00, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 };
    CHECK(BytesAre(p.Store(), expected, sizeof(expected)));
}
