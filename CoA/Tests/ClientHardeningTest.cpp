#include "CoA/Tests/TestMain.h"
#include "WoW/Source/Glue/PatchCommandPolicy.h"

// prepatch.lst "extract" / "delete" (0x4E4F70 / 0x4E5000) run unauthenticated on Ascension (patch 0x4E57FE): their
// paths must stay inside the download folder and extract may not drop runnable files.
COA_TEST(Hardening_PatchPathsStayInTheDownloadFolder) {
    CHECK(PatchPathConfined("wow-patch.mpq"));
    CHECK(PatchPathConfined("Data\\enUS\\patch-enUS-4.MPQ"));
    CHECK(!PatchPathConfined("..\\evil.mpq"));
    CHECK(!PatchPathConfined("Data\\..\\..\\evil.mpq"));
    CHECK(!PatchPathConfined("Data/../../evil.mpq"));
    CHECK(!PatchPathConfined("C:\\Users\\x\\Startup\\evil.mpq"));
    CHECK(!PatchPathConfined("\\Windows\\evil.mpq"));
    CHECK(!PatchPathConfined("/evil.mpq"));
    CHECK(!PatchPathConfined("\\\\attacker\\share\\x"));
    CHECK(!PatchPathConfined("file.mpq:stream"));
    CHECK(!PatchPathConfined(""));
    CHECK(!PatchPathConfined(nullptr));
}

COA_TEST(Hardening_PatchExtractRefusesRunnableFiles) {
    CHECK(PatchExtractAllowed("Data\\enUS\\patch-enUS-4.MPQ"));
    CHECK(PatchExtractAllowed("realmlist.wtf"));
    CHECK(!PatchExtractAllowed("Wow.exe"));
    CHECK(!PatchExtractAllowed("d3d9.DLL"));
    CHECK(!PatchExtractAllowed("x64\\d3d9.dll"));
    CHECK(!PatchExtractAllowed("run.bat"));
    CHECK(!PatchExtractAllowed("run.cmd"));
    CHECK(!PatchExtractAllowed("a.ps1"));
    CHECK(!PatchExtractAllowed("a.vbs"));
    CHECK(!PatchExtractAllowed("a.js"));
    CHECK(!PatchExtractAllowed("a.scr"));
    CHECK(!PatchExtractAllowed("a.msi"));
    CHECK(!PatchExtractAllowed("a.lnk"));
    CHECK(!PatchExtractAllowed("a.com"));
    CHECK(!PatchExtractAllowed("a.exe."));    // Windows strips trailing dots / spaces
    CHECK(!PatchExtractAllowed("a.dll "));
    CHECK(!PatchExtractAllowed("..\\a.mpq"));
}
