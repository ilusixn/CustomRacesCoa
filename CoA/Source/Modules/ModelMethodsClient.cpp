// AscModelMethods.cpp: the #Model methods SetSpell / SetSpellVisual / GetSpell / SetUnitGUID / SetSheathe and the DLL's
// own spell-visual kit player behind SetSpell / SetSpellVisual.
//
// Every method but SetUnitGUID / GetSpell acts on the frame's CM2Model (CSimpleModel::m_model, frame +0x2A0) and does
// nothing without one.
//   PlayKit (FUN_10309880): each SpellVisualKitModelAttach row of the kit, then the kit's twelve effect columns at
//     fixed attachment points (AttachEffect, FUN_10309560); then the kit's animation.
//   PlayVisual (FUN_10309450): instant spells play precast + cast + impact and reset at the animation's end
//     (OnCastEnd 0x103080F0); cast-time spells play precast, then cast + impact when it ends (OnPrecastEnd 0x10309D50).
//   PlayMissile (FUN_10309690 / FUN_10112860): the missile state at 0x10BCCD68, launched (FUN_10112660) and moved by a
//     16 ms tick (0x10A76860). The original's tick has no plain caller (the VM-protected region drives it), so a timer
//     runs it here, and launch waits until the model's M2 data is loaded.
// SetSpell flips the two jump bytes 0x59714D / 0x5971C1 (CGCharacterModelBase::RotateModel / UpdateModel) between
// jne (0x75) and jmp (0xEB) as the original does: g_rotateModelJump / g_updateModelJump.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "CoA/Source/Runtime/Timer.h"
#include "Engine/Source/Frame/CSimpleModel.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/SpellCastTimesRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/DB/Rec/SpellVisualEffectNameRec.h"
#include "WoW/Source/DB/Rec/SpellVisualKitModelAttachRec.h"
#include "WoW/Source/DB/Rec/SpellVisualKitRec.h"
#include "WoW/Source/DB/Rec/SpellVisualRec.h"
#include "WoW/Source/Object/ObjectClient/UnitCombatLog_C.h"
#include "WoW/Source/Ui/CharacterModelBase.h"
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

// WoW/Source/Ui/DressUpModelFrame.cpp: the dress-up frame's two hand items (0xC0E4F8, 0xC0E4FC).
extern int32_t s_mainHandItemId;
extern int32_t s_offHandItemId;

namespace {
    using CoA::Script::CheckNumber;
    using CoA::Script::ToInt;

    void* Self(lua_State* L) {   // self[0]
        lua_rawgeti(L, 1, 0);
        void* obj = lua_touserdata(L, -1);
        lua_settop(L, -2);
        return obj;
    }

    CM2Model* ModelOf(lua_State* L) {   // frame +0x2A0
        CSimpleModel* frame = static_cast<CSimpleModel*>(Self(L));
        return frame ? frame->m_model : nullptr;
    }

    bool FetchSpell(uint32_t spell, SpellRec& rec) {
        return g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec) != 0;
    }

    const SpellVisualRec* SpellVisual(int32_t id) { return g_spellVisualDB.GetRecord(id); }
    const SpellVisualKitRec* SpellVisualKit(int32_t id) { return g_spellVisualKitDB.GetRecord(id); }
    const SpellVisualEffectNameRec* SpellVisualEffectName(int32_t id) { return g_spellVisualEffectNameDB.GetRecord(id); }
    const SpellCastTimesRec* SpellCastTimes(int32_t id) { return g_spellCastTimesDB.GetRecord(id); }

    // ---- CM2Model calls -------------------------------------------------------------------------------------
    void SetCallback(CM2Model* m, M2SequenceCallback fn, uint64_t arg) {   // 0x823FE0
        m->SetSequenceCallback(reinterpret_cast<void*>(fn), arg);
    }

    // 0x832AB0 (m, -1, anim, NaN, 0, speed, 1, 1): the original passes a float NaN in the variation slot.
    void PlayAnim(CM2Model* m, uint32_t anim, float speed) {
        m->SetBoneSequence(0xFFFFFFFF, anim, 0x7FC00000, 0, speed, 1, 1);
    }

    // ---- 4x4 row-major matrix (FUN_102bd230 / 310 / 3f0 / 4d0 / 530) ----------------------------------------
    struct Mat {
        float m[16];
    };

    Mat Identity() {
        return Mat { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };
    }

    Mat Mul(const Mat& a, const Mat& b) {   // FUN_102bcea0
        Mat o;

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                o.m[i * 4 + j] = a.m[i * 4] * b.m[j] + a.m[i * 4 + 1] * b.m[4 + j] + a.m[i * 4 + 2] * b.m[8 + j] + a.m[i * 4 + 3] * b.m[12 + j];
            }
        }

        return o;
    }

    void Rotate(Mat& t, int axis, float a) {   // t = R * t
        const float c = cosf(a);
        const float s = sinf(a);
        Mat r = Identity();

        if (axis == 0) {
            r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c;
        } else if (axis == 1) {
            r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c;
        } else {
            r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c;
        }

        t = Mul(r, t);
    }

    void Translate(Mat& t, const float* v) {
        for (int j = 0; j < 3; ++j) {
            t.m[12 + j] += v[0] * t.m[j] + v[1] * t.m[4 + j] + v[2] * t.m[8 + j];
        }
    }

    void Scale(Mat& t, float s) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                t.m[i * 4 + j] *= s;
            }
        }
    }

    // ---- kit player -----------------------------------------------------------------------------------------
    // 0x10309DE0: an attached effect's animation ended -- loop its "stand" (0x9F) at speed 0, or replay.
    void OnEffectAnimEnd(CM2Model* model, uint32_t, uint32_t anim, int32_t, uint32_t, uint64_t) {
        SetCallback(model, nullptr, 0);

        if (model->HasSequence(0x9F)) {
            PlayAnim(model, 0x9F, 0.0f);
        } else {
            PlayAnim(model, anim, 1.0f);
        }
    }

    // FUN_10309560: create the effect model and attach it with offset / rotation / effect scale.
    bool AttachEffect(CM2Model* model, int32_t effectName, uint32_t attach, bool loop, const float* offset, const float* rot) {
        const SpellVisualEffectNameRec* e = SpellVisualEffectName(effectName);

        if (!e) {
            return false;
        }

        CM2Model* fx = model->m_scene->CreateModel(e->m_fileName, 0);

        if (!fx) {
            return false;
        }

        if (loop) {
            SetCallback(fx, OnEffectAnimEnd, 0);
        }

        fx->AttachToParent(model, attach, nullptr, 0);
        Mat t = Identity();

        if (offset) {
            Translate(t, offset);
        }

        if (rot) {
            Rotate(t, 0, rot[0]);
            Rotate(t, 1, rot[1]);
            Rotate(t, 2, rot[2]);
        }

        Scale(t, e->m_scale);
        C44Matrix matrix;
        static_assert(sizeof(C44Matrix) == sizeof(t.m), "C44Matrix is 16 floats");
        memcpy(&matrix, t.m, sizeof(t.m));
        fx->SetWorldTransform(matrix);
        return true;
    }

    // FUN_10309880
    bool PlayKit(CM2Model* model, int32_t kitId, bool noAnim, bool loop) {
        const SpellVisualKitRec* kit = SpellVisualKit(kitId);

        if (!kit) {
            return false;
        }

        std::vector<uint32_t> attached;

        for (int32_t id = g_spellVisualKitModelAttachDB.m_minID; id <= g_spellVisualKitModelAttachDB.m_maxID; ++id) {
            const SpellVisualKitModelAttachRec* a = g_spellVisualKitModelAttachDB.GetRecord(id);

            if (!a || a->m_parentSpellVisualKitID != kit->m_id) {
                continue;
            }

            const float rot[3] = { a->m_yaw, a->m_pitch, a->m_roll };

            if (AttachEffect(model, a->m_spellVisualEffectNameID, static_cast<uint32_t>(a->m_attachmentID), loop, a->m_offset, rot)) {
                attached.push_back(static_cast<uint32_t>(a->m_attachmentID));
            }
        }

        // The kit's effect columns and the attachment point each one goes to.
        const struct {
            int32_t effect;
            uint32_t attach;
        } columns[] = {
            { kit->m_headEffect, 0x14 }, { kit->m_chestEffect, 0x22 }, { kit->m_baseEffect, 0x13 },
            { kit->m_leftHandEffect, 0x15 }, { kit->m_rightHandEffect, 0x16 }, { kit->m_breathEffect, 0x11 },
            { kit->m_leftWeaponEffect, 0x20 }, { kit->m_rightWeaponEffect, 0x21 }, { kit->m_specialEffect[0], 0x17 },
            { kit->m_specialEffect[1], 0x18 }, { kit->m_specialEffect[2], 0x19 }, { kit->m_worldEffect, 0x00 },
        };

        for (const auto& c : columns) {
            if (AttachEffect(model, c.effect, c.attach, loop, nullptr, nullptr)) {
                attached.push_back(c.attach);
            }
        }

        const uint32_t anim = static_cast<uint32_t>(kit->m_animID);

        if (!noAnim && anim != 0xFFFFFFFF) {
            PlayAnim(model, anim, 1.0f);
            noAnim = false;
        }

        return !(attached.empty() && (noAnim || anim == 0xFFFFFFFF));
    }

    // 0x103080F0: the cast finished -- back to stand, every effect point cleared.
    void OnCastEnd(CM2Model* model, uint32_t, uint32_t, int32_t, uint32_t, uint64_t visual) {
        if (visual == 0 || !SpellVisual(static_cast<int32_t>(visual))) {
            return;
        }

        SetCallback(model, nullptr, 0);
        PlayAnim(model, 0, 1.0f);

        for (uint32_t a : { 0x14u, 0x22u, 0x13u, 0x15u, 0x16u, 0x11u, 0x20u, 0x21u, 0x17u, 0x18u, 0x19u }) {
            model->DetachAllChildrenById(a);
        }
    }

    // 0x10309D50: the precast animation ended -- play cast + impact.
    void OnPrecastEnd(CM2Model* model, uint32_t, uint32_t anim, int32_t, uint32_t, uint64_t visual) {
        if (visual == 0) {
            return;
        }

        const SpellVisualRec* v = SpellVisual(static_cast<int32_t>(visual));
        const SpellVisualKitRec* precast = v ? SpellVisualKit(v->m_precastKit) : nullptr;

        if (!precast || anim != static_cast<uint32_t>(precast->m_animID)) {
            return;
        }

        SetCallback(model, nullptr, 0);

        if (!PlayKit(model, v->m_castKit, false, false)) {
            return;
        }

        PlayKit(model, v->m_impactKit, false, false);

        // The original passes the cast kit's animation, not the visual, as the callback argument; OnCastEnd then
        // looks it up as a visual id.
        if (const SpellVisualKitRec* cast = SpellVisualKit(v->m_castKit)) {
            SetCallback(model, OnCastEnd, static_cast<uint32_t>(cast->m_animID));
        }
    }

    // FUN_10309450
    bool PlayVisual(CM2Model* model, uint32_t spell, uint32_t visualOverride) {
        SpellRec rec;

        if (!FetchSpell(spell, rec)) {
            return true;
        }

        const SpellVisualRec* v = SpellVisual(visualOverride ? static_cast<int32_t>(visualOverride) : rec.m_spellVisualID[0]);

        if (!v || !SpellVisualKit(v->m_precastKit)) {
            return false;
        }

        const SpellCastTimesRec* castTime = SpellCastTimes(rec.m_castingTimeIndex);

        if (!castTime) {
            return false;
        }

        if (castTime->m_base != 0) {
            if (!PlayKit(model, v->m_precastKit, false, false)) {
                return false;
            }

            SetCallback(model, OnPrecastEnd, static_cast<uint32_t>(v->m_id));
            return true;
        }

        PlayKit(model, v->m_precastKit, false, false);
        PlayKit(model, v->m_castKit, false, false);
        PlayKit(model, v->m_impactKit, false, false);
        SetCallback(model, OnCastEnd, static_cast<uint32_t>(v->m_id));
        return true;
    }

    // ---- missile (0x10BCCD68) -------------------------------------------------------------------------------
    struct Missile {
        CM2Model* model = nullptr;   // +0x00
        uint32_t spell = 0;          // +0x04
        uint32_t start = 0;          // +0x08
        CM2Model* fx = nullptr;      // +0x0C
        float pos[3] = {};           // +0x10
        float height = 0.0f;         // +0x1C
        float speed = 0.0f;          // +0x20
        bool loop = false;           // +0x24
    } s_missile;

    uint32_t s_missileTimer = 0;

    void MissileLaunch(Missile& m) {   // FUN_10112660
        m.start = 0;
        SpellRec rec;

        if (!FetchSpell(m.spell, rec)) {
            return;
        }

        const SpellVisualRec* v = SpellVisual(rec.m_spellVisualID[0]);

        if (!v || v->m_hasMissile == 0) {
            return;
        }

        const SpellVisualEffectNameRec* e = SpellVisualEffectName(v->m_missileModel);

        if (!e) {
            return;
        }

        auto hasAttach = [&](uint32_t a) { return m.model->HasAttachment(a) != 0; };
        int32_t attach = v->m_missileAttachment;

        // The original's fallback picks 0xF exactly when the model has no 0xF (0x101126E2), else 0x13 or -1.
        if (!hasAttach(static_cast<uint32_t>(attach))) {
            if (!hasAttach(0xF)) {
                attach = 0xF;
            } else {
                attach = hasAttach(0x13) ? 0x13 : -1;
            }
        }

        m.fx = m.model->m_scene->CreateModel(e->m_fileName, 0);

        if (!m.fx) {
            return;
        }

        m.fx->AttachToParent(m.model, static_cast<uint32_t>(attach), nullptr, 0);

        // Deviation (as the reconstruction): 0x831330 faults on an attachment the model lacks (0x4C21BC), so only
        // attachments that exist are queried; the others stay at zero.
        auto attachPos = [&](float* out, int32_t a) {
            if (a >= 0 && hasAttach(static_cast<uint32_t>(a))) {
                const C3Vector p = m.model->GetAttachmentPosition(static_cast<uint32_t>(a));
                out[0] = p.x;
                out[1] = p.y;
                out[2] = p.z;
            }
        };

        float hand[3] = {};
        float origin[3] = {};

        if (m.model->HasEvent(0x4C534324)) {          // "$CSL"
            attachPos(hand, 0x15);
        } else if (m.model->HasEvent(0x52534324)) {   // "$CSR"
            attachPos(hand, 0x16);
        } else if (m.model->HasEvent(0x54534324)) {   // "$CST"
            attachPos(hand, 0x22);
        }

        attachPos(origin, attach);

        // Gnome models are measured 0.75 lower (DLL 0x10B2F5D4).
        const char* file = m.model->m_shared->m_fileName;
        float z = origin[2];

        if (strcmp(file, "character\\gnome\\male\\gnomemale.m2") == 0 || strcmp(file, "character\\gnome\\female\\gnomefemale.m2") == 0) {
            z -= 0.75f;
        }

        m.pos[0] = hand[0] - origin[0];
        m.pos[1] = hand[1] - origin[1];
        m.pos[2] = hand[2] - z;
        m.height = hasAttach(0x15) ? m.model->GetAttachmentWorldScale(0x15) : 0.0f;   // same guard
        m.speed = rec.m_speed;
    }

    int32_t MissileTick(const void*, void*) {   // FUN_10112890 (0x10A76860)
        Missile& m = s_missile;

        // Deviation (as the reconstruction): the timer can run before a freshly set model has its M2 data; the
        // attachment calls fault on it, so launch waits for it.
        if (m.start != 0 && OsGetAsyncTimeMs() >= m.start && m.model->m_shared) {
            MissileLaunch(m);
        }

        if (m.fx && m.loop) {
            m.pos[0] += m.speed * 0.001f;

            if (m.pos[0] >= 25.0f) {
                m.fx->DetachFromParent();
                m.fx = nullptr;
            } else {
                m.fx->SetWorldTransform(C3Vector(m.pos[0], m.pos[1], m.pos[2]), 0.0f, m.height);
            }
        }

        s_missileTimer = CoA::Timer::Schedule(16, MissileTick, nullptr);
        return 0;
    }

    // FUN_10309690 + FUN_10112860
    bool PlayMissile(CM2Model* model, uint32_t spell, uint32_t visualOverride, uint32_t start, bool clear, bool loop) {
        SpellRec rec;

        if (!FetchSpell(spell, rec)) {
            return false;
        }

        const SpellVisualRec* v = SpellVisual(visualOverride ? static_cast<int32_t>(visualOverride) : rec.m_spellVisualID[0]);

        if (!v || v->m_hasMissile == 0) {
            return false;
        }

        s_missile.model = model;
        s_missile.spell = spell;
        s_missile.start = start ? start : OsGetAsyncTimeMs();
        s_missile.loop = loop;

        if (clear) {
            model->m_baseAlpha = 0.0f;   // model +0x17C
        }

        if (!s_missileTimer) {
            s_missileTimer = CoA::Timer::Schedule(16, MissileTick, nullptr);
        }

        return true;
    }

    // ---- the two exe jump bytes -----------------------------------------------------------------------------
    void SetFrameJumps(uint8_t op) {
        g_updateModelJump = op;   // 0x5971C1
        g_rotateModelJump = op;   // 0x59714D
    }

    uint32_t s_spell = 0;    // 0x10D3DA04
    uint32_t s_visual = 0;   // 0x10D3DA08

    // Shared body of handler_SetSpell / handler_SetSpellVisual (visual 0 = the spell's own). Spell 0 clears the hands
    // and resets the animation, then still looks the spell up, as the original does.
    int ApplySpell(CM2Model* model, uint32_t spell, uint32_t visual, bool explicitVisual) {
        if (spell == 0) {
            SetFrameJumps(0x75);
            model->DetachAllChildrenById(0x15);
            model->DetachAllChildrenById(0x16);
            PlayAnim(model, 0, 1.0f);
        }

        SpellRec rec;

        if (!FetchSpell(spell, rec)) {
            return 0;
        }

        const SpellVisualRec* v = SpellVisual(explicitVisual ? static_cast<int32_t>(visual) : rec.m_spellVisualID[0]);

        if (!v) {
            return 0;
        }

        if (v->m_stateKit != 0) {
            PlayKit(model, v->m_stateKit, false, true);
        } else if (v->m_hasMissile != 0) {
            PlayMissile(model, spell, 0, 1, false, true);
        } else if (v->m_castKit != 0) {
            SetFrameJumps(0xEB);
            PlayVisual(model, spell, explicitVisual ? visual : 0);
        }

        return 0;
    }

    // handler_SetSpell (0x10a6c1c0): (spell)
    int SetSpell(lua_State* L) {
        CM2Model* model = ModelOf(L);

        if (!model) {
            return 0;
        }

        s_spell = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        return ApplySpell(model, s_spell, 0, false);
    }

    // handler_SetSpellVisual (0x10a6ca30): (spell, visual)
    int SetSpellVisual(lua_State* L) {
        CM2Model* model = ModelOf(L);

        if (!model) {
            return 0;
        }

        s_spell = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        s_visual = static_cast<uint32_t>(ToInt(CheckNumber(L, 3)));
        return ApplySpell(model, s_spell, s_visual, true);
    }

    // 0x10a6be90: the last spell given to any model.
    int GetSpell(lua_State* L) {
        lua_pushinteger(L, static_cast<int32_t>(s_spell));
        return 1;
    }

    // FUN_10a6beb0: sheathe the dress-up frame's two hand items (0xC0E4F8) on the model.
    int SetSheathe(lua_State* L) {
        CM2Model* model = ModelOf(L);

        if (!model) {
            return 0;
        }

        const int32_t items[2] = { s_mainHandItemId, s_offHandItemId };

        for (uint32_t i = 0; i < 2; ++i) {
            const ItemStats_C* item = g_itemDBCache.GetRecord(items[i], WOWGUID(), nullptr, nullptr, false);

            if (!item) {
                continue;
            }

            ItemDisplayInfoRec display;

            if (!g_itemDisplayInfoDB.GetRecord(static_cast<int32_t>(item->m_displayId), &display)) {
                continue;
            }

            const char* texture = display.m_modelTexture[0];
            const std::string path = std::string("Item\\ObjectComponents\\Weapon\\") + (texture ? texture : "") + ".blp";
            const GEOCOMPONENTLINKS slot = CCharacterComponent::GetSheatheLink(static_cast<SHEATHE_TYPE>(item->m_sheatheType), i == 0);
            CCharacterComponent::AddLink(model, slot, display.m_modelName[0], path.c_str(), 0, &display);
        }

        CCharacterComponent::RemoveLinkpt(model, static_cast<GEOCOMPONENTLINKS>(1));
        CCharacterComponent::RemoveLinkpt(model, static_cast<GEOCOMPONENTLINKS>(2));
        return 0;
    }

    // FUN_10a6d2c0: parse the GUID string (StringToGuid 0x74D120) and hand it to the frame (RefreshUnit 0x5977E0).
    // Returns 1 without pushing, as the original does. No type check on self: the original calls the character-model
    // method on any Model frame.
    int SetUnitGUID(lua_State* L) {
        CGCharacterModelBase* frame = static_cast<CGCharacterModelBase*>(Self(L));

        if (!lua_isstring(L, 2)) {
            luaL_error(L, "Usage: SetUnitGUID(\"GUID\")");
        }

        const std::string s = CoA::Script::CheckString(L, 2);
        const WOWGUID guid = StringToGuid(s.c_str());

        if (!guid.m_guid) {
            luaL_error(L, "Usage: SetUnitGUID(\"GUID\")");
        }

        frame->RefreshUnit(guid);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("#Model", "SetSpell", SetSpell),
        COA_NATIVE("#Model", "SetSpellVisual", SetSpellVisual),
        COA_NATIVE("#Model", "GetSpell", GetSpell),
        COA_NATIVE("#Model", "SetUnitGUID", SetUnitGUID),
        COA_NATIVE("#Model", "SetSheathe", SetSheathe),
    };
}

void CoA::ModelMethodsClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
