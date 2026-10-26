// AscAlphaAmbience.cpp's client half: the SoundAmbience.dbc rows (container 0xAD464C, FUN_100b2110).
#include "CoA/Source/Modules/AlphaAmbience.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SoundAmbienceRec.h"

namespace {
    bool SetAmbience(uint32_t id, uint32_t day, uint32_t night) {   // +4 / +8
        SoundAmbienceRec* row = g_soundAmbienceDB.GetRecord(static_cast<int32_t>(id));

        if (!row) {
            return false;
        }

        row->m_ambienceID[0] = static_cast<int32_t>(day);
        row->m_ambienceID[1] = static_cast<int32_t>(night);
        return true;
    }
}

void CoA::AlphaAmbienceClient::Init() {
    CoA::AlphaAmbience::Dependencies().setAmbience = SetAmbience;
}
