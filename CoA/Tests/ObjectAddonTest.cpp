#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <string>
#include <vector>

namespace {
    std::vector<std::string> s_calls;

    void Feed(CoATest::Bytes bytes) {
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(0x578, 0, &msg, result);
    }

    void Setup() {
        CoA::Packets::Clear();
        CoA::ObjectAddon::Reset();
        CoA::ObjectAddon::Init();
        s_calls.clear();
    }

    void FieldCallback(uint64_t guid, uint32_t index, void*, CoA::ObjectAddon::Field* f) {
        s_calls.push_back("field " + std::to_string(index) + "=" + std::to_string(f->value));
    }

    void TypeCallback(uint64_t guid, uint32_t index, void* arg, CoA::ObjectAddon::Field* f) {
        s_calls.push_back("type " + std::to_string(reinterpret_cast<uintptr_t>(arg)) + " " + std::to_string(index));
    }
}

// SMSG 0x578: {u64 guid, (u32 index, u32 value)...}; the field's callback, then the kind's callback, per written index.
COA_TEST(ObjectAddon_UnitFieldsAndCallbacks) {
    Setup();
    CoA::ObjectAddon::Unit(0x1234)[3].callback = FieldCallback;
    CoA::ObjectAddon::RegisterTypeCallback(4, 3, TypeCallback, reinterpret_cast<void*>(4));
    Feed(CoATest::Bytes().U64(0x1234).U32(3).U32(7).U32(86).U32(9));
    CHECK(CoA::ObjectAddon::Unit(0x1234)[3].value == 7);
    CHECK(CoA::ObjectAddon::Unit(0x1234)[86].value == 9);
    CHECK(CoA::ObjectAddon::Unit(0x1234).size() == 0x58);
    CHECK((s_calls == std::vector<std::string>{ "field 3=7", "type 4 3" }));
}

// An index past the record is skipped and the next pair still applies; items get one field; unknown kinds nothing.
COA_TEST(ObjectAddon_OutOfRangeAndKinds) {
    Setup();
    Feed(CoATest::Bytes().U64(0xF130000000000055ull).U32(0x58).U32(1).U32(2).U32(5));
    CHECK(CoA::ObjectAddon::Unit(0xF130000000000055ull)[2].value == 5);

    Feed(CoATest::Bytes().U64(0x4000000000000077ull).U32(0).U32(42).U32(1).U32(43));
    CHECK(CoA::ObjectAddon::Item(0x4000000000000077ull).size() == 1);
    CHECK(CoA::ObjectAddon::Item(0x4000000000000077ull)[0].value == 42);

    const size_t units = CoA::ObjectAddon::UnitCount();
    Feed(CoATest::Bytes().U64(0x1F50000000000001ull).U32(0).U32(1));
    CHECK(CoA::ObjectAddon::UnitCount() == units);

    // A trailing half pair stops the reader (the original reads past the packet).
    Feed(CoATest::Bytes().U64(0x99).U32(1).U32(11).U32(2));
    CHECK(CoA::ObjectAddon::Unit(0x99)[1].value == 11);

    CHECK(CoA::ObjectAddon::Kind(0x0000000000001234ull) == 4);
    CHECK(CoA::ObjectAddon::Kind(0x4000000000000001ull) == 1);
    CHECK(CoA::ObjectAddon::Kind(0xF130000000000001ull) == 3);
    CHECK(CoA::ObjectAddon::Kind(0xF140000000000001ull) == 3);
    CHECK(CoA::ObjectAddon::Kind(0xF150000000000001ull) == 3);
    CHECK(CoA::ObjectAddon::Kind(0xF110000000000001ull) == 5);
    CHECK(CoA::ObjectAddon::Kind(0x1FC0000000000001ull) == 5);
    CHECK(CoA::ObjectAddon::Kind(0xF100000000000001ull) == 6);
    CHECK(CoA::ObjectAddon::Kind(0xF101000000000001ull) == 7);
    CHECK(CoA::ObjectAddon::Kind(0x1F50000000000001ull) == 0);
}

// An object leaving the client drops its unit record; every second SMSG_INITIAL_SPELLS keeps only the player's.
COA_TEST(ObjectAddon_Lifecycle) {
    Setup();
    CoA::ObjectAddon::Unit(1)[0].value = 1;
    CoA::ObjectAddon::Unit(2)[0].value = 2;
    CoA::ObjectAddon::Unit(3)[0].value = 3;
    CoA::ObjectAddon::DropAllUnitsExcept(1);
    CHECK(CoA::ObjectAddon::UnitCount() == 1);
    CHECK(CoA::ObjectAddon::Unit(1)[0].value == 1);
    CoA::ObjectAddon::DropUnit(1);
    CHECK(CoA::ObjectAddon::Unit(1)[0].value == 0);
}
