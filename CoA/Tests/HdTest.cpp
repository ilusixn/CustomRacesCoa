#include "CoA/Source/Runtime/Hd.h"
#include "CoA/Tests/TestMain.h"

COA_TEST(Hd_OnWhateverTheRenderer) {
    CoA::Hd::SetVulkanRenderer(false);
    CHECK(CoA::Hd::Available());
    CoA::Hd::SetVulkanRenderer(true);
    CHECK(CoA::Hd::Available());
    CoA::Hd::SetVulkanRenderer(false);
}
