// AscTrialCreator.cpp's client questions: the SpellIcon.dbc texture paths and the clock.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/TrialCreator.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellIconRec.h"
#include <cstring>
#include <ctime>

namespace {
    // SpellIcon (client container 0xAD488C): ids min (0xAD489C) .. max (0xAD4898), a row's +4 path equal to the name.
    bool IconExists(const char* icon) {
        for (int32_t id = g_spellIconDB.m_minID; id < g_spellIconDB.m_maxID + 1; ++id) {
            if (const SpellIconRec* rec = g_spellIconDB.GetRecord(id)) {
                if (rec->m_textureFilename && strcmp(icon, rec->m_textureFilename) == 0) {
                    return true;
                }
            }
        }

        return false;
    }

    int64_t Now() {
        return static_cast<int64_t>(time(nullptr));
    }
}

void CoA::TrialCreatorClient::Init() {
    CoA::TrialCreator::Deps& deps = CoA::TrialCreator::Dependencies();
    deps.iconExists = IconExists;
    deps.now = Now;
}
