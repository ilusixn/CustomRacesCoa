// AscUnitSelect.cpp (see UnitSelect.h).
#include "CoA/Source/Modules/UnitSelect.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Packet.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include <algorithm>
#include <cmath>

namespace {
    CoA::UnitSelect::Deps s_deps;

    void* s_unitSelect = nullptr;   // 0x10BE384C
    void* s_tracking = nullptr;     // 0x10BE3850
    void* s_questSelect = nullptr;  // 0x10BE3854
    void* s_cone30 = nullptr;       // 0x10BE3858
    void* s_cone90 = nullptr;       // 0x10BE385C
    uint32_t s_lastInteract = 0;    // 0x10BE386C
    std::vector<uint32_t> s_questEntries;   // 0x10BE3860
    float s_zPad = 2.0f;            // what the operand at 0x82080B points at (0xA4040C / 0x10BCBF80)

    // The slots the world registration refills (0x10114090).
    CVar* s_self = nullptr;         // 0x10BE3838
    CVar* s_enemy = nullptr;        // 0x10BE383C
    CVar* s_friendly = nullptr;     // 0x10BE3840
    CVar* s_quest = nullptr;        // 0x10BE3844
    CVar* s_circle = nullptr;       // 0x10BE3848
    CVar* s_turnSpeed = nullptr;    // 0x10BE3870
    CVar* s_cursorTime = nullptr;   // 0x10BE3874

    int32_t Int(const CVar* var) {
        return var ? var->m_intValue : 0;
    }

    void* Load(const char* path) {
        return s_deps.loadTexture ? s_deps.loadTexture(path) : nullptr;
    }

    // FUN_102dad10: the float operand at 0x82080B -> 10.0 (0x10BCBF80) or the stock 0xA4040C (2.0).
    void PatchRadius(bool on) {
        s_zPad = on ? 10.0f : 2.0f;

        if (s_deps.setProjectionZPad) {
            s_deps.setProjectionZPad(s_zPad);
        }
    }
}

CoA::UnitSelect::Deps& CoA::UnitSelect::Dependencies() {
    return s_deps;
}

void* const* CoA::UnitSelect::UnitSelectTexture() {
    return &s_unitSelect;
}

void* const* CoA::UnitSelect::TrackingTexture() {
    return &s_tracking;
}

void* const* CoA::UnitSelect::QuestTexture() {
    return &s_questSelect;
}

float CoA::UnitSelect::ProjectionZPad() {
    return s_zPad;
}

void* CoA::UnitSelect::Cone30() {
    return s_cone30;
}

void* CoA::UnitSelect::Cone90() {
    return s_cone90;
}

void CoA::UnitSelect::LoadCones() {
    if (s_cone30 && s_deps.releaseTexture) {
        s_deps.releaseTexture(s_cone30);
    }

    if (s_cone90 && s_deps.releaseTexture) {
        s_deps.releaseTexture(s_cone90);
    }

    s_cone30 = Load("Interface\\SpellShadow\\ConeSplat_Static_30d_30yd.blp");
    s_cone90 = Load("Interface\\SpellShadow\\ConeSplat_Static_90d_30yd.blp");
}

// The three are loaded again on every client initialization; the old handles are not released (as in the original).
void CoA::UnitSelect::AfterObjectInitialize() {
    s_unitSelect = Load("Textures\\UnitSelectTexture.blp");
    s_tracking = Load("Textures\\TrackingTexture.blp");
    s_questSelect = Load("Textures\\UnitSelectTexture-Quest.blp");
}

bool CoA::UnitSelect::SkipAutoTrackCursor() {
    if (s_cursorTime) {
        const uint32_t ms = static_cast<uint32_t>(s_cursorTime->m_intValue);
        const uint32_t now = s_deps.now ? s_deps.now() : 0;

        if (ms && ms < now - s_lastInteract) {
            return true;
        }
    }

    return false;
}

// CMSG 0x51F with three strings (XOR-decoded at run time in the original).
void CoA::UnitSelect::BeforeClickToMove(bool returnPastImage) {
    if (!returnPastImage) {
        return;
    }

    CoA::Packet packet(0x51F);
    packet.Str("Ascension").Str("CTM").Str("Click-to-Move called by bot.");
    packet.Send();
}

// After the original: AutoInteractTurnSpeed's float (+0x2C) is the tracking turn rate, and the time is kept. Ruling: the
// original dereferences the slot unchecked; before the world registration fills it the write is skipped here.
void CoA::UnitSelect::AfterClickToMove() {
    if (s_turnSpeed && s_deps.setTrackingTurnRate) {
        s_deps.setTrackingTurnRate(s_turnSpeed->m_floatValue);
    }

    s_lastInteract = s_deps.now ? s_deps.now() : 0;
}

bool CoA::UnitSelect::IndicatorColour(bool isSelf, int32_t reaction, uint32_t& colour) {
    const CoA::UnitSelect::Colours c = s_deps.colours ? s_deps.colours() : CoA::UnitSelect::Colours();
    colour = c.self;

    if (isSelf) {
        return Int(s_self) != 0;
    }

    if (reaction < 3) {
        if (!Int(s_enemy)) {
            return false;
        }

        colour = c.enemy;
        return true;
    }

    if (reaction <= 3 || !Int(s_friendly)) {
        return false;
    }

    colour = c.friendly;
    return true;
}

void CoA::UnitSelect::Box(const float p[3], float r, float box[6]) {
    box[3] = p[0] + r;
    box[0] = p[0] - r;
    box[4] = p[1] + r;
    box[1] = p[1] - r;
    box[5] = r + r + p[2];
    box[2] = p[2] - (r + r);
}

bool CoA::UnitSelect::IsCone(const uint32_t targetA[3]) {
    for (uint32_t i = 0; i < 3; ++i) {
        if (targetA[i] == 0x18 || targetA[i] == 0x36 || targetA[i] == 0x68 || targetA[i] == 0x3B) {
            return true;
        }
    }

    return false;
}

float CoA::UnitSelect::Radius(float base, float perLevel, float max, uint32_t level) {
    const float l = static_cast<float>(static_cast<double>(level));
    const float r = l * perLevel + base;
    return r > max ? max : r;
}

void CoA::UnitSelect::ConeBox(const float p[3], float r, float facing, float box[6]) {
    r = r * 0.725f;
    CoA::UnitSelect::Box(p, r, box);
    r = r * 0.725f;
    const float dx = static_cast<float>(cos(static_cast<double>(facing))) * r;
    const float dy = static_cast<float>(sin(static_cast<double>(facing))) * r;
    box[5] = box[5] + 0.0f;
    box[3] = dx + box[3];
    box[0] = dx + box[0];
    box[4] = dy + box[4];
    box[1] = dy + box[1];
    box[2] = box[2] + 0.0f;
}

bool CoA::UnitSelect::QuestCircle(uint32_t entry, bool (*relevant)(uint32_t entry, void* ctx), void* ctx) {
    if (std::find(s_questEntries.begin(), s_questEntries.end(), entry) == s_questEntries.end() &&
        !(relevant && relevant(entry, ctx))) {
        return false;
    }

    s_questEntries.push_back(entry);
    return true;
}

const std::vector<uint32_t>& CoA::UnitSelect::QuestEntries() {
    return s_questEntries;
}

void CoA::UnitSelect::Render() {
    if (s_deps.drawIndicators) {
        s_deps.drawIndicators();
    }

    if (Int(s_quest) != 0) {
        s_questEntries.clear();

        if (s_deps.drawQuestCircles) {
            s_deps.drawQuestCircles();
        }
    }

    if (Int(s_circle) == 1 && s_deps.drawOwnCircle) {
        s_deps.drawOwnCircle();
    }
}

// Outside a dungeon / raid (Map.dbc +8 not 1 / 2) the pad is 10.0; inside one, with any aoeRadiusIndicator* 1, the stock
// 2.0 comes back (and stays whatever it was otherwise). No Map row: nothing.
void CoA::UnitSelect::Tick() {
    uint32_t type = 0;

    if (!s_deps.mapInstanceType || !s_deps.mapInstanceType(type)) {
        return;
    }

    if (type != 1 && type != 2) {
        PatchRadius(true);
        return;
    }

    if (Int(s_self) == 1 || Int(s_enemy) == 1 || Int(s_friendly) == 1) {
        PatchRadius(false);
    }
}

void CoA::UnitSelect::Init() {
    // name, help, flags, default, callback, category, setCommand, slot, world
    CoA::ClientHooks::enterWorld.Add(CoA::UnitSelect::LoadCones);
    COA_CVAR("aoeRadiusIndicatorSelf", nullptr, 1, "0", nullptr, 3, false, &s_self, true);
    COA_CVAR("aoeRadiusIndicatorEnemy", nullptr, 1, "0", nullptr, 3, false, &s_enemy, true);
    COA_CVAR("aoeRadiusIndicatorFriendly", nullptr, 1, "0", nullptr, 3, false, &s_friendly, true);
    CoA::ClientHooks::afterClearTrackedObjects.Add(CoA::UnitSelect::Render);
    CoA::ClientHooks::initialSpellsAlternate.Add(CoA::UnitSelect::Tick);
    COA_CVAR("showQuestUnitCircles", nullptr, 1, "1", nullptr, 4, false, &s_quest, true);
    COA_CVAR("ObjectSelectionCircleSelf", nullptr, 1, "0", nullptr, 4, false, &s_circle, true);
    COA_CVAR("AutoInteractTurnSpeed", nullptr, 1, "3.15", nullptr, 3, false, &s_turnSpeed, true);
    COA_CVAR("AutoInteractCursorTime", nullptr, 1, "0", nullptr, 3, false, &s_cursorTime, true);
    PatchRadius(true);
}

void CoA::UnitSelect::Reset() {
    s_deps = Deps();
    s_unitSelect = s_tracking = s_questSelect = s_cone30 = s_cone90 = nullptr;
    s_lastInteract = 0;
    s_questEntries.clear();
    s_zPad = 2.0f;

    for (CVar** slot : { &s_self, &s_enemy, &s_friendly, &s_quest, &s_circle, &s_turnSpeed, &s_cursorTime }) {
        *slot = nullptr;
    }
}
