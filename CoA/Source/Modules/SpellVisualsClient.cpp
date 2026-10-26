// AscSpellVisuals.cpp's client half: objects and their descriptors, SpellVisual.dbc rows (0xAD4AA8), spell records and
// the visual records of the four detoured client functions.
#include "CoA/Source/Modules/SpellVisuals.h"
#include "CoA/Source/Modules/SpellVisualsClient.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SpellApiClient.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/DB/Rec/SpellVisualRec.h"
#include "WoW/Source/Object/DynamicObject.h"
#include "WoW/Source/Object/ObjectClient/DynamicObject_C.h"
#include "WoW/Source/Object/ObjectClient/Effect_C.h"
#include "WoW/Source/Object/ObjectClient/Missile_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include <cstring>

namespace {
    static_assert(sizeof(SpellVisualRec) == 0x80, "the DLL copies 0x80 bytes");

    bool s_fromSpellGo = false;

    bool ObjectInfo(void* object, CoA::SpellVisuals::ObjectInfo& out) {
        const CGObject_C* o = static_cast<const CGObject_C*>(object);
        out = CoA::SpellVisuals::ObjectInfo();
        out.type = static_cast<uint32_t>(o->m_typeId);
        out.guid = o->m_obj->m_guid.m_guid;

        if (o->m_obj->m_type & TYPE_UNIT) {   // descriptor +0x30 / +0x38 / +0x40 / +0x60
            const CGUnit_C* unit = static_cast<const CGUnit_C*>(o);
            out.charmedBy = unit->m_unit->m_unitFieldCharmedby.m_guid;
            out.summonedBy = unit->m_unit->m_unitFieldSummonedby.m_guid;
            out.createdBy = unit->m_unit->m_unitFieldCreatedby.m_guid;
            out.health = static_cast<uint32_t>(unit->m_unit->m_unitFieldHealth);
        }

        return true;
    }

    void* ObjectByGuid(uint64_t guid, uint32_t mask) {   // 0x4D4DB0
        WOWGUID g;
        g.m_guid = guid;
        return ClntObjMgrObjectPtr(g, static_cast<OBJECT_TYPE>(mask), __FILE__, __LINE__);
    }

    void* ActivePlayer() {   // 0x4038F0
        return ClntObjMgrGetActivePlayerObj();
    }

    uint64_t ActiveGuid() {
        return ClntObjMgrGetActivePlayer().m_guid;
    }

    const SpellVisualRec* VisualRow(uint32_t id) {   // FUN_100b2620 over 0xAD4AA8
        return g_spellVisualDB.GetRecord(static_cast<int32_t>(id));
    }

    bool VisualRowExists(uint32_t id) {
        return VisualRow(id) != nullptr;
    }

    uint32_t EffectSpell(void* effect) {   // +0x18
        return static_cast<CEffect*>(effect)->m_spellId;
    }

    void SetRecordVisual(const void* rec, uint32_t visual) {   // +0x20C (the aura path's local record copy)
        const_cast<SpellRec*>(static_cast<const SpellRec*>(rec))->m_spellVisualID[0] = static_cast<int32_t>(visual);
    }

    bool Spell(uint32_t id, CoA::SpellApi::SpellFields& out) {   // FetchSpell (0x4CFD20 over 0xAD49D0)
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(id), &rec)) {
            return false;
        }

        CoA::SpellApiClient::Fields(rec, out);
        return true;
    }
}

CoA::SpellVisualsClient::SpellGoScope::SpellGoScope() {
    s_fromSpellGo = true;
}

CoA::SpellVisualsClient::SpellGoScope::~SpellGoScope() {
    s_fromSpellGo = false;
}

bool CoA::SpellVisualsClient::SkipAppropriateVisual(const CGUnit_C* unit, const SpellRec* spell) {
    const bool fromSpellGo = s_fromSpellGo;
    s_fromSpellGo = false;

    if (!fromSpellGo) {
        return false;
    }

    CoA::SpellApi::SpellFields fields;
    CoA::SpellApiClient::Fields(*spell, fields);
    return CoA::SpellVisuals::SkipAppropriateVisual(const_cast<CGUnit_C*>(unit), true, fields);
}

void CoA::SpellVisualsClient::AfterAppropriateVisual(const CGUnit_C* unit, const SpellRec* spell, SpellVisualRec& visual) {
    const uint32_t id = CoA::SpellVisuals::AppropriateVisual(const_cast<CGUnit_C*>(unit), static_cast<uint32_t>(spell->m_id));

    if (id == 0) {
        return;
    }

    if (id == 0xFFFFFFFF) {
        memset(&visual, 0, sizeof(visual));
        return;
    }

    if (const SpellVisualRec* row = VisualRow(id)) {
        memcpy(&visual, row, sizeof(visual));
    }
}

const SpellVisualRec* CoA::SpellVisualsClient::MissileHitVisual(const CMissile* missile, const SpellRec* spell, const SpellVisualRec* visual) {
    // The original reads the spell id through the argument unchecked; HitTarget(target, nullptr, nullptr) (the missile's
    // no-spell path) would dereference nullptr (ledgered ruling: no spell keeps the argument).
    if (!spell) {
        return visual;
    }

    const uint32_t id = CoA::SpellVisuals::MissileHitVisual(missile->m_casterGuid.m_guid, static_cast<uint32_t>(spell->m_id));

    if (id == 0xFFFFFFFF) {
        return nullptr;
    }

    if (id) {
        if (const SpellVisualRec* row = VisualRow(id)) {
            return row;
        }
    }

    return visual;
}

bool CoA::SpellVisualsClient::SkipDynamicObject(const CGDynamicObject_C* object) {
    return CoA::SpellVisuals::SkipDynamicObject(object->m_dynamicObject->m_caster.m_guid, object->m_dynamicObject->m_spellID);
}

void CoA::SpellVisualsClient::AfterFireMissiles(const CMissile* missile) {
    CoA::SpellVisuals::Missile m;
    m.caster = missile->m_casterGuid.m_guid;
    m.target = missile->m_targetGuid.m_guid;
    m.spell = missile->m_spellID;
    m.castId = missile->m_castID;
    m.launch[0] = missile->m_launchPos.x;
    m.launch[1] = missile->m_launchPos.y;
    m.launch[2] = missile->m_launchPos.z;
    m.impact[0] = missile->m_impactPos.x;
    m.impact[1] = missile->m_impactPos.y;
    m.impact[2] = missile->m_impactPos.z;
    CoA::SpellVisuals::AfterFireMissiles(m);
}

void CoA::SpellVisualsClient::Init() {
    CoA::SpellVisuals::Deps& d = CoA::SpellVisuals::Dependencies();
    d.objectInfo = ObjectInfo;
    d.objectByGuid = ObjectByGuid;
    d.activePlayer = ActivePlayer;
    d.activeGuid = ActiveGuid;
    d.visualRowExists = VisualRowExists;
    d.effectSpell = EffectSpell;
    d.setRecordVisual = SetRecordVisual;
    d.spell = Spell;
}
