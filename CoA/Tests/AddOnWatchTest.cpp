#include "CoA/Source/Modules/AddOnWatch.h"
#include "CoA/Tests/TestMain.h"

// The folder itself and its .toc count; deeper files, other files and dot folders do not.
COA_TEST(AddOnWatch_RelevantChanges) {
    CHECK(CoA::AddOnWatch::AddOnForChange("MyAddon") == "MyAddon");
    CHECK(CoA::AddOnWatch::AddOnForChange("MyAddon\\MyAddon.toc") == "MyAddon");
    CHECK(CoA::AddOnWatch::AddOnForChange("MyAddon/MyAddon.TOC") == "MyAddon");
    CHECK(CoA::AddOnWatch::AddOnForChange("MyAddon\\Core.lua").empty());
    CHECK(CoA::AddOnWatch::AddOnForChange("MyAddon\\MyAddon.url").empty());
    CHECK(CoA::AddOnWatch::AddOnForChange("MyAddon\\Libs\\Lib.toc").empty());
    CHECK(CoA::AddOnWatch::AddOnForChange(".git").empty());
    CHECK(CoA::AddOnWatch::AddOnForChange("").empty());
    CHECK(CoA::AddOnWatch::AddOnForChange(nullptr).empty());
}

// Nothing is due until the burst has been quiet for kSettleMs; names are deduplicated case-insensitively.
COA_TEST(AddOnWatch_SettlesBursts) {
    CoA::AddOnWatch::Reset();
    std::vector<std::string> names;
    bool all = true;

    CHECK(!CoA::AddOnWatch::Take(1000, names, all));

    CoA::AddOnWatch::Note("MyAddon", 1000);
    CoA::AddOnWatch::Note("myaddon", 1200);
    CoA::AddOnWatch::Note("Other", 1300);
    CHECK(!CoA::AddOnWatch::Take(1300 + CoA::AddOnWatch::kSettleMs - 1, names, all));
    CHECK(CoA::AddOnWatch::Take(1300 + CoA::AddOnWatch::kSettleMs, names, all));
    CHECK(names.size() == 2);
    CHECK(!all);

    // Taken: nothing more until the next change.
    CHECK(!CoA::AddOnWatch::Take(5000, names, all));
}

// An overflow asks for every addon.
COA_TEST(AddOnWatch_OverflowRescansAll) {
    CoA::AddOnWatch::Reset();
    std::vector<std::string> names;
    bool all = false;

    CoA::AddOnWatch::NoteAll(100);
    CHECK(CoA::AddOnWatch::Take(100 + CoA::AddOnWatch::kSettleMs, names, all));
    CHECK(all);
    CoA::AddOnWatch::Reset();
}
