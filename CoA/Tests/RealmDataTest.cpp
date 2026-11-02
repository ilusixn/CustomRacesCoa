#include "CoA/Source/Modules/RealmData.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <Windows.h>
#include <cstdio>

namespace {
    void WriteFile(const char* path, const char* text) {
        FILE* f = fopen(path, "w");
        fputs(text, f);
        fclose(f);
    }
}

COA_TEST(RealmData_ReadsListArchive) {
    CoATest::TempCwd cwd;
    CreateDirectoryA("Data", nullptr);
    CreateDirectoryA("Data\\area-52", nullptr);
    WriteFile("Data\\area-52\\listarchive", "patch-D.MPQ\npatch-E.MPQ\n");
    std::vector<std::string> names = CoA::RealmData::ListArchive("Data\\area-52");
    CHECK((names == std::vector<std::string>{ "patch-D.MPQ", "patch-E.MPQ" }));
    CHECK(CoA::RealmData::ListArchive("Data\\missing").empty());
}

COA_TEST(RealmData_BuildInfoDefault12340) {
    CoATest::TempCwd cwd;
    CHECK(CoA::RealmData::BuildNumber("Data\\area-52") == 12340);
}

COA_TEST(RealmData_BuildInfoRead) {
    CoATest::TempCwd cwd;
    CreateDirectoryA("Data", nullptr);
    // The original's path has no separator before "buildinfo": Data\<path>buildinfo.
    WriteFile("Data\\area-52buildinfo", "12344\n");
    CHECK(CoA::RealmData::BuildNumber("Data\\area-52") == 12344);
}

COA_TEST(RealmData_CachePathPerRealm) {
    CHECK(CoA::RealmData::CachePath("enUS", "Pale Reach") == "Cache/WDB/enUS/Pale Reach");
}

COA_TEST(RealmData_SwitchOnlyForNewPath) {
    CoA::RealmData::ResetState("");
    CHECK(!CoA::RealmData::NeedsSwitch(""));
    CHECK(CoA::RealmData::NeedsSwitch("area-52"));
    CoA::RealmData::ResetState("area-52");
    CHECK(!CoA::RealmData::NeedsSwitch("area-52"));
    CHECK(CoA::RealmData::NeedsSwitch(""));
    CoA::RealmData::ResetState("");
}

COA_TEST(RealmData_HdOnlyArchiveNames) {
    CHECK(CoA::RealmData::IsHdOnlyArchive("Data\\patch-ZZZZZ-HD.MPQ"));
    CHECK(CoA::RealmData::IsHdOnlyArchive("data\\PATCH-zzzzz-hd.mpq"));
    CHECK(!CoA::RealmData::IsHdOnlyArchive("Data\\patch-ZZZZY.MPQ"));
    CHECK(!CoA::RealmData::IsHdOnlyArchive("Data\\patch-Q.MPQ"));
    CHECK(!CoA::RealmData::IsHdOnlyArchive("Data\\area-52\\patch-D-HD.MPQ"));
    CHECK(!CoA::RealmData::IsHdOnlyArchive("Data\\patch--HD.MPQ"));
    CHECK(!CoA::RealmData::IsHdOnlyArchive(nullptr));
}
