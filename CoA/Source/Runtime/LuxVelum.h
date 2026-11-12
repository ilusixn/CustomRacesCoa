#ifndef COA_SOURCE_RUNTIME_LUXVELUM_H
#define COA_SOURCE_RUNTIME_LUXVELUM_H

// Loads LuxVelum64.dll (fog, light and water add-on) from the exe's folder when present; "-noluxvelum" skips it.
// The engine calls the dll through g_luxVelum (Engine/Source/Gx/LuxVelum.h). Not part of the original (CoA64 addition).
namespace CoA::LuxVelum {
    void Load();
}

#endif
