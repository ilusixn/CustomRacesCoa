#include "CoA/Source/Modules/JoinedNames.h"
#include "CoA/Tests/TestMain.h"

COA_TEST(JoinedNamesPutTheMeleeHeadFirst) {
    CHECK(CoA::JoinedNames::Joined("Grogh", "Rogh") == "Grogh'Rogh");
    CHECK(CoA::JoinedNames::Joined("Grogh", "") == "Grogh");
    CHECK(CoA::JoinedNames::Joined("", "Rogh").empty());
}

COA_TEST(JoinedNamesRebuildOnlyWhenTheRiderChanges) {
    CoA::JoinedNames::Reset();
    CHECK(!CoA::JoinedNames::RiderChanged(0x10, 0));
    CHECK(CoA::JoinedNames::RiderChanged(0x10, 0x20));
    CHECK(!CoA::JoinedNames::RiderChanged(0x10, 0x20));
    CHECK(CoA::JoinedNames::RiderChanged(0x10, 0x30));
    CHECK(CoA::JoinedNames::RiderChanged(0x10, 0));
    CHECK(!CoA::JoinedNames::RiderChanged(0x10, 0));
    CoA::JoinedNames::Reset();
}
