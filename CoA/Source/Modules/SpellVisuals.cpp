// AscSpellVisuals.cpp (see SpellVisuals.h): server-assigned spell visuals.
#include "CoA/Source/Modules/SpellVisuals.h"
#include "CoA/Source/Modules/Appearance.h"
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include <cstring>
#include <unordered_map>

namespace {
    using Map = std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>>>;

    CoA::SpellVisuals::Deps s_deps;
    Map s_visuals;              // 0x10BE447C
    CVar* s_option = nullptr;   // DAT_10BE4478, refilled by each world registration

    const uint32_t kPlayerMask = 0x10;   // TYPEMASK_PLAYER
    const uint32_t kUnitMask = 8;        // TYPEMASK_UNIT

    uint64_t ActiveGuid() {
        return s_deps.activeGuid ? s_deps.activeGuid() : 0;
    }

    void* ObjectByGuid(uint64_t guid, uint32_t mask) {
        return s_deps.objectByGuid ? s_deps.objectByGuid(guid, mask) : nullptr;
    }

    bool Info(void* object, CoA::SpellVisuals::ObjectInfo& out) {
        return s_deps.objectInfo && s_deps.objectInfo(object, out);
    }

    bool IsPlayerGuid(uint64_t guid) {   // FUN_102ea1c0
        return guid != 0 && static_cast<uint32_t>(guid >> 32) < 0x10000000;
    }

    uint32_t AttrAt(uint32_t spell, uint32_t offset) {
        const uint8_t* row = CoA::CA::SpellCustomAttrRow(spell);
        uint32_t value = 0;

        if (row) {
            memcpy(&value, row + offset, 4);
        }

        return value;
    }

    // FUN_103233a0: a spell flagged SpellCustomAttr +0x18 & 0x40 on an object that is not us, nor (a unit) charmed or
    // summoned by us.
    bool OthersFlaggedEffect(const CoA::SpellVisuals::ObjectInfo& o, uint32_t spell) {
        if (!CoA::CA::SpellCustomAttrRow(spell) || !(AttrAt(spell, 0x18) & 0x40)) {
            return false;
        }

        const uint64_t active = ActiveGuid();

        if (o.guid == active) {
            return false;
        }

        if (o.type == 3 && (o.charmedBy == active || o.summonedBy == active)) {
            return false;
        }

        return true;
    }

    // The shared body of 0x10323410 / 0x10323480 (FUN_10324a10(spell, 8)).
    bool HideFor(void* object, uint32_t spell) {
        CoA::SpellVisuals::ObjectInfo o;

        if (!Info(object, o)) {
            return false;
        }

        if (CoA::CA::SpellCustomAttrRow(spell) && (AttrAt(spell, 0x14) & 8)) {
            if (o.type == 4 || o.type == 3) {
                if (o.health == 0) {
                    return true;
                }
            } else if (o.type == 7) {
                return true;
            }
        }

        return OthersFlaggedEffect(o, spell);
    }
}

CoA::SpellVisuals::Deps& CoA::SpellVisuals::Dependencies() {
    return s_deps;
}

uint32_t CoA::SpellVisuals::GuidKind(uint64_t guid) {
    const uint32_t hi = static_cast<uint32_t>(guid >> 32);

    if (hi < 0x10000000) {
        return 4;
    }

    switch (hi >> 16) {
        case 0xF100: return 6;
        case 0x1FC0: return 5;
        case 0x4000: return 1;
        case 0xF101: return 7;
        case 0xF110: return 5;
        case 0xF130:
        case 0xF140:
        case 0xF150: return 3;
        default: return 0;
    }
}

uint32_t CoA::SpellVisuals::GuidCounter(uint64_t guid) {
    const uint32_t lo = static_cast<uint32_t>(guid);
    const uint32_t hi = static_cast<uint32_t>(guid >> 32);

    if (hi < 0x10000000) {
        return lo;
    }

    switch (hi >> 16) {
        case 0xF101:
        case 0x1FC0:
        case 0x1F40:
        case 0x1F50:
        case 0x4000:
        case 0xF100:
            return lo;
        default:
            return lo & 0xFFFFFF;
    }
}

uint32_t CoA::SpellVisuals::Resolve(uint32_t kind, uint64_t guid, uint32_t spell) {
    if (CoA::ClientHooks::visualHide.AnyTrue(kind, static_cast<uint32_t>(guid), static_cast<uint32_t>(guid >> 32), spell)) {
        return 0xFFFFFFFF;
    }

    uint32_t visual;

    // FUN_100a55d0(out, 0, counter): the counter as a GUID with a zero high half.
    if (kind == 4 && static_cast<uint64_t>(GuidCounter(guid)) == ActiveGuid() && s_deps.playerSpellVisual &&
        s_deps.playerSpellVisual(spell, visual)) {
        return visual;
    }

    auto k = s_visuals.find(kind);

    if (k == s_visuals.end()) {
        return 0;
    }

    auto c = k->second.find(GuidCounter(guid));

    if (c == k->second.end()) {
        return 0;
    }

    auto s = c->second.find(spell);
    return s != c->second.end() ? s->second : 0;
}

bool CoA::SpellVisuals::OptionOn() {
    return s_option && s_option->m_intValue != 0;
}

bool CoA::SpellVisuals::IsHelpful(const CoA::SpellApi::SpellFields& spell) {
    for (uint32_t i = 0; i < 3; ++i) {
        if (!CoA::SpellApi::IsPositiveEffect(spell, i, true)) {
            return false;
        }
    }

    return true;
}

int32_t CoA::SpellVisuals::OnSpellVisualOverride(void*, uint32_t, uint32_t, CDataStore* msg) {
    const uint64_t guid = CoA::Read::U64(msg);
    const uint32_t spell = CoA::Read::U32(msg);
    const uint32_t visual = CoA::Read::U32(msg);
    s_visuals[GuidKind(guid)][GuidCounter(guid)][spell] = visual;
    return 1;
}

bool CoA::SpellVisuals::SkipAppropriateVisual(void* object, bool fromSpellGo, const CoA::SpellApi::SpellFields& spell) {
    if (!fromSpellGo || (s_deps.activePlayer && object == s_deps.activePlayer())) {
        return false;
    }

    ObjectInfo o;
    return Info(object, o) && o.type == 4 && OptionOn() && !IsHelpful(spell);
}

uint32_t CoA::SpellVisuals::AppropriateVisual(void* object, uint32_t spell) {
    ObjectInfo o;

    if (!Info(object, o)) {
        return 0;
    }

    return Resolve(o.type, o.guid, spell);
}

uint32_t CoA::SpellVisuals::MissileHitVisual(uint64_t caster, uint32_t spell) {
    return Resolve(GuidKind(caster), caster, spell);
}

bool CoA::SpellVisuals::SkipDynamicObject(uint64_t caster, uint32_t spell) {
    if (!OptionOn() || caster == 0 || caster == ActiveGuid() || !ObjectByGuid(caster, kPlayerMask)) {
        return false;
    }

    // FetchSpell into a stack record: a missing row leaves the original's record uninitialised (ledgered ruling: no row
    // reads as zero fields).
    CoA::SpellApi::SpellFields fields;

    if (!s_deps.spell || !s_deps.spell(spell, fields)) {
        fields = CoA::SpellApi::SpellFields();
    }

    return !IsHelpful(fields);
}

void CoA::SpellVisuals::AfterFireMissiles(const Missile& m) {
    const uint64_t active = ActiveGuid();
    const uint64_t guid = m.caster;

    if (guid != active) {
        if (guid != 0 && static_cast<uint32_t>(guid >> 32) < 0x10000000) {
            return;
        }

        void* unit = ObjectByGuid(guid, kUnitMask);
        ObjectInfo o;

        if (!unit || !Info(unit, o)) {
            return;
        }

        if (o.summonedBy != 0 && static_cast<uint32_t>(o.summonedBy >> 32) < 0x10000000 && o.summonedBy != active) {
            return;
        }

        if (IsPlayerGuid(o.charmedBy) && o.charmedBy != active) {
            return;
        }

        if (IsPlayerGuid(o.createdBy) && o.createdBy != active) {
            return;
        }
    }

    CoA::Packet(0x9C7)
        .U64(m.caster)
        .U64(m.target)
        .U32(m.spell)
        .U8(m.castId)
        .F32(m.launch[0])
        .F32(m.launch[1])
        .F32(m.launch[2])
        .F32(m.impact[0])
        .F32(m.impact[1])
        .F32(m.impact[2])
        .Send();
}

bool CoA::SpellVisuals::HideVisual(uint32_t, uint32_t guidLo, uint32_t guidHi, uint32_t spell) {
    void* object = ObjectByGuid(guidLo | static_cast<uint64_t>(guidHi) << 32, kPlayerMask);
    return object && HideFor(object, spell);
}

bool CoA::SpellVisuals::RemoveEffect(void* unit, void* effect) {
    return HideFor(unit, s_deps.effectSpell ? s_deps.effectSpell(effect) : 0);
}

bool CoA::SpellVisuals::BeforeAuraApply(void* unit, uint32_t, const void* spellRec) {
    uint32_t spell;
    memcpy(&spell, spellRec, 4);
    ObjectInfo o;

    if (!Info(unit, o)) {
        return true;
    }

    const uint32_t visual = Resolve(GuidKind(o.guid), o.guid, spell);

    if (s_deps.setRecordVisual) {
        if (visual == 0xFFFFFFFF) {
            s_deps.setRecordVisual(spellRec, 0);
        } else if (visual && s_deps.visualRowExists && s_deps.visualRowExists(visual)) {
            s_deps.setRecordVisual(spellRec, visual);
        }
    }

    if (spell != 0x31029) {
        return true;
    }

    if (s_deps.activePlayer && unit == s_deps.activePlayer()) {
        return false;
    }

    return o.summonedBy != ActiveGuid();
}

void CoA::SpellVisuals::Init() {
    // FUN_10322fb0, in its order: the four detours (SpellVisualsClient / the marked client sites), SMSG 0x662, the three
    // lists, the CVar.
    COA_PACKET(0x662, CoA::SpellVisuals::OnSpellVisualOverride);
    CoA::ClientHooks::removeEffect.Add(CoA::SpellVisuals::RemoveEffect, 0x1032313D);
    CoA::ClientHooks::visualHide.Add(CoA::SpellVisuals::HideVisual, 0x10323147);
    CoA::ClientHooks::addAuraEffect.Add(CoA::SpellVisuals::BeforeAuraApply, 0x10323151);
    // name, help, flags, default, callback, category, setCommand, slot, world
    COA_CVAR("hideOtherPlayerHarmfulSpellVisuals", nullptr, 1, "0", nullptr, 1, false, &s_option, true);
    s_deps.playerSpellVisual = CoA::Appearance::PlayerSpellVisual;
}

void CoA::SpellVisuals::Reset() {
    s_deps = Deps();
    s_visuals.clear();
    s_option = nullptr;
}
