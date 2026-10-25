#include "CoA/Source/Runtime/Lifecycle.h"
#include <vector>

namespace {
    std::vector<CoA::Lifecycle::Callback>& GlueScreenList() {
        static std::vector<CoA::Lifecycle::Callback> list;
        return list;
    }

    std::vector<CoA::Lifecycle::Callback>& AfterPaintList() {
        static std::vector<CoA::Lifecycle::Callback> list;
        return list;
    }

    std::vector<CoA::Lifecycle::Callback>& AfterIdleList() {
        static std::vector<CoA::Lifecycle::Callback> list;
        return list;
    }
}

void CoA::Lifecycle::OnGlueScreen(CoA::Lifecycle::Callback cb) {
    GlueScreenList().push_back(cb);
}

void CoA::Lifecycle::RunGlueScreen() {
    for (CoA::Lifecycle::Callback cb : GlueScreenList()) {
        cb();
    }
}

void CoA::Lifecycle::OnAfterIdle(CoA::Lifecycle::Callback cb) {
    AfterIdleList().push_back(cb);
}

void CoA::Lifecycle::RunAfterIdle() {
    for (CoA::Lifecycle::Callback cb : AfterIdleList()) {
        cb();
    }
}

void CoA::Lifecycle::OnAfterPaint(CoA::Lifecycle::Callback cb) {
    AfterPaintList().push_back(cb);
}

void CoA::Lifecycle::RunAfterPaint() {
    for (CoA::Lifecycle::Callback cb : AfterPaintList()) {
        cb();
    }
}

void CoA::Lifecycle::Reset() {
    GlueScreenList().clear();
    AfterIdleList().clear();
    AfterPaintList().clear();
}
