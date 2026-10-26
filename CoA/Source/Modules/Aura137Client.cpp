// AscAura137.cpp's client questions: the active player (0x4CEE50) and Spell.dbc EffectApplyAuraName (+0x17C).
#include "CoA/Source/Modules/Aura137.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"

namespace {
    bool IsActivePlayer(void* unit) {
        return static_cast<CGUnit_C*>(unit)->IsActivePlayer();
    }

    bool HasAura137(const void* spell) {
        const SpellRec* rec = static_cast<const SpellRec*>(spell);

        for (uint32_t i = 0; i < 3; ++i) {
            if (rec->m_effectAura[i] == 0x137) {
                return true;
            }
        }

        return false;
    }
}

void CoA::Aura137Client::Init() {
    CoA::Aura137::Deps& d = CoA::Aura137::Dependencies();
    d.isActivePlayer = IsActivePlayer;
    d.hasAura137 = HasAura137;
}
