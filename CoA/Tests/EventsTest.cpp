#include "CoA/Source/Runtime/Events.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <vector>

namespace {
    std::vector<const char*> StockList(uint32_t count) {
        static const char* stock = "STOCK_EVENT";
        return std::vector<const char*>(count, stock);
    }
}

COA_TEST(Events_GlueListGetsTheFiveGlueCustoms) {
    CoA::Events::ResetQueue();
    std::vector<const char*> stock = StockList(0x29);
    uint32_t count = 0x29;
    const char** list = CoA::Events::ExtendList(stock.data(), count);
    CHECK(count == 0x29 + 5);

    if (count == 0x29 + 5) {
        CHECK(list[0x28] == stock[0x28]);
        CHECK(std::strcmp(list[0x29], "CHARACTER_ACTIVATE_RESULT") == 0);
        CHECK(std::strcmp(list[0x2D], "CHARACTER_CREATE_PREVIEW_CHANGED") == 0);
    }
}

COA_TEST(Events_WorldListGetsTheQueuedCustoms) {
    CoA::Events::ResetQueue();
    CoA::Events::RegisterNames();
    std::vector<const char*> stock = StockList(0x2D2);
    uint32_t count = 0x2D2;
    const char** list = CoA::Events::ExtendList(stock.data(), count);
    CHECK(count == 0x2D2 + 303);

    if (count == 0x2D2 + 303) {
        CHECK(std::strcmp(list[0x2D2], "APPLY_PENDING_APPEARANCE_RESULT") == 0);
    }
}

COA_TEST(Events_ModuleQueuedNameFollowsTheTable) {
    CoA::Events::ResetQueue();
    CoA::Events::RegisterNames();
    CoA::Events::Queue("COA_TEST_EVENT");
    std::vector<const char*> stock = StockList(0x2D2);
    uint32_t count = 0x2D2;
    const char** list = CoA::Events::ExtendList(stock.data(), count);
    CHECK(count == 0x2D2 + 304);

    if (count == 0x2D2 + 304) {
        CHECK(std::strcmp(list[count - 1], "COA_TEST_EVENT") == 0);
    }
}

COA_TEST(Events_OtherCountsPassThrough) {
    CoA::Events::ResetQueue();
    CoA::Events::RegisterNames();
    std::vector<const char*> stock = StockList(12);
    uint32_t count = 12;
    const char** list = CoA::Events::ExtendList(stock.data(), count);
    CHECK(count == 12);
    CHECK(list == stock.data());
}
