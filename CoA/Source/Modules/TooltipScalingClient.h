#ifndef COA_SOURCE_MODULES_TOOLTIPSCALINGCLIENT_H
#define COA_SOURCE_MODULES_TOOLTIPSCALINGCLIENT_H

#include <cstdint>
#include <string>

class CGItem_C;
class CGTooltip;
class CImVector;
class ItemStats_C;
struct lua_State;

// AscTooltipScaling.cpp's client half: what the decomp sites call.
namespace CoA::TooltipScalingClient {
    // FUN_1027b110 over 0x61FEC0 (CGTooltip::AddLine): true drops the line; otherwise left / right / the colours are what
    // the client adds (the strings live in the caller's buffers).
    bool BeforeAddLine(CGTooltip* tooltip, const char*& left, const char*& right, CImVector& leftColor, CImVector& rightColor,
        std::string& leftBuffer, std::string& rightBuffer);
    // sub_10287640 over 0x61A8F0 (GetFeralAttackPower): true when the module computed it; damage is rewritten either way.
    bool FeralAttackPower(const ItemStats_C* stats, float& damage, int32_t& result);
    // sub_1027ae80, replacing 0x708540 (CGItem_C::GetRepairCost).
    int32_t RepairCost(CGItem_C* item);
    // sub_1027ade0 after 0x754400 (CGBag_C::GetItemPointer).
    void KeepItemObject(CGItem_C* item);
    // sub_1027ae00 after 0x5E9250 (CGCharacterInfo::CanEquipItemInSlot).
    bool AfterCanEquip(const ItemStats_C* stats, bool result);
    // sub_1027afc0 / FUN_10283f20 before 0x62F9E0 / 0x6304A0.
    void BeforeInboxItem(lua_State* L);
    void BeforeGuildBankItem(lua_State* L);
}

// Engine/Source/Frame/CSimpleFrame_Scripts.cpp: Has/Get/SetScript (0x49EAB0 / 0x49EB70 / 0x49EC80) ask this first;
// a result >= 0 is returned instead of running the client's.
extern int32_t (*g_frameScriptHook)(int32_t which, lua_State* L);

#endif
