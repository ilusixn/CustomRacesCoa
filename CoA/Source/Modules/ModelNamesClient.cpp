// AscModelNames.cpp's client half: the three client tables PopulateModelNameCache reads.
#include "CoA/Source/Modules/ModelNames.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/DB/DBClient/DBClient.h"

namespace {
    // CreatureModelData +0x08, GameObjectDisplayInfo +0x04, SpellVisualEffectName +0x08: ids 0 .. max - 1.
    void ClientPaths(void (*add)(const char* path, void* ctx), void* ctx) {
        for (int32_t id = 0; id < g_creatureModelDataDB.m_maxID; ++id) {
            if (const CreatureModelDataRec* row = g_creatureModelDataDB.GetRecord(id)) {
                add(row->m_modelName, ctx);
            }
        }

        for (int32_t id = 0; id < g_gameObjectDisplayInfoDB.m_maxID; ++id) {
            if (const GameObjectDisplayInfoRec* row = g_gameObjectDisplayInfoDB.GetRecord(id)) {
                add(row->m_modelName, ctx);
            }
        }

        for (int32_t id = 0; id < g_spellVisualEffectNameDB.m_maxID; ++id) {
            if (const SpellVisualEffectNameRec* row = g_spellVisualEffectNameDB.GetRecord(id)) {
                add(row->m_fileName, ctx);
            }
        }
    }
}

void CoA::ModelNamesClient::Init() {
    CoA::ModelNames::Dependencies().clientPaths = ClientPaths;
}
