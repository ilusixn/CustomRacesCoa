#include "CoA/Source/Modules/AnimationCount.h"

namespace {
    uint32_t s_rows = 506;   // the immediate 0x1FA at each site
}

uint32_t CoA::AnimationCount::Rows() {
    return s_rows;
}

void CoA::AnimationCount::Set(uint32_t rows) {
    s_rows = rows;
}

void CoA::AnimationCount::Reset() {
    s_rows = 506;
}
