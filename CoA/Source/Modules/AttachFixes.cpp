#include "CoA/Source/Modules/AttachFixes.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/Packet.h"
#include <cstdlib>
#include <cstring>

namespace {
    CoA::AttachFixes::Deps s_deps;

    // DAT_10D3D824: -1 = the next PutActionInSlot becomes ClearCursor(1, 1); 1 = it runs once; 0 = idle.
    int32_t s_castState = 0;
    // The je at 0x542D4E as FUN_10a43820 leaves it for the call.
    CoA::AttachFixes::Placement s_placement = CoA::AttachFixes::Placement::Stock;
    // DAT_10BCC8DA (initialised 1).
    bool s_allowSpecialEffect = true;
    // PTR_DAT_10BCCAD8.
    const char* s_markerModel = "";
    // The client constant 0xA104B0 (stock 625 = 25^2).
    float s_chatBubbleRangeSq = 625.0f;
}

CoA::AttachFixes::Deps& CoA::AttachFixes::Dependencies() {
    return s_deps;
}

uint32_t CoA::AttachFixes::CustomAttr(uint32_t spell, uint32_t offset) {
    uint32_t a10 = 0;
    uint32_t a14 = 0;
    uint32_t a18 = 0;

    if (!s_deps.spellCustomAttr || !s_deps.spellCustomAttr(spell, a10, a14, a18)) {
        return 0;
    }

    return offset == 0x10 ? a10 : offset == 0x14 ? a14 : offset == 0x18 ? a18 : 0;
}

void CoA::AttachFixes::LogMissingFileOnce(const char* name) {
    CoA::Logger::WriteOnce(CoA::Logger::MissingFiles, name ? name : "");
}

bool CoA::AttachFixes::ReplaceNameToken(std::string& name, const char* guidName) {
    if (name.size() < 3 || name.find("$CN") == std::string::npos || !guidName) {
        return false;
    }

    name.replace(0, 3, guidName);
    return true;
}

bool CoA::AttachFixes::AddUnknownEventWhileLoading(const char* name) {
    if (!s_deps.worldLoading || !s_deps.eventId || !s_deps.worldLoading() || s_deps.eventId(name) != -1) {
        return false;
    }

    // The original copies the name into a block of its own that the event table keeps.
    const size_t n = strlen(name) + 1;
    char* copy = static_cast<char*>(malloc(n));
    memcpy(copy, name, n);
    CoA::Events::Queue(copy);

    if (s_deps.rebuildEvents) {
        s_deps.rebuildEvents();
    }

    return true;
}

uint8_t CoA::AttachFixes::LfgRoleClass(uint8_t playerClass) {
    uint32_t roles = playerClass == 8 ? 0xD : playerClass == 9 ? 0xB : 0;

    if (const uint8_t* row = CoA::Dbc::Get("DBFilesClient\\ChrClassesRoles.dbc").Row(playerClass)) {
        roles |= CoA::Dbc::Table::U32(row, 4) & 0xE;

        for (uint32_t i = 0; i < 3; ++i) {
            const uint32_t extra = CoA::Dbc::Table::U32(row, 8 + i * 4) & 0xE;

            if (extra && s_deps.roleCondition &&
                s_deps.roleCondition(CoA::Dbc::Table::I32(row, 0x14 + i * 4), CoA::Dbc::Table::U32(row, 0x20 + i * 4))) {
                roles |= extra;
            }
        }
    }

    if ((roles & 0xE) == 0) {
        return playerClass;
    }

    if (roles & 2) {
        return (roles & 4) ? 0xB : 1;   // tank (+ healer): druid, else warrior
    }

    if (roles & 4) {
        return 5;                       // healer: priest
    }

    return (roles & 8) ? 4 : 0;         // damage: rogue
}

const char* CoA::AttachFixes::MarkerModel(uint32_t kind, int32_t state) {
    switch (kind) {
        case 1:
            switch (state) {
                case 1: return "";
                case 2: case 8: return "interface\\buttons\\talktome_new.m2";
                case 3: case 6: case 9: return "interface\\buttons\\talktomequestion_new_ltblue.m2";
                case 4: case 7: return "interface\\buttons\\talktomeblue_new.m2";
                case 5: return "interface\\buttons\\talktomequestionmark_new_grey.m2";
                case 10: return "interface\\buttons\\talktomequestionmark_new.m2";
            }
            break;
        case 2:
            switch (state) {
                case 1: return "";
                case 2: case 8: return "interface\\buttons\\talktome_journey.m2";
                case 3: case 6: case 9: return "interface\\buttons\\talktome_callingsquestion.m2";
                case 4: case 7: return "interface\\buttons\\talktome_callings.m2";
                case 5: return "interface\\buttons\\talktomequestion_journey_grey.m2";
                case 10: return "interface\\buttons\\talktomequestion_journey.m2";
            }
            break;
        case 3:
            switch (state) {
                case 1: return "";
                case 2: case 8: return "interface\\buttons\\talktome_important.m2";
                case 5: return "interface\\buttons\\talktomequestion_important_grey.m2";
                case 10: return "interface\\buttons\\talktomequestion_important.m2";
            }
            break;
        case 4:
            switch (state) {
                case 1: return "interface\\buttons\\talktomeorange_new_grey.m2";
                case 2: case 8: return "interface\\buttons\\talktomeorange_new.m2";
                case 5: return "interface\\buttons\\talktome_new_questionlegendary_grey.m2";
                case 10: return "interface\\buttons\\talktome_new_questionlegendary.m2";
            }
            break;
        case 5:
            if (state == 8) {
                return "interface\\buttons\\talktome_petbattles.m2";
            }
            break;
        case 6:
            switch (state) {
                case 1: return "";
                case 2: case 8: return "interface\\buttons\\talktome_asc.m2";
                case 3: case 6: return "interface\\buttons\\talktome_questionmark_daily_asc.m2";
                case 4: case 7: return "interface\\buttons\\talktome_daily_asc.m2";
                case 5: return "interface\\buttons\\talktome_questionmark_grey_asc.m2";
                case 10: return "interface\\buttons\\talktome_questionmark_asc.m2";
            }
            break;
    }

    return nullptr;
}

const char*& CoA::AttachFixes::CurrentMarkerModel() {
    return s_markerModel;
}

int32_t CoA::AttachFixes::AuctionDeposit(uint32_t depositRate, uint32_t sellPrice, uint32_t runTime, uint32_t itemQuality, float rate) {
    if (itemQuality == 6 || (runTime != 720 && runTime != 1440 && runTime != 2880)) {
        return 1;
    }

    float deposit = static_cast<float>(static_cast<double>(depositRate)) * static_cast<float>(static_cast<double>(sellPrice)) / 100.0f *
                    static_cast<float>(static_cast<double>(runTime)) * 0.004166667f;
    deposit = static_cast<float>(static_cast<long double>(rate) * deposit);

    if (1.0f <= deposit) {
        return static_cast<int32_t>(static_cast<int64_t>(deposit));   // _ftol2
    }

    return 1;
}

void CoA::AttachFixes::SendGuidCleared(uint64_t guid) {
    CoA::Packet(0x741).U64(guid).Send();
}

void CoA::AttachFixes::SendTamperReport(const char* what) {
    const std::string text = std::string("External call to ") + what + ". Hacker/botter.";
    CoA::Packet(0x51F).Str("Ascension").Str(what).Str(text.c_str()).Send();
}

void CoA::AttachFixes::BeginAddKnownSpell(uint32_t spell, int32_t spellClassSet, int32_t progressionRank) {
    if (spellClassSet == 0) {
        s_castState = -1;
    }

    if ((CoA::AttachFixes::CustomAttr(spell, 0x10) & 0x10000) && progressionRank < 2) {
        s_castState = 1;
        s_placement = Placement::Always;   // jmp 0x542DA2: the level / rank / attribute checks skipped
    }

    if (CoA::AttachFixes::CustomAttr(spell, 0x14) & 0x1000000) {
        s_castState = -1;
        s_placement = Placement::Never;    // jmp 0x542DEB: no placement
    }
}

void CoA::AttachFixes::EndAddKnownSpell() {
    s_castState = 0;
    s_placement = Placement::Stock;
}

CoA::AttachFixes::Placement CoA::AttachFixes::PlacementJump() {
    return s_placement;
}

bool CoA::AttachFixes::TakeSkipPutAction() {
    if (s_castState == -1) {
        s_castState = 0;
        return true;
    }

    if (s_castState == 1) {
        s_castState = 0;
    }

    return false;
}

void CoA::AttachFixes::UnitLevelUpdated(bool isUnit) {
    if (isUnit) {
        s_allowSpecialEffect = false;
    }
}

bool CoA::AttachFixes::TakeSkipSpecialEffect() {
    if (s_allowSpecialEffect) {
        return false;
    }

    s_allowSpecialEffect = true;
    return true;
}

void CoA::AttachFixes::ClampSpellEffect(int32_t& auraType, int32_t& miscValue) {
    const uint32_t type = static_cast<uint32_t>(auraType);

    if (type > 0x13C) {
        auraType = 4;
    } else if ((type == 0x6B || type == 0x6C) && miscValue > 0x1E) {
        auraType = 4;
        miscValue = 0;
    }
}

bool CoA::AttachFixes::RefusedInPvP(uint32_t spell) {
    switch (spell) {
        case 0xEF373: case 0x9788B3: case 0x9788B5: case 0x9788B6: case 0x9788B7: case 0x9788B8: case 0x9788B9:
        case 0x9788BA: case 0x9788BB: case 0x9788BC: case 0x9788BF: case 0x9788C0: case 0x9788C1: case 0x9788D6:
        case 0x9788D7: case 0x9788D8: case 0x978916: case 0x978925: case 0x978926:
            return true;
    }

    return false;
}

float& CoA::AttachFixes::ChatBubbleRangeSq() {
    return s_chatBubbleRangeSq;
}

void CoA::AttachFixes::SetChatBubbleRange(int32_t type, int32_t mapInstanceType) {
    if (type == 0xC || type == 0xE || type == 0x33) {
        s_chatBubbleRangeSq = mapInstanceType == 1 || mapInstanceType == 2 ? 28900.0f : 10000.0f;
    } else {
        s_chatBubbleRangeSq = 625.0f;
    }
}
