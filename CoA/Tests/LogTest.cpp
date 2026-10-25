#include "CoA/Source/Runtime/Log.h"
#include "CoA/Tests/TestMain.h"

COA_TEST(Log_ExtlogSwitchTurnsTheLogOn) {
    CHECK(CoA::Log::Requested("CoA64.exe -extlog 1"));
    CHECK(CoA::Log::Requested("\"D:\WoW\Coa\CoA64.exe\" -windowed /extlog 5"));
    CHECK(CoA::Log::Requested("CoA64.exe --extlog 1"));
}

COA_TEST(Log_ExtlogSwitchOffOrAbsent) {
    CHECK(!CoA::Log::Requested("CoA64.exe"));
    CHECK(!CoA::Log::Requested("CoA64.exe -extlog 0"));
    CHECK(!CoA::Log::Requested("CoA64.exe -extlog"));
    CHECK(!CoA::Log::Requested("CoA64.exe extlog 1"));
}
