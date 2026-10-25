// AscPatchData.cpp's game-object textures (installer FUN_1020c390): a game object whose display has a
// GameObjectDisplayInfoAddon.dbc row naming a texture gets it in its model's texture slot 2 -- after the client builds
// the object (the hooks on 0x713F50 ModelLoaded and 0x712400 LoadBaseObject) and, after SMSG 0x6FB, on every visible
// game object (FUN_1020c500).
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/PatchData.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/Gx/CGxTexFlags.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Services/Texture.h"
#include "WoW/Source/Object/GameObjectStats.h"
#include "WoW/Source/Object/ObjectClient/GameObject_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace {
    // FUN_1020c060: the texture is loaded with CGxTexFlags(3, 1, 1, 0, 0, 0, 1, 0, 0, 0) and the FrameXML status, and
    // put in the world model's slot 2. (The original calls the model unchecked; an object without one is skipped.)
    void RefreshGameObjectTexture(CGGameObject_C* object) {
        if (!object->m_stats) {
            return;
        }

        CoA::Dbc::Table& table = CoA::Dbc::Get("DBFilesClient/GameObjectDisplayInfoAddon.dbc");
        const uint8_t* row = table.Row(object->m_stats->m_displayID);
        const char* name = row ? table.Str(row, 4) : nullptr;

        if (!name || !*name) {
            return;
        }

        CGxTexFlags flags(static_cast<EGxTexFilter>(3), 1, 1, 0, 0, 0, 1, 0, 0, 0);
        CTexture* texture = TextureCreate(name, flags, FrameXML_Status(), 0);

        if (texture && object->m_worldModel) {
            object->m_worldModel->ReplaceTexture(2, texture);
        }
    }

    // FUN_1020c100: the original also records the entry and the model's name for its crash reports.
    void AfterModelLoaded(CGGameObject_C* object, CM2Model*) {
        RefreshGameObjectTexture(object);
    }

    // LAB_1020c030
    int32_t RefreshEach(WOWGUID guid, void*) {
        if (CGGameObject_C* object = static_cast<CGGameObject_C*>(ClntObjMgrObjectPtr(guid, TYPE_GAMEOBJECT, __FILE__, __LINE__))) {
            RefreshGameObjectTexture(object);
        }

        return 1;
    }

    // FUN_1020c500: in world, every visible game object.
    void RefreshGameObjectTextures() {
        if (ClntObjMgrGetActivePlayerObj()) {
            ClntObjMgrEnumVisibleObjects(RefreshEach, nullptr);
        }
    }
}

void CoA::PatchDataClient::Init() {
    g_gameObjectModelLoadedHook = AfterModelLoaded;
    g_gameObjectBaseLoadedHook = RefreshGameObjectTexture;
    CoA::PatchData::Dependencies().gameObjectTextures = RefreshGameObjectTextures;
}
