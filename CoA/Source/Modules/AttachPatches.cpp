// AscAttachPatches.cpp -- see AttachPatches.h. The rows themselves are the "(Extensions.dll patch 0xADDR)" edits in
// the decomp.
#include "CoA/Source/Modules/AttachPatches.h"

namespace {
    bool s_afkLogout = false;
    uint8_t s_runeUiClass = 0;
}

const char* const CoA::AttachPatches::kUiScaleDefault = "0.85";

const int32_t CoA::AttachPatches::kLevels[8] = { 18, 16, 14, 12, 10, 9, 7, 4 };

const float CoA::AttachPatches::kDistances[8] = { 210.0f, 180.0f, 150.0f, 120.0f, 90.0f, 60.0f, 40.0f, 25.0f };

const uint32_t CoA::AttachPatches::kClassColours[33] = {
    0xFF000000, 0xFFC69B6D, 0xFFF48CBA, 0xFFAAD372, 0xFFFFF468, 0xFFFFFFFF, 0xFFC41E3A, 0xFF0070DD,
    0xFF68CCEF, 0xFF9382C9, 0xFFFFD624, 0xFFFF7C0A,
    0xFF8A3303, 0xFF6EFF00, 0xFFA330C9, 0xFFABD473, 0xFF0070DE, 0xFFD64747, 0xFFC79C6E,
    0xFF00FF99, 0xFFCC9900, 0xFFFFF569, 0xFFF2E699, 0xFF8787ED, 0xFFFF300F, 0xFFE0C7FF,
    0xFFB5FFFF, 0xFFF58CBA, 0xFFA3A3A3, 0xFFFF7D0A, 0xFFC41F3B, 0xFF0D2ED6, 0xFF40C7EB,
};

const char* const CoA::AttachPatches::kRaceFileNames[32] = {
    nullptr, "Human", "Orc", "Dwarf", "NightElf", "Scourge", "Tauren", "Gnome", "Troll", "Goblin", "BloodElf",
    "Draenei", "FelOrc", "Naga_", "Broken", "Skeleton", "Vrykul", "Tuskarr", "ForestTroll", "Taunka",
    "NorthrendSkeleton", "IceTroll", "", "", "", "", "", "", "", "", "", "",
};

bool CoA::AttachPatches::AfkLogoutEnabled() {
    return s_afkLogout;
}

void CoA::AttachPatches::SetAfkLogoutEnabled(bool enabled) {
    s_afkLogout = enabled;
}

uint8_t CoA::AttachPatches::RuneUiClass() {
    return s_runeUiClass;
}

void CoA::AttachPatches::SetRuneUiClass(uint8_t powerType) {
    s_runeUiClass = powerType;
}
