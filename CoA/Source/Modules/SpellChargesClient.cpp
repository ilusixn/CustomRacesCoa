// AscSpellCharges.cpp's client question: the millisecond clock (0x86AE20).
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SpellCharges.h"
#include "Engine/Source/Os/TimeManager.h"

void CoA::SpellChargesClient::Init() {
    CoA::SpellCharges::Dependencies().now = OsGetAsyncTimeMs;
}
