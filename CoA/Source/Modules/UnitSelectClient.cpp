// AscUnitSelect.cpp's client half: the Deps, the drawers (FUN_102db9c0 / FUN_102db140 / FUN_102dbfe0) and what the
// decomp sites ask.
#include "CoA/Source/Modules/UnitSelect.h"
#include "CoA/Source/Modules/UnitSelectClient.h"
#include "CoA/Source/Modules/AttachFixesClient.h"
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/ClientOptions.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Gx/CGxTexFlags.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Tempest/CAaBox.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/MapRec.h"
#include "WoW/Source/DB/Rec/SpellRadiusRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Game/GameClient/ProjectTex.h"
#include "WoW/Source/Object/ObjectClient/DynamicObject_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"
#include <cstring>
#include <windows.h>

// Engine/Source/Model2/CM2SceneRender.cpp: the z pad of a projected batch (the float operand at 0x82080B).
extern float g_m2ProjectedBatchZPad;

namespace {
    void* LoadTexture(const char* path) {   // 0x4B9760(path, 0x201, status, 1)
        CStatus status;
        return TextureCreate(path, CGxTexFlags(), &status, 1);
    }

    void ReleaseTexture(void* texture) {   // 0x47BF30
        HandleClose(reinterpret_cast<HOBJECT>(texture));
    }

    uint32_t Now() {   // 0x86AE20
        return OsGetAsyncTimeMs();
    }

    void SetTrackingTurnRate(float rate) {   // 0xCA11DC
        UnitSetTrackingTurnRate(rate);
    }

    bool MapInstanceType(uint32_t& type) {   // Map.dbc (0xAD4160) row of *0xADFBC4, +8
        const MapRec* map = g_mapDB.GetRecord(CGGameUI::m_iCurrentMapID);

        if (!map) {
            return false;
        }

        type = static_cast<uint32_t>(map->m_instanceType);
        return true;
    }

    void SetProjectionZPad(float pad) {
        g_m2ProjectedBatchZPad = pad;
    }

    // 0xAD2D3C / 0xAD2D48 / 0xAD2D40: Tooltip.cpp's s_tooltipDisenchantColor / s_tooltipBrightRedColor /
    // s_tooltipGreenColor (constants nothing writes).
    CoA::UnitSelect::Colours Colours() {
        CoA::UnitSelect::Colours c;
        c.self = 0xFF88AAFF;
        c.enemy = 0xFFFF0000;
        c.friendly = 0xFF00FF00;
        return c;
    }

    bool Colour(CGPlayer_C* player, CGUnit_C* unit, uint32_t& colour) {
        const bool self = player->m_obj->m_guid == unit->m_obj->m_guid;
        // 0x7251C0 is asked once for the enemy test and once more for the friendly one; it has no side effects.
        const int32_t reaction = self ? 0 : static_cast<int32_t>(player->UnitReaction(unit));
        return CoA::UnitSelect::IndicatorColour(self, reaction, colour);
    }

    CAaBox ToBox(const float b[6]) {
        return CAaBox(C3Vector(b[0], b[1], b[2]), C3Vector(b[3], b[4], b[5]));
    }

    CImVector ToColour(uint32_t argb) {
        CImVector c;
        memcpy(&c, &argb, 4);
        return c;
    }

    // Each drawer points the 0x744EB1 operand at its texture slot, draws through 0x7E4370 between 0x409670 / 0x685FB0
    // on the GX device, then puts the stock operand back.
    void Draw(void* const* texture, const float box[6], uint32_t colour, const C44Matrix& matrix) {
        CoA::ClientHooks::options.circleTexture = texture;
        g_theGxDevicePtr->RsPush();

        if (CGObject_C::SetCircleRenderStates()) {
            ProjectTex::Render2d(ToBox(box), ToColour(colour), &matrix, 0.5f, 0x200122, 1, 0.4f);
        }

        g_theGxDevicePtr->RsPop();
        CoA::ClientHooks::options.circleTexture = nullptr;
    }

    bool CustomAttr14(uint32_t spell, uint32_t mask) {   // FUN_10324a10
        const uint8_t* row = CoA::CA::SpellCustomAttrRow(spell);
        uint32_t v = 0;

        if (row) {
            memcpy(&v, row + 0x14, 4);
        }

        return row && (v & mask) != 0;
    }

    void Position(CGObject_C* object, float p[3]) {   // vtable +0x2C
        const C3Vector v = object->GetPosition();
        p[0] = v.x;
        p[1] = v.y;
        p[2] = v.z;
    }

    // FUN_102db9c0: a dynamic object's AoE -- its radius + 0.5 around it, the unit-select texture.
    void DrawDynamicObject(CGDynamicObject_C* object) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return;
        }

        CGUnit_C* caster = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(object->m_dynamicObject->m_caster, TYPE_UNIT, __FILE__, __LINE__));

        if (!caster) {
            return;
        }

        const uint32_t spell = object->m_dynamicObject->m_spellID;

        if (!spell || CustomAttr14(spell, 0x800)) {
            return;
        }

        const float r = object->m_dynamicObject->m_radius + 0.5f;
        uint32_t colour;

        if (!Colour(player, caster, colour) || !CGGameUI::m_visible) {
            return;
        }

        C44Matrix matrix;
        float p[3];
        float box[6];
        Position(object, p);
        CoA::UnitSelect::Box(p, r, box);
        Draw(CoA::UnitSelect::UnitSelectTexture(), box, colour, matrix);
    }

    // FUN_102db140: a unit casting a cone -- the 90-degree cone texture, 0.725 x the radius, turned to the unit's facing
    // and pushed 0.725 x that ahead.
    void DrawUnit(CGUnit_C* unit) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return;
        }

        const uint32_t spell = unit->m_castingSpellId;
        SpellRec rec;

        if (!spell || !g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec) || CustomAttr14(spell, 0x800)) {
            return;
        }

        const uint32_t targets[3] = { static_cast<uint32_t>(rec.m_implicitTargetA[0]), static_cast<uint32_t>(rec.m_implicitTargetA[1]),
            static_cast<uint32_t>(rec.m_implicitTargetA[2]) };

        if (!CoA::UnitSelect::IsCone(targets)) {
            return;
        }

        float radius[2] = { 0.0f, 0.0f };

        for (uint32_t k = 0; k < 2; ++k) {
            // The original reads EffectRadiusIndex[0] both times.
            const int32_t index = rec.m_effectRadiusIndex[0];
            const SpellRadiusRec* row = index ? g_spellRadiusDB.GetRecord(index) : nullptr;

            if (row) {
                radius[k] = CoA::UnitSelect::Radius(row->m_radius, row->m_radiusPerLevel, row->m_radiusMax, unit->m_unit->m_unitFieldLevel);
            }
        }

        const float r = radius[0] > radius[1] ? radius[0] : radius[1];
        uint32_t colour;

        // The original tests the pointer 0x10BCBF6C itself here (always set), not *0xAC80A8.
        if (!Colour(player, unit, colour)) {
            return;
        }

        C44Matrix matrix;
        float p[3];
        float box[6];
        Position(unit, p);
        const float facing = unit->GetFacing();
        CoA::ClientOptions::RotateZ(&matrix.a0, -facing);   // FUN_102bd3f0
        CoA::UnitSelect::ConeBox(p, r, facing, box);
        void* texture = CoA::UnitSelect::Cone90();
        Draw(&texture, box, colour, matrix);
    }

    bool Relevant(uint32_t entry, void* player) {   // FUN_1008de00
        return CoA::AttachFixesClient::QuestRelevant(static_cast<CGPlayer_C*>(player), entry, false);
    }

    // FUN_102dbfe0: a living, non-pet unit whose entry an unfinished quest still needs -- a circle of its selection
    // radius (+0xB0C) in 0x32FFD200 with the quest texture.
    void DrawQuestUnit(CGUnit_C* unit) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player || unit->m_unit->m_unitFieldHealth == 0 || unit->m_unit->m_unitFieldPetnumber != 0) {
            return;
        }

        if (!CoA::UnitSelect::QuestCircle(unit->m_obj->m_entryID, Relevant, player) || !CGGameUI::m_visible) {
            return;
        }

        C44Matrix matrix;
        float p[3];
        float box[6];
        Position(unit, p);
        CoA::UnitSelect::Box(p, unit->m_selectionRadius, box);
        Draw(CoA::UnitSelect::QuestTexture(), box, 0x32FFD200, matrix);
    }

    int32_t EnumDynamic(WOWGUID guid, void*) {   // FUN_102da220
        if (CGObject_C* object = ClntObjMgrObjectPtr(guid, TYPE_DYNAMICOBJECT, __FILE__, __LINE__)) {
            DrawDynamicObject(static_cast<CGDynamicObject_C*>(object));
        }

        return 1;
    }

    int32_t EnumUnit(WOWGUID guid, void*) {   // FUN_102da250
        if (CGObject_C* unit = ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__)) {
            DrawUnit(static_cast<CGUnit_C*>(unit));
        }

        return 1;
    }

    int32_t EnumQuest(WOWGUID guid, void*) {   // FUN_102da280
        if (CGObject_C* unit = ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__)) {
            DrawQuestUnit(static_cast<CGUnit_C*>(unit));
        }

        return 1;
    }

    void DrawIndicators() {
        ClntObjMgrEnumVisibleObjects(EnumDynamic, nullptr);
        ClntObjMgrEnumVisibleObjects(EnumUnit, nullptr);
    }

    void DrawQuestCircles() {
        ClntObjMgrEnumVisibleObjects(EnumQuest, nullptr);
    }

    // The active player's vtable +0x6C (its own selection circle), only while it is its own locked target.
    void DrawOwnCircle() {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (player && player->m_obj->m_guid == CGGameUI::m_lockedTarget) {
            player->RenderTargetSelection();
        }
    }

    // A return address past the running exe's image (the original: > 0xDFCBFF, the end of Ascension.exe's).
    uintptr_t ImageEnd() {
        static uintptr_t s_end = 0;

        if (!s_end) {
            const uint8_t* base = reinterpret_cast<const uint8_t*>(GetModuleHandleA(nullptr));
            const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
            s_end = reinterpret_cast<uintptr_t>(base) + nt->OptionalHeader.SizeOfImage - 1;
        }

        return s_end;
    }
}

bool CoA::UnitSelectClient::RenderUnit(CGUnit_C* unit) {
    if (CoA::UnitSelect::SkipAutoTrackCursor()) {
        return true;
    }

    CoA::ClientHooks::options.circleTexture = CoA::UnitSelect::TrackingTexture();
    unit->RenderAutoTrackCursor();
    CoA::ClientHooks::options.circleTexture = nullptr;
    return true;
}

void CoA::UnitSelectClient::BeforeClickToMove(const void* returnAddress) {
    CoA::UnitSelect::BeforeClickToMove(reinterpret_cast<uintptr_t>(returnAddress) > ImageEnd());
}

void CoA::UnitSelectClient::Init() {
    CoA::UnitSelect::Deps& d = CoA::UnitSelect::Dependencies();
    d.loadTexture = LoadTexture;
    d.releaseTexture = ReleaseTexture;
    d.now = Now;
    d.setTrackingTurnRate = SetTrackingTurnRate;
    d.mapInstanceType = MapInstanceType;
    d.setProjectionZPad = SetProjectionZPad;
    d.colours = Colours;
    d.drawIndicators = DrawIndicators;
    d.drawQuestCircles = DrawQuestCircles;
    d.drawOwnCircle = DrawOwnCircle;
    // The installer's 10.0 was written before the Deps existed.
    g_m2ProjectedBatchZPad = CoA::UnitSelect::ProjectionZPad();
}
