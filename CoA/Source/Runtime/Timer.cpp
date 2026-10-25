#include "CoA/Source/Runtime/Timer.h"

namespace {
    uint32_t (*s_schedule)(uint32_t ms, CoA::Timer::Fn fn, void* param) = nullptr;
    void (*s_cancel)(uint32_t id, CoA::Timer::Fn fn, void* param) = nullptr;
}

uint32_t CoA::Timer::Schedule(uint32_t ms, CoA::Timer::Fn fn, void* param) {
    return s_schedule ? s_schedule(ms, fn, param) : 0;
}

void CoA::Timer::Cancel(uint32_t id, CoA::Timer::Fn fn, void* param) {
    if (s_cancel) {
        s_cancel(id, fn, param);
    }
}

void CoA::Timer::SetBackend(uint32_t (*schedule)(uint32_t ms, CoA::Timer::Fn fn, void* param), void (*cancel)(uint32_t id, CoA::Timer::Fn fn, void* param)) {
    s_schedule = schedule;
    s_cancel = cancel;
}
