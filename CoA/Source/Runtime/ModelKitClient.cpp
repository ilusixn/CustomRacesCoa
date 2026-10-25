// AscModelMethods.cpp's kit player and transform helpers (AscModelKit) over the decomp's CM2Model.
#include "CoA/Source/Runtime/ModelKit.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/DB/Rec/SpellVisualEffectNameRec.h"
#include "WoW/Source/DB/Rec/SpellVisualKitModelAttachRec.h"
#include "WoW/Source/DB/Rec/SpellVisualKitRec.h"
#include "WoW/Source/DB/Rec/SpellVisualRec.h"
#include <cmath>
#include <cstring>
#include <vector>

namespace {
    // 4x4 row-major (FUN_102bd230 / 310 / 3f0 / 4d0 / 530).
    struct Mat {
        float m[16];
    };

    const Mat kIdentityMat = { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };

    // FUN_102bcea0
    Mat Mul(const Mat& a, const Mat& b) {
        Mat o;

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                o.m[i * 4 + j] = a.m[i * 4] * b.m[j] + a.m[i * 4 + 1] * b.m[4 + j] + a.m[i * 4 + 2] * b.m[8 + j] + a.m[i * 4 + 3] * b.m[12 + j];
            }
        }

        return o;
    }

    // t = R * t
    void Rotate(Mat& t, int axis, float a) {
        float c = cosf(a);
        float s = sinf(a);
        Mat r = kIdentityMat;

        if (axis == 0) {
            r.m[5] = c;
            r.m[6] = s;
            r.m[9] = -s;
            r.m[10] = c;
        } else if (axis == 1) {
            r.m[0] = c;
            r.m[2] = -s;
            r.m[8] = s;
            r.m[10] = c;
        } else {
            r.m[0] = c;
            r.m[1] = s;
            r.m[4] = -s;
            r.m[5] = c;
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

    void SetMatrix(CM2Model* model, const Mat& t) {
        C44Matrix m;
        static_assert(sizeof(C44Matrix) == sizeof(Mat), "C44Matrix is 16 floats");
        memcpy(&m, t.m, sizeof(t.m));
        model->SetWorldTransform(m);
    }

    const SpellVisualRec* SpellVisual(uint32_t spell) {
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return nullptr;
        }

        return g_spellVisualDB.GetRecord(rec.m_spellVisualID[0]);
    }

    // 0x10309DE0: an attached effect's animation ended -- loop its "stand" (0x9F) or replay.
    void OnEffectAnimEnd(CM2Model* model, uint32_t boneId, uint32_t sequenceId, int32_t interrupted, uint32_t time, uint64_t param) {
        model->SetSequenceCallback(nullptr, 0);

        if (model->HasSequence(0x9F)) {
            CoA::ModelKit::PlayAnim(model, 0x9F, 0.0f);
        } else {
            CoA::ModelKit::PlayAnim(model, sequenceId, 1.0f);
        }
    }

    // FUN_10309560: create the effect model and attach it with offset / rotation / effect scale.
    bool AttachEffect(CM2Model* model, uint32_t effectName, uint32_t attach, bool loop, const float* offset, const float* rot) {
        const SpellVisualEffectNameRec* e = g_spellVisualEffectNameDB.GetRecord(static_cast<int32_t>(effectName));

        if (!e || !model->m_scene) {
            return false;
        }

        CM2Model* fx = model->m_scene->CreateModel(e->m_fileName, 0);

        if (!fx) {
            return false;
        }

        if (loop) {
            fx->SetSequenceCallback(reinterpret_cast<void*>(&OnEffectAnimEnd), 0);
        }

        fx->AttachToParent(model, attach, nullptr, 0);
        Mat t = kIdentityMat;

        if (offset) {
            Translate(t, offset);
        }

        if (rot) {
            Rotate(t, 0, rot[0]);
            Rotate(t, 1, rot[1]);
            Rotate(t, 2, rot[2]);
        }

        Scale(t, e->m_scale);
        SetMatrix(fx, t);
        return true;
    }
}

void CoA::ModelKit::SetTransform(CM2Model* model, const Transform& x) {
    Mat t = kIdentityMat;
    Translate(t, x.pos);
    Rotate(t, 0, x.rot[0]);
    Rotate(t, 1, x.rot[1]);
    Rotate(t, 2, x.rot[2]);
    Scale(t, x.scale);
    SetMatrix(model, t);
}

bool CoA::ModelKit::PlayKit(CM2Model* model, uint32_t kitId, bool noAnim, bool loop) {
    const SpellVisualKitRec* kit = g_spellVisualKitDB.GetRecord(static_cast<int32_t>(kitId));

    if (!kit) {
        return false;
    }

    std::vector<uint32_t> attached;
    // SpellVisualKitModelAttach: {kit, effect, attachment, offset xyz, yaw / pitch / roll}.
    for (int32_t i = 0; i < g_spellVisualKitModelAttachDB.m_numRecords; ++i) {
        const SpellVisualKitModelAttachRec& a = g_spellVisualKitModelAttachDB.m_records[i];

        if (a.m_parentSpellVisualKitID != kit->m_id) {
            continue;
        }

        const float rot[3] = { a.m_yaw, a.m_pitch, a.m_roll };

        if (AttachEffect(model, a.m_spellVisualEffectNameID, a.m_attachmentID, loop, a.m_offset, rot)) {
            attached.push_back(a.m_attachmentID);
        }
    }

    // Kit column, attachment point.
    const struct {
        int32_t effect;
        uint32_t attach;
    } columns[] = {
        { kit->m_headEffect, 0x14 }, { kit->m_chestEffect, 0x22 }, { kit->m_baseEffect, 0x13 }, { kit->m_leftHandEffect, 0x15 },
        { kit->m_rightHandEffect, 0x16 }, { kit->m_breathEffect, 0x11 }, { kit->m_leftWeaponEffect, 0x20 }, { kit->m_rightWeaponEffect, 0x21 },
        { kit->m_specialEffect[0], 0x17 }, { kit->m_specialEffect[1], 0x18 }, { kit->m_specialEffect[2], 0x19 }, { kit->m_worldEffect, 0x00 },
    };

    for (const auto& c : columns) {
        if (AttachEffect(model, static_cast<uint32_t>(c.effect), c.attach, loop, nullptr, nullptr)) {
            attached.push_back(c.attach);
        }
    }

    uint32_t anim = static_cast<uint32_t>(kit->m_animID);

    if (!noAnim && anim != 0xFFFFFFFF) {
        CoA::ModelKit::PlayAnim(model, anim, 1.0f);
        noAnim = false;
    }

    return !(attached.empty() && (noAnim || anim == 0xFFFFFFFF));
}

bool CoA::ModelKit::PlayStateKit(CM2Model* model, uint32_t spell, bool noAnim) {
    const SpellVisualRec* v = SpellVisual(spell);
    return v && PlayKit(model, static_cast<uint32_t>(v->m_stateKit), noAnim, true);
}

bool CoA::ModelKit::PlayPrecastKit(CM2Model* model, uint32_t spell, bool noAnim) {
    const SpellVisualRec* v = SpellVisual(spell);
    return v && PlayKit(model, static_cast<uint32_t>(v->m_precastKit), noAnim, false);
}

void CoA::ModelKit::PlayAnim(CM2Model* model, uint32_t anim, float speed) {
    // The original passes a float NaN (0x7FC00000) in the variation slot.
    model->SetBoneSequence(0xFFFFFFFF, anim, 0x7FC00000, 0, speed, 1, 1);
}

void CoA::ModelKit::Detach(CM2Model* model, uint32_t attach) {
    model->DetachAllChildrenById(attach);
}
