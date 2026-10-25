#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include <cstring>

namespace {
    CVar* s_characterSlot;
    CVar* s_accountSlot;
    CVar* s_glueSlot;
}

COA_TEST(CVars_WorldRegistrationWritesTheSlot) {
    CoA::CVars::Reset();
    s_characterSlot = nullptr;
    COA_CVAR("coaTestCharacter", nullptr, 0x21, "1", nullptr, 4, false, &s_characterSlot, true);
    CoA::CVars::RegisterWorld();
    CHECK(s_characterSlot != nullptr);
    CHECK(CVar::Lookup("coaTestCharacter") == s_characterSlot);
}

COA_TEST(CVars_CharacterCVarIsRecreatedAfterTheClientDeletesIt) {
    CoA::CVars::Reset();
    s_characterSlot = nullptr;
    COA_CVAR("coaTestCharacter2", nullptr, 0x21, "1", nullptr, 4, false, &s_characterSlot, true);
    CoA::CVars::RegisterWorld();
    CHECK(CVar::Lookup("coaTestCharacter2") != nullptr);

    // Logout: the client's own per-character delete (archive + (flags & 0x30) == 0x20).
    CVar::DeleteCharacterCVars();
    CHECK(CVar::Lookup("coaTestCharacter2") == nullptr);

    CoA::CVars::RegisterWorld();
    CHECK(s_characterSlot != nullptr);
    CHECK(CVar::Lookup("coaTestCharacter2") == s_characterSlot);
}

COA_TEST(CVars_GlueListOnlyRegistersAtGlue) {
    CoA::CVars::Reset();
    s_glueSlot = nullptr;
    s_accountSlot = nullptr;
    COA_CVAR("coaTestGlue", nullptr, 0x1, "0", nullptr, 1, false, &s_glueSlot, false);
    COA_CVAR("coaTestWorldOnly", nullptr, 0x1, "0", nullptr, 1, false, &s_accountSlot, true);
    CoA::CVars::RegisterGlue();
    CHECK(s_glueSlot != nullptr);
    CHECK(CVar::Lookup("coaTestGlue") != nullptr);
    CHECK(CVar::Lookup("coaTestWorldOnly") == nullptr);
    CHECK(s_accountSlot == nullptr);
}

COA_TEST(CVars_DefaultValueAndOrder) {
    CoA::CVars::Reset();
    COA_CVAR("coaTestOrderA", nullptr, 0x1, "abc", nullptr, 1, false, nullptr, true);
    COA_CVAR("coaTestOrderB", nullptr, 0x1, "def", nullptr, 1, false, nullptr, true);
    size_t count = 0;
    const CoA::CVarEntry* world = CoA::CVars::World(count);
    CHECK(count == 2);

    if (count == 2) {
        CHECK(std::strcmp(world[0].name, "coaTestOrderA") == 0);
        CHECK(std::strcmp(world[1].name, "coaTestOrderB") == 0);
    }

    CoA::CVars::RegisterWorld();
    CVar* a = CVar::Lookup("coaTestOrderA");
    CHECK(a && std::strcmp(a->m_stringValue.m_str, "abc") == 0);
}
