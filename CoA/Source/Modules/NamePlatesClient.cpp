// AscNamePlates.cpp's client half: the Deps and what the decomp sites ask with client objects.
#include "CoA/Source/Modules/NamePlates.h"
#include "CoA/Source/Modules/NamePlatesClient.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/NamePlateFrame.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/WorldClient/World.h"

namespace {
    void Fill(CGUnit_C* unit, CoA::NamePlates::Body& out) {
        const C3Vector p = unit->GetPosition();
        out.guid = unit->m_obj->m_guid.m_guid;
        out.pos[0] = p.x;
        out.pos[1] = p.y;
        out.pos[2] = p.z;
        out.height = unit->GetModelScale(unit->m_unit->m_unitFieldDisplayid);   // 0x722AE0(unit, descriptor +0x10C)
        out.facing = unit->GetFacing();
    }

    bool Player(CoA::NamePlates::Body& out) {   // 0x4038F0
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return false;
        }

        Fill(player, out);
        out.guid = ClntObjMgrGetActivePlayer().m_guid;   // 0x4D3790
        return true;
    }

    uint64_t LockedTarget() {   // 0xBD07B0
        return CGGameUI::m_lockedTarget.m_guid;
    }

    bool CameraXY(float& x, float& y) {   // 0x4F5960 +8 / +0xC
        CGCamera* camera = CGWorldFrame::GetActiveCamera();

        if (!camera) {
            return false;
        }

        x = camera->m_position.x;
        y = camera->m_position.y;
        return true;
    }

    bool UnitBody(uint64_t guid, uint32_t mask, CoA::NamePlates::Body& out) {   // 0x4D4DB0
        WOWGUID g;
        g.m_guid = guid;
        CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(g, static_cast<OBJECT_TYPE>(mask), __FILE__, __LINE__));

        if (!unit) {
            return false;
        }

        Fill(unit, out);
        return true;
    }

    bool LineBlocked(const float from[3], const float to[3]) {   // 0x77F310(from, to, hit, &fraction, 0x100070, 0)
        C3Vector start(from[0], from[1], from[2]);
        C3Vector end(to[0], to[1], to[2]);
        C3Vector hit(0.0f, 0.0f, 0.0f);
        float fraction = 1.0f;
        return World::Intersect(start, end, &hit, &fraction, 0x100070, nullptr) != 0;
    }

    void SetAlpha(void* frame, uint32_t alpha) {   // 0x48EA10
        static_cast<CGNamePlateFrame*>(frame)->SetFrameAlpha(static_cast<uint8_t>(alpha));
    }

    void ShowPlate(void* frame, bool shown) {   // 0x48F660 / 0x48F620
        if (shown) {
            static_cast<CGNamePlateFrame*>(frame)->Show();
        } else {
            static_cast<CGNamePlateFrame*>(frame)->Hide();
        }
    }

    float Bearing(const float from[3], const float to[3]) {   // 0x4F5130
        C3Vector a(from[0], from[1], from[2]);
        C3Vector b(to[0], to[1], to[2]);
        return CalculateFacingTo(&a, &b);
    }

    void AddPlate(CGNamePlateFrame* frame, void* param) {
        CoA::NamePlates::Plate p;
        p.frame = frame;
        p.guid = frame->m_guid.m_guid;
        static_cast<std::vector<CoA::NamePlates::Plate>*>(param)->push_back(p);
    }

    std::vector<CoA::NamePlates::Plate> Plates() {   // the list at 0xADAA88
        std::vector<CoA::NamePlates::Plate> out;
        UnitForEachNamePlate(AddPlate, &out);
        return out;
    }

    int32_t CollectUnit(WOWGUID guid, void* param) {   // FUN_102c0a70 / sub_102bd5d0: mask 0x18
        CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__));

        if (unit) {
            CoA::NamePlates::Unit u;
            u.unit = unit;
            u.guid = unit->m_obj->m_guid.m_guid;
            u.plate = unit->m_namePlateFrame;
            const C3Vector p = unit->GetPosition();
            u.pos[0] = p.x;
            u.pos[1] = p.y;
            u.pos[2] = p.z;
            static_cast<std::vector<CoA::NamePlates::Unit>*>(param)->push_back(u);
        }

        return 1;
    }

    std::vector<CoA::NamePlates::Unit> Units() {   // 0x4D4B30
        std::vector<CoA::NamePlates::Unit> out;
        ClntObjMgrEnumVisibleObjects(CollectUnit, &out);
        return out;
    }

    void SetFrameLevel(void* frame, int32_t level) {   // 0x4910A0(frame, level, 1)
        static_cast<CGNamePlateFrame*>(frame)->SetFrameLevel(level, 1);
    }

    void RerunPlatePass() {   // 0x725890(*0xB7436C)
        CGUnit_C::UnitNameplatesReposition(CGWorldFrame::s_currentWorldFrame);
    }

    uint64_t KeywordGuid(const char* token) {   // 0x60C1C0
        return GetGuidByKeyword(token).m_guid;
    }

    void PushPlate(lua_State* L, void* frame) {   // 0x819880 when unregistered, then lua_rawgeti(L, -10000, +8)
        CGNamePlateFrame* plate = static_cast<CGNamePlateFrame*>(frame);

        if (!plate->m_luaRegistered) {
            plate->RegisterScriptObject(nullptr);
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, plate->m_luaObjectRef);
    }
}

void CoA::NamePlatesClient::AfterLayerUpdate(CGNamePlateFrame* frame) {
    CoA::NamePlates::AfterPlateUpdate(frame, frame->m_guid.m_guid);
}

bool CoA::NamePlatesClient::BeforeShouldShow(CGUnit_C* unit, CGWorldFrame* worldFrame, C3Vector* position, int32_t& result) {
    CoA::NamePlates::ShowQuery q;
    q.platesShown = CGGameUI::m_visible != 0;   // *0xAC80A8
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
    q.havePlayer = player != nullptr;

    if (q.platesShown && player) {
        q.isActivePlayer = unit == player;
        q.reaction = static_cast<int32_t>(player->UnitReaction(unit));
        q.inCombat = (unit->m_unit->m_unitFieldFlags >> 0x13) & 1;
        q.isLockedTarget = unit->m_obj->m_guid == CGGameUI::m_lockedTarget;
        const C3Vector from = player->GetPosition();
        const C3Vector at = unit->GetPosition();
        q.playerPos[0] = from.x;
        q.playerPos[1] = from.y;
        q.playerPos[2] = from.z;
        q.unitPos[0] = at.x;
        q.unitPos[1] = at.y;
        q.unitPos[2] = at.z;
    }

    switch (CoA::NamePlates::BeforeShow(q)) {
        case CoA::NamePlates::Show::Original:
            return false;
        case CoA::NamePlates::Show::None:
            result = 0;
            return true;
        case CoA::NamePlates::Show::PersonalName: {
            // vtable +0x20, projected through 0x4F6D20(view, out, out, 0); the projection's result is ignored.
            const C3Vector p = player->GetNamePosition();
            worldFrame->GetScreenCoordinates(p, position, nullptr);
            result = 1;
            return true;
        }
        case CoA::NamePlates::Show::PersonalFeet: {
            const C3Vector p = player->GetPosition();
            worldFrame->GetScreenCoordinates(p, position, nullptr);
            position->y = position->y - 0.04f;
            result = 1;
            return true;
        }
    }

    return false;
}

bool CoA::NamePlatesClient::IsOwnFocus() {
    const CGNamePlateFrame* focus = CGUnit_C::s_namePlateFocus;
    // Ruling: FUN_102c2e90 dereferences the focus unchecked; a null focus here is not the player's.
    return focus && CoA::NamePlates::IsOwnPlate(focus->m_guid.m_guid, ClntObjMgrGetActivePlayer().m_guid);
}

bool CoA::NamePlatesClient::TokenGuid(const char* token, WOWGUID& guid) {
    CGUnit_C* unit = static_cast<CGUnit_C*>(CoA::NamePlates::UnitForToken(token));

    if (!unit) {
        return false;
    }

    guid = unit->m_obj->m_guid;
    return true;
}

void CoA::NamePlatesClient::Init() {
    CoA::NamePlates::Deps& d = CoA::NamePlates::Dependencies();
    d.player = Player;
    d.lockedTarget = LockedTarget;
    d.cameraXY = CameraXY;
    d.unitBody = UnitBody;
    d.lineBlocked = LineBlocked;
    d.setAlpha = SetAlpha;
    d.showPlate = ShowPlate;
    d.bearing = Bearing;
    d.plates = Plates;
    d.units = Units;
    d.setFrameLevel = SetFrameLevel;
    d.rerunPlatePass = RerunPlatePass;
    d.tokenGuid = KeywordGuid;
    d.pushPlate = PushPlate;
}
