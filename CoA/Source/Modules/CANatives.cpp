// AscCAMgr.cpp's read-only bindings: investment and budgets (active and pending), entry and spell queries, entry lists,
// known entries / spells / talent ranks, unit builds, qualities, tags, categories, class info, PickupSpell, HasRuneUI,
// CanUseBrowser and CA_GetCreditAmount; and the helpers every CA native file shares (CANativesInternal.h).
#include "CoA/Source/Modules/CANativesInternal.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>

namespace {
#include "CoA/Source/Modules/CAEnums.generated.inc"
}

using namespace CoA::CA;
using namespace CoA::CA::detail;
using CoA::Script::CheckNumber;
using CoA::Script::CheckString;
using CoA::Script::PushBool;
using CoA::Script::PushInt;
using CoA::Script::PushNils;
using CoA::Script::PushStr;
using CoA::Script::ReadNumber;
using CoA::Script::ReadString;
using CoA::Script::ToInt;
using CoA::Script::ValidateInput;

// ---- the shared helpers ----------------------------------------------------------------------------------------
int CoA::CA::detail::Usage(lua_State* L, const char* msg) {
    return luaL_error(L, msg);
}

uint32_t CoA::CA::detail::EnumByName(const char* name, const char* const* names, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if (strcmp(name, names[i]) == 0) {
            return i + 1;
        }
    }

    CoA::Log::Printf("Unexpected Value: %s", name);
    return 0;
}

uint32_t CoA::CA::detail::EnumArg(lua_State* L, int idx, const char* const* names, uint32_t count) {
    const char* s = CheckString(L, idx);

    for (uint32_t i = 0; i < count; ++i) {
        if (strcmp(s, names[i]) == 0) {
            return i + 1;
        }
    }

    CoA::Log::Printf("lua_tovalue<enum>: Unknown string '%s'", s);
    return 0;
}

bool CoA::CA::detail::ClassMinArgs(lua_State* L, uint32_t& cls, uint32_t& minimum) {
    if (!ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
        return false;
    }

    cls = EnumArg(L, 1, kClassTypes, 0x2F);
    minimum = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
    return true;
}

bool CoA::CA::detail::ClassTabMinArgs(lua_State* L, uint32_t& cls, uint32_t& tab, uint32_t& minimum) {
    if (!ValidateInput(L, { LUA_TSTRING, LUA_TSTRING, LUA_TNUMBER })) {
        return false;
    }

    cls = EnumArg(L, 1, kClassTypes, 0x2F);
    tab = EnumArg(L, 2, kTabTypes, 0x5F);
    minimum = static_cast<uint32_t>(ToInt(CheckNumber(L, 3)));
    return true;
}

std::string CoA::CA::detail::ClassName(uint32_t cls) {
    if (const uint8_t* ct = ClassTypes().Row(cls)) {
        return ClassTypes().Str(ct, 4);
    }

    return "INVALID_CLASS_TYPE_" + std::to_string(cls);
}

std::string CoA::CA::detail::TabName(uint32_t tab) {
    if (const uint8_t* tt = TabTypes().Row(tab)) {
        return TabTypes().Str(tt, 4);
    }

    return "INVALID_TAB_TYPE_" + std::to_string(tab);
}

namespace {
    void SetInt(lua_State* L, const char* k, int32_t v) {
        lua_pushstring(L, k);
        lua_pushinteger(L, v);
        lua_settable(L, -3);
    }

    void SetNum(lua_State* L, const char* k, float v) {
        lua_pushstring(L, k);
        lua_pushnumber(L, v);
        lua_settable(L, -3);
    }

    void SetStr(lua_State* L, const char* k, const std::string& v) {
        lua_pushstring(L, k);
        lua_pushstring(L, v.c_str());
        lua_settable(L, -3);
    }

    void SetInts(lua_State* L, const char* k, const std::vector<uint32_t>& v) {   // FUN_1009a460
        lua_pushstring(L, k);
        PushIntArray(L, v);
        lua_settable(L, -3);
    }

    std::vector<uint32_t> NonZero(Row r, uint32_t off, uint32_t count) {
        std::vector<uint32_t> v;

        for (uint32_t i = 0; i < count; ++i) {
            if (const uint32_t x = RowU32(r, off + i * 4)) {
                v.push_back(x);
            }
        }

        return v;
    }

    int32_t I(uint32_t v) {
        return static_cast<int32_t>(v);
    }
}

void CoA::CA::detail::PushIntArray(lua_State* L, const std::vector<uint32_t>& v) {
    lua_createtable(L, 0, static_cast<int>(v.size()));
    lua_checkstack(L, 2);

    for (size_t i = 0; i < v.size(); ++i) {
        lua_pushnumber(L, static_cast<double>(i + 1));
        lua_pushinteger(L, static_cast<int32_t>(v[i]));
        lua_settable(L, -3);
    }
}

// FUN_101c03c0 builds the entry, FUN_1016c910 serialises it.
void CoA::CA::detail::PushEntry(lua_State* L, Row r) {
    if (!r) {
        lua_pushnil(L);
        return;
    }

    lua_createtable(L, 0, 0x25);
    lua_checkstack(L, 2);
    SetInt(L, "ID", I(RowU32(r, 0)));
    SetStr(L, "Type", RowStr(r, 4));
    SetInts(L, "Spells", NonZero(r, 0x14, 9));
    SetStr(L, "Class", ClassName(RowU32(r, 0x80)));
    SetStr(L, "Name", RowStr(r, 0xBC));
    SetInt(L, "RequiredLevel", I(RowU32(r, ModeColumn(0x68, 0x6C, 0x70))));
    SetStr(L, "Icon", RowStr(r, 0xC0));
    SetInt(L, "AECost", I(RowU32(r, ModeColumn(0x38, 0x48, 0x58))));
    SetInt(L, "TECost", I(RowU32(r, ModeColumn(0x3C, 0x4C, 0x5C))));
    SetStr(L, "Quality", RowStr(r, ModeColumn(0x40, 0x50, 0x60)));
    SetInt(L, "QualityCost", I(RowU32(r, ModeColumn(0x44, 0x54, 0x64))));
    SetInt(L, "Column", I(RowU32(r, 0x78)));
    SetInt(L, "Row", I(RowU32(r, 0x7C)));
    SetStr(L, "Tab", TabName(RowU32(r, 0x84)));
    SetInt(L, "RequiredAEInvestment", I(RowU32(r, 0x88)));
    SetInt(L, "RequiredTEInvestment", I(RowU32(r, 0x8C)));
    SetInt(L, "RequiredClassAEInvestment", I(RowU32(r, 0x90)));
    SetInt(L, "RequiredClassTEInvestment", I(RowU32(r, 0x94)));
    SetInt(L, "RequiredTabAEInvestment", I(RowU32(r, 0x98)));
    SetInt(L, "RequiredTabTEInvestment", I(RowU32(r, 0x9C)));
    SetInt(L, "Points", I(RowU32(r, 0xA0)));
    SetInt(L, "RequiredClassPoints", I(RowU32(r, 0xA4)));
    SetInt(L, "Flags", I(RowU32(r, 0x124)));
    SetInts(L, "Masteries", NonZero(r, 0x1B0, 3));
    SetInts(L, "ConnectedNodes", NonZero(r, 0xE4, 15));
    SetInt(L, "Group", I(RowU32(r, 0x74)));
    SetNum(L, "PositionX", RowF32(r, 0xD0));
    SetNum(L, "PositionY", RowF32(r, 0xD4));
    SetInts(L, "RequiredIDs", NonZero(r, 0x08, 3));
    SetStr(L, "NodeType", RowStr(r, 0x1AC));
    SetInt(L, "ParentNode", I(RowU32(r, 0x19C)));
    SetNum(L, "SizeX", RowF32(r, 0xD8));
    SetNum(L, "SizeY", RowF32(r, 0xDC));
    SetNum(L, "Distance", RowF32(r, 0x1A0));
    SetStr(L, "Anchor", RowStr(r, 0x1A4));
    SetStr(L, "Color", RowStr(r, 0x1A8));

    // SpellCastReq: up to three {class, tab, points} requirements, each checked on the active build.
    lua_pushstring(L, "SpellCastReq");
    lua_createtable(L, 0, 0);
    lua_checkstack(L, 2);
    Build* active = ActiveBuild();

    for (uint32_t i = 0, n = 1; i < 3; ++i) {
        const uint32_t cls = RowU32(r, 0x1D8 + i * 4);
        const uint32_t tab = RowU32(r, 0x1E4 + i * 4);
        const uint32_t points = RowU32(r, 0x1F0 + i * 4);

        if (!cls && !tab) {
            break;
        }

        lua_pushnumber(L, static_cast<double>(n++));
        lua_createtable(L, 0, 4);
        lua_checkstack(L, 2);
        SetInt(L, "ClassType", I(cls));
        SetInt(L, "TabType", I(tab));
        SetInt(L, "Points", I(points));
        lua_pushstring(L, "IsMet");
        lua_pushboolean(L, active->MeetsPoints(cls, tab, points, 0) ? 1 : 0);
        lua_settable(L, -3);
        lua_settable(L, -3);
    }

    lua_settable(L, -3);
}

void CoA::CA::detail::PushEntryList(lua_State* L, const std::vector<Row>& rows) {
    lua_createtable(L, 0, 0);
    lua_checkstack(L, 2);

    for (size_t i = 0; i < rows.size(); ++i) {
        PushInt(L, static_cast<int32_t>(i + 1));
        PushEntry(L, rows[i]);
        lua_settable(L, -3);
    }
}

Build* CoA::CA::detail::PendingOrError(lua_State* L, const char* name) {
    Build* pending = M().pending;

    if (!pending) {
        const std::string msg = std::string(name) + ": pending build is not available";
        luaL_error(L, msg.c_str());
    }

    return pending;
}

const Request* CoA::CA::detail::InFlight(uint32_t id) {
    for (const Request& r : M().requests) {
        if (r.id == id) {
            return &r;
        }
    }

    return nullptr;
}

// Not on this realm, or (unless skipped) no player / the player's class can't use it.
bool CoA::CA::detail::HiddenBase(Row r, bool skipClass) {
    if (!Visible(r)) {
        return true;
    }

    if (skipClass) {
        return false;
    }

    const uint64_t player = ActivePlayerGuid();
    return !UnitExists(player) || !ClassTypeAdmits(r, UnitClass(player));
}

// Also hidden while the pending build lacks a 0x80000 row, or outside the mode a mode-restricted row belongs to.
bool CoA::CA::detail::Hidden(Row r, bool skipClass) {
    if (HiddenBase(r, skipClass)) {
        return true;
    }

    const uint32_t flags = RowU32(r, 0x124);
    const Build* p = M().pending;

    if ((flags >> 19) & 1) {
        if (!p || p->RankOf(RowU32(r, 0)) == 0) {
            return true;
        }
    }

    if (!(flags & 0x1800000)) {
        return false;
    }

    bool allowed = (flags & 0x800000) && p && p->wildcard;

    if ((flags & 0x1000000) && p && p->draft) {
        allowed = true;
    }

    return !allowed;
}

uint32_t CoA::CA::detail::ActiveRequiredLevel(Row r) {
    const Build* b = ActiveBuild();
    return b->draft ? RowU32(r, 0x6C) : b->wildcard ? RowU32(r, 0x70) : RowU32(r, 0x68);
}

bool CoA::CA::detail::KindSpell(Row r) {
    const int t = RowType(r);
    return t == 1 || t == 4;
}

bool CoA::CA::detail::KindTalent(Row r) {
    return RowType(r) == 2;
}

bool CoA::CA::detail::QualityByName(const std::string& name, int& q) {
    for (int i = 0; i < 9; ++i) {
        if (name == kQualities[i]) {
            q = i;
            return true;
        }
    }

    return false;
}

CoA::Dbc::Table& CoA::CA::detail::ChrSpecs() {
    return CoA::Dbc::Get("DBFilesClient\\ChrSpecs.dbc");
}

uint32_t CoA::CA::detail::SpecOf(const Build& b) {
    CoA::Dbc::Table& t = ChrSpecs();

    for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
        if (Row r = t.Row(id)) {
            if (b.Has(RowU32(r, 0x70))) {
                return RowU32(r, 0);
            }
        }
    }

    return 0;
}

bool CoA::CA::detail::UnitTokenGuid(const char* token, uint64_t& guid) {
    guid = 0;
    const Deps& d = Dependencies();
    return d.unitTokenGuid && d.unitTokenGuid(token, guid);
}

uint32_t CoA::CA::detail::RankNumber(uint32_t spell) {   // AscSpellRank::RankNumber: 1 for an unranked spell
    const Deps& d = Dependencies();
    return d.rankNumber ? d.rankNumber(spell) : 1;
}

// ---- the bindings ----------------------------------------------------------------------------------------------
namespace {
    int IsPendingBuildAvailable(lua_State* L) {
        PushBool(L, M().pending != nullptr);
        return 1;
    }

    int IsActiveBuildAvailable(lua_State* L) {
        PushBool(L, ActiveBuild() != nullptr);
        return 1;
    }

    int GetGlobalAEInvestment(lua_State* L) {
        PushInt(L, I(ActiveBuild()->GlobalAE(0)));
        return 1;
    }

    int GetGlobalTEInvestment(lua_State* L) {
        PushInt(L, I(ActiveBuild()->GlobalTE(0)));
        return 1;
    }

    int GetRemainingAE(lua_State* L) {
        PushInt(L, I(ActiveBuild()->RemainingAE()));
        return 1;
    }

    int GetRemainingTE(lua_State* L) {
        PushInt(L, I(ActiveBuild()->RemainingTE()));
        return 1;
    }

    int GetExpectedAE(lua_State* L) {
        uint32_t level;

        if (!ReadNumber(L, level)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetExpectedAE(level)");
        }

        PushInt(L, I(ActiveBuild()->AEBudget(level)));
        return 1;
    }

    int GetExpectedTE(lua_State* L) {
        uint32_t level;

        if (!ReadNumber(L, level)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetExpectedTE(level)");
        }

        PushInt(L, I(ActiveBuild()->TEBudget(level)));
        return 1;
    }

    // The per-class / per-tab sums: active build, or the pending one (nothing, no error, without a pending build).
    enum Sum { CLASS_AE, CLASS_TE, CLASS_POINTS, TAB_AE, TAB_TE };

    int SumOf(lua_State* L, Sum sum, bool pending, const char* usage) {
        uint32_t cls;
        uint32_t tab = 0;
        uint32_t minimum;
        const bool ok = sum == TAB_AE || sum == TAB_TE ? ClassTabMinArgs(L, cls, tab, minimum) : ClassMinArgs(L, cls, minimum);

        if (!ok) {
            return Usage(L, usage);
        }

        const Build* b = pending ? M().pending : ActiveBuild();

        if (!b) {
            return 0;
        }

        switch (sum) {
            case CLASS_AE: PushInt(L, I(b->ClassAE(cls, minimum))); break;
            case CLASS_TE: PushInt(L, I(b->ClassTE(cls, minimum))); break;
            case CLASS_POINTS: PushInt(L, I(b->ClassPoints(cls, minimum))); break;
            case TAB_AE: PushInt(L, I(b->TabAE(cls, tab, minimum))); break;
            case TAB_TE: PushInt(L, I(b->TabTE(cls, tab, minimum))); break;
        }

        return 1;
    }

    int GetClassAEInvestment(lua_State* L) {
        return SumOf(L, CLASS_AE, false, "Usage: C_CharacterAdvancement.GetClassAEInvestment(classType, minInvestment)");
    }

    int GetClassTEInvestment(lua_State* L) {
        return SumOf(L, CLASS_TE, false, "Usage: C_CharacterAdvancement.GetClassTEInvestment(classType, minInvestment)");
    }

    int GetClassPointInvestment(lua_State* L) {
        return SumOf(L, CLASS_POINTS, false, "Usage: C_CharacterAdvancement.GetClassPointInvestment(classType, minInvestment)");
    }

    int GetTabAEInvestment(lua_State* L) {
        return SumOf(L, TAB_AE, false, "Usage: C_CharacterAdvancement.GetTabAEInvestment(classType, tabType, minInvestment)");
    }

    int GetTabTEInvestment(lua_State* L) {
        return SumOf(L, TAB_TE, false, "Usage: C_CharacterAdvancement.GetTabTEInvestment(classType, tabType, minInvestment)");
    }

    int GetPendingClassAEInvestment(lua_State* L) {
        return SumOf(L, CLASS_AE, true, "Usage: C_CharacterAdvancement.GetPendingClassAEInvestment(classType, minInvestment)");
    }

    int GetPendingClassTEInvestment(lua_State* L) {
        return SumOf(L, CLASS_TE, true, "Usage: C_CharacterAdvancement.GetPendingClassTEInvestment(classType, minInvestment)");
    }

    int GetPendingClassPointInvestment(lua_State* L) {
        return SumOf(L, CLASS_POINTS, true, "Usage: C_CharacterAdvancement.GetPendingClassPointInvestment(classType, minInvestment)");
    }

    int GetPendingTabAEInvestment(lua_State* L) {
        return SumOf(L, TAB_AE, true, "Usage: C_CharacterAdvancement.GetPendingTabAEInvestment(classType, tabType, minInvestment)");
    }

    int GetPendingTabTEInvestment(lua_State* L) {   // FUN_10179be0
        return SumOf(L, TAB_TE, true, "Usage: C_CharacterAdvancement.GetPendingTabTEInvestment(classType, tabType, minInvestment)");
    }

    int GetActiveSpecID(lua_State* L) {
        Player* p = FindPlayer(ActivePlayerGuid());
        PushInt(L, p ? static_cast<int32_t>(p->spec + 1) : -1);
        return 1;
    }

    int IsLockedID(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.IsLockedID(entryId)");
        }

        PushBool(L, ActiveBuild()->LockedOf(id));
        return 1;
    }

    int GetEntryBySpellID(lua_State* L) {
        uint32_t spell;

        if (!ReadNumber(L, spell)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetEntryBySpellID(spellId)");
        }

        PushEntry(L, RowBySpell(spell));
        return 1;
    }

    int GetEntryByInternalID(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetEntryByInternalID(id)");
        }

        PushEntry(L, FindRow(id));
        return 1;
    }

    int GetInternalID(lua_State* L) {
        uint32_t spell;

        if (!ReadNumber(L, spell)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetInternalID(spellId)");
        }

        if (Row r = RowBySpell(spell)) {
            PushInt(L, I(RowU32(r, 0)));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // Is*SpellID: the spell's entry type; nil when no entry lists the spell. Is*ID: the same for an entry id.
    int TypeOf(lua_State* L, const char* usage, int type, bool bySpell) {
        uint32_t v;

        if (!ReadNumber(L, v)) {
            return Usage(L, usage);
        }

        if (Row r = bySpell ? RowBySpell(v) : FindRow(v)) {
            PushBool(L, RowType(r) == type);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int IsAbilitySpellID(lua_State* L) {
        return TypeOf(L, "Usage: C_CharacterAdvancement.IsAbilitySpellID(spellId)", 1, true);
    }

    int IsTalentSpellID(lua_State* L) {
        return TypeOf(L, "Usage: C_CharacterAdvancement.IsTalentSpellID(spellId)", 2, true);
    }

    int IsTalentAbilitySpellID(lua_State* L) {
        return TypeOf(L, "Usage: C_CharacterAdvancement.IsTalentAbilitySpellID(spellId)", 4, true);
    }

    int IsAbilityID(lua_State* L) {
        return TypeOf(L, "Usage: C_CharacterAdvancement.IsAbilityID(entryId)", 1, false);
    }

    int IsTalentID(lua_State* L) {
        return TypeOf(L, "Usage: C_CharacterAdvancement.IsTalentID(entryId)", 2, false);
    }

    int IsTalentAbilityID(lua_State* L) {
        return TypeOf(L, "Usage: C_CharacterAdvancement.IsTalentAbilityID(entryId)", 4, false);
    }

    int IsMastery(lua_State* L) {
        uint32_t spell;

        if (!ReadNumber(L, spell)) {
            return Usage(L, "Usage: C_CharacterAdvancement.IsMastery(spellId)");
        }

        if (Row r = RowBySpell(spell)) {
            PushBool(L, RowHasFlag(r, 0x1000));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int EssenceCost(lua_State* L, const char* usage, bool talent) {
        uint32_t spell;

        if (!ReadNumber(L, spell)) {
            return Usage(L, usage);
        }

        if (Row r = RowBySpell(spell)) {
            PushInt(L, I(talent ? ActiveBuild()->TECost(r, 1) : ActiveBuild()->AECost(r, 1)));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetTalentEssenceCost(lua_State* L) {
        return EssenceCost(L, "Usage: C_CharacterAdvancement.GetTalentEssenceCost(spellId)", true);
    }

    int GetAbilityEssenceCost(lua_State* L) {
        return EssenceCost(L, "Usage: C_CharacterAdvancement.GetAbilityEssenceCost(spellId)", false);
    }

    int GetClassName(lua_State* L) {
        uint32_t cls;

        if (!ReadNumber(L, cls)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetClassName(classType)");
        }

        if (const uint8_t* ct = ClassTypes().Row(cls)) {
            PushStr(L, ClassTypes().Str(ct, 0x18));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetTabName(lua_State* L) {
        uint32_t tab;

        if (!ReadNumber(L, tab)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetTabName(tabType)");
        }

        if (const uint8_t* tt = TabTypes().Row(tab)) {
            PushStr(L, TabTypes().Str(tt, 8));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // ---- the pending build -------------------------------------------------------------------------------------
    int PendingTotal(lua_State* L, const char* name, uint32_t (*of)(const Build&)) {
        Build* b = PendingOrError(L, name);

        if (!b) {
            return 0;
        }

        PushInt(L, I(of(*b)));
        return 1;
    }

    int GetPendingGlobalAEInvestment(lua_State* L) {
        return PendingTotal(L, "GetPendingGlobalAEInvestment", [](const Build& b) { return b.GlobalAE(0); });
    }

    int GetPendingGlobalTEInvestment(lua_State* L) {
        return PendingTotal(L, "GetPendingGlobalTEInvestment", [](const Build& b) { return b.GlobalTE(0); });
    }

    int GetPendingRemainingAE(lua_State* L) {
        return PendingTotal(L, "GetPendingRemainingAE", [](const Build& b) { return b.RemainingAE(); });
    }

    int GetPendingRemainingTE(lua_State* L) {
        return PendingTotal(L, "GetPendingRemainingTE", [](const Build& b) { return b.RemainingTE(); });
    }

    int PendingExpected(lua_State* L, const char* usage, const char* name, bool te) {
        uint32_t level;

        if (!ReadNumber(L, level)) {
            return Usage(L, usage);
        }

        Build* b = PendingOrError(L, name);

        if (!b) {
            return 0;
        }

        PushInt(L, I(te ? b->TEBudget(level) : b->AEBudget(level)));
        return 1;
    }

    int GetPendingExpectedAE(lua_State* L) {
        return PendingExpected(L, "Usage: C_CharacterAdvancement.GetPendingExpectedAE(level)", "GetPendingExpectedAE", false);
    }

    int GetPendingExpectedTE(lua_State* L) {
        return PendingExpected(L, "Usage: C_CharacterAdvancement.GetPendingExpectedTE(level)", "GetPendingExpectedTE", true);
    }

    // FUN_10179d90: the quality the active build gives the spell's row and what it costs; nil without a row.
    int GetQualityInfo(lua_State* L) {
        uint32_t spell;

        if (!ReadNumber(L, spell)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetQualityInfo(spellId)");
        }

        Row r = RowBySpell(spell);

        if (!r) {
            lua_pushnil(L);
            return 1;
        }

        PushStr(L, QualityName(static_cast<uint8_t>(QualityIndexOf(*ActiveBuild(), r))).c_str());   // FUN_101539b0
        PushInt(L, I(QualityCostOf(*ActiveBuild(), r)));                                             // FUN_101539f0
        return 2;
    }

    int IsPendingEntryID(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.IsPendingEntryID(entryId)");
        }

        Build* b = PendingOrError(L, "IsPendingEntryID");

        if (!b) {
            return 0;
        }

        if (InFlight(id)) {
            PushBool(L, false);
            return 1;
        }

        PushBool(L, b->RankOf(id) != ActiveBuild()->RankOf(id));
        return 1;
    }

    int GetPendingRankByEntryID(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetPendingRankByEntryID(entryId)");
        }

        Build* b = PendingOrError(L, "GetPendingRankByEntryID");

        if (!b) {
            return 0;
        }

        const Request* q = InFlight(id);
        PushInt(L, I(q ? q->rank : b->RankOf(id)));

        if (Row r = FindRow(id)) {
            PushInt(L, I(MaxRank(r)));
        } else {
            lua_pushnil(L);
        }

        return 2;
    }

    int GetLowestInvestmentRequired(lua_State* L) {
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TSTRING })) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetLowestInvestmentRequired(classType, tabType)");
        }

        const uint32_t cls = EnumArg(L, 1, kClassTypes, 0x2F);
        const uint32_t tab = EnumArg(L, 2, kTabTypes, 0x5F);
        Build* b = PendingOrError(L, "GetLowestInvestmentRequired");

        if (!b) {
            return 0;
        }

        uint32_t ae = 0;
        uint32_t te = 0;

        for (const Entry& e : b->entries) {
            if (RowU32(e.row, 0x80) == cls && RowU32(e.row, 0x84) == tab) {
                ae = std::max(ae, RowU32(e.row, 0x98));
                te = std::max(te, RowU32(e.row, 0x9C));
            }
        }

        PushInt(L, I(ae));
        PushInt(L, I(te));
        return 2;
    }

    // FUN_1016f480 on the pending build; nil without one or when it holds no spec's entry.
    int GetActiveChrSpec(lua_State* L) {
        if (Build* b = M().pending) {
            if (const uint32_t spec = SpecOf(*b)) {
                PushInt(L, I(spec));
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // ---- entry lists -------------------------------------------------------------------------------------------
    // FUN_101802e0: required level, then the investment requirements, then +0x1D4, then id.
    bool EntryOrder(Row a, Row b) {
        const uint32_t la = ActiveRequiredLevel(a);
        const uint32_t lb = ActiveRequiredLevel(b);

        if (la != lb) {
            return la < lb;
        }

        for (uint32_t off : { 0xA4u, 0x88u, 0x8Cu, 0x90u, 0x94u, 0x98u, 0x9Cu, 0x1D4u }) {
            if (RowU32(a, off) != RowU32(b, off)) {
                return RowU32(a, off) < RowU32(b, off);
            }
        }

        return RowU32(a, 0) < RowU32(b, 0);
    }

    // FUN_101801d0 (talents): required level, then id.
    bool TalentOrder(Row a, Row b) {
        const uint32_t la = ActiveRequiredLevel(a);
        const uint32_t lb = ActiveRequiredLevel(b);
        return la != lb ? la < lb : RowU32(a, 0) < RowU32(b, 0);
    }

    // FUN_101494b0 (tab 1 = every tab) / FUN_101495a0: realm-visible rows of the class (and tab).
    std::vector<Row> ClassRows(uint32_t cls, uint32_t tab) {
        std::vector<Row> rows;

        ForEachRow([&](Row r) {
            if (Visible(r) && RowU32(r, 0x80) == cls && (tab == 1 || RowU32(r, 0x84) == tab)) {
                rows.push_back(r);
            }
        });

        return rows;
    }

    int ListByClass(lua_State* L, bool withFlag, const char* usage, bool (*keep)(Row, bool), bool (*order)(Row, Row)) {
        const bool ok = withFlag ? ValidateInput(L, { LUA_TSTRING, LUA_TSTRING, LUA_TBOOLEAN }) : ValidateInput(L, { LUA_TSTRING, LUA_TSTRING });

        if (!ok) {
            return Usage(L, usage);
        }

        const uint32_t cls = EnumByName(CheckString(L, 1), kClassTypes, 0x2F);
        const uint32_t tab = EnumByName(CheckString(L, 2), kTabTypes, 0x5F);
        const bool withMasteries = withFlag && lua_toboolean(L, 3) != 0;
        std::vector<Row> rows = ClassRows(cls, tab);
        rows.erase(std::remove_if(rows.begin(), rows.end(), [&](Row r) { return !keep(r, withMasteries); }), rows.end());
        std::sort(rows.begin(), rows.end(), order);
        PushEntryList(L, rows);
        return 1;
    }

    bool KeepEntry(Row r, bool withMasteries) {
        return !Hidden(r, true) && (withMasteries || !RowHasFlag(r, 0x1000));
    }

    bool KeepSpell(Row r, bool withMasteries) {
        return RowType(r) == 1 && KeepEntry(r, withMasteries);
    }

    bool KeepTalent(Row r, bool withMasteries) {
        const int t = RowType(r);
        return !RowHasFlag(r, 0x400000) && (t == 2 || t == 4) && KeepEntry(r, withMasteries);
    }

    bool KeepMastery(Row r, bool) {
        return RowHasFlag(r, 0x1000) && !Hidden(r, true);
    }

    bool KeepImplicit(Row r, bool) {
        return RowHasFlag(r, 0x400000) && !Hidden(r, true);
    }

    int GetEntriesByClass(lua_State* L) {
        return ListByClass(L, true, "Usage: C_CharacterAdvancement.GetEntriesByClass(className, tabName, withMasteries)", KeepEntry, EntryOrder);
    }

    int GetSpellsByClass(lua_State* L) {
        return ListByClass(L, true, "Usage: C_CharacterAdvancement.GetSpellsByClass(className, tabName, withMasteries)", KeepSpell, EntryOrder);
    }

    int GetTalentsByClass(lua_State* L) {
        return ListByClass(L, true, "Usage: C_CharacterAdvancement.GetTalentsByClass(className, tabName, withMasteries)", KeepTalent, TalentOrder);
    }

    int GetMasteriesByClass(lua_State* L) {
        return ListByClass(L, false, "Usage: C_CharacterAdvancement.GetMasteriesByClass(className, tabName)", KeepMastery, EntryOrder);
    }

    int GetImplicitByClass(lua_State* L) {
        return ListByClass(L, false, "Usage: C_CharacterAdvancement.GetImplicitByClass(className, tabName)", KeepImplicit, EntryOrder);
    }

    // FUN_1016f5b0: every row not hidden (class check included), in id order.
    int GetAllEntries(lua_State* L) {
        std::vector<Row> rows;

        ForEachRow([&](Row r) {
            if (!Hidden(r, false)) {
                rows.push_back(r);
            }
        });

        PushEntryList(L, rows);
        return 1;
    }

    // ---- known entries and spells ------------------------------------------------------------------------------
    int IsSpellIDKnown(lua_State* L) {   // 0x1017CC90
        uint32_t spell;

        if (!ReadNumber(L, spell)) {
            return Usage(L, "Usage: C_CharacterAdvancement.IsSpellIDKnown(spellId)");
        }

        PushBool(L, KnownSpellSet().count(spell) != 0);
        return 1;
    }

    int IsKnownID(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.IsKnownID(entryId)");
        }

        if (InFlight(id)) {
            PushBool(L, false);
            return 1;
        }

        if (Row r = FindRow(id)) {
            if (RowHasFlag(r, 0x20000) && ChainKnown(RowU32(r, 0x14))) {
                PushBool(L, true);
                return 1;
            }
        }

        PushBool(L, ActiveBuild()->Has(id));
        return 1;
    }

    int IsKnownSpellID(lua_State* L) {
        uint32_t spell;

        if (!ReadNumber(L, spell)) {
            return Usage(L, "Usage: C_CharacterAdvancement.IsKnownSpellID(spellId)");
        }

        Row r = RowBySpell(spell);

        if (!r) {
            lua_pushnil(L);
            return 1;
        }

        if (InFlight(RowU32(r, 0))) {
            PushBool(L, false);
            return 1;
        }

        if (RowHasFlag(r, 0x20000) && ChainKnown(RowU32(r, 0x14))) {
            PushBool(L, true);
            return 1;
        }

        PushBool(L, RankNumber(spell) <= ActiveBuild()->RankOf(RowU32(r, 0)));
        return 1;
    }

    // FUN_10165e80: (unitToken, entryId, specialization) -> the unit's build for that spec, or why not.
    enum UnitBuildResult { UB_OK, UB_USAGE, UB_SPEC, UB_TOKEN, UB_NIL };

    UnitBuildResult UnitBuildArgs(lua_State* L, uint32_t& id, Build*& b) {
        b = nullptr;

        if (!ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER, LUA_TNUMBER })) {
            return UB_USAGE;
        }

        const std::string token = CheckString(L, 1);
        id = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        const uint32_t spec = static_cast<uint32_t>(ToInt(CheckNumber(L, 3)));

        if (spec == 0) {
            return UB_SPEC;
        }

        uint64_t guid;

        if (!UnitTokenGuid(token.c_str(), guid)) {
            return UB_TOKEN;
        }

        Player* p = FindPlayer(guid);
        b = p ? p->Get(spec - 1) : nullptr;
        return b ? UB_OK : UB_NIL;
    }

    int UnitKnownID(lua_State* L) {
        uint32_t id;
        Build* b;

        switch (UnitBuildArgs(L, id, b)) {
            case UB_USAGE: return Usage(L, "Usage: C_CharacterAdvancement.UnitKnownID(unitToken, entryId, specialization)");
            case UB_SPEC: return Usage(L, "UnitKnownID: specialization must be >= 1");
            case UB_TOKEN: return Usage(L, "UnitKnownID: invalid unit token");
            case UB_NIL: lua_pushnil(L); return 1;
            default: PushBool(L, b->Has(id)); return 1;
        }
    }

    int UnitTalentRankByID(lua_State* L) {
        uint32_t id;
        Build* b;

        switch (UnitBuildArgs(L, id, b)) {
            case UB_USAGE: return Usage(L, "Usage: C_CharacterAdvancement.UnitTalentRankByID(unitToken, entryId, specialization)");
            case UB_SPEC: return Usage(L, "UnitTalentRankByID: specialization must be >= 1");
            case UB_TOKEN: return Usage(L, "UnitTalentRankByID: invalid unit token");
            case UB_NIL: lua_pushnil(L); return 1;
            default: break;
        }

        if (const uint32_t rank = b->RankOf(id)) {
            PushInt(L, I(rank));
        } else {
            lua_pushnil(L);
        }

        if (Row r = FindRow(id)) {
            PushInt(L, I(MaxRank(r)));
        } else {
            lua_pushnil(L);
        }

        return 2;
    }

    // (rank, max rank) with a talent's max rank or 1 (FUN_10173140), a request in flight overriding the rank.
    int PushTalentRank(lua_State* L, Row r) {
        const Request* q = InFlight(RowU32(r, 0));
        PushInt(L, I(q ? q->rank : ActiveBuild()->RankOf(RowU32(r, 0))));
        PushInt(L, RowType(r) == 2 ? I(MaxRank(r)) : 1);
        return 2;
    }

    int GetTalentRankByID(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetTalentRankByID(entryId)");
        }

        if (Row r = FindRow(id)) {
            return PushTalentRank(L, r);
        }

        return PushNils(L, 2);
    }

    int GetTalentRankBySpellID(lua_State* L) {
        uint32_t spell;

        if (!ReadNumber(L, spell)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetTalentRankBySpellID(spellId)");
        }

        if (Row r = RowBySpell(spell)) {
            return PushTalentRank(L, r);
        }

        return PushNils(L, 2);
    }

    // GetLearnedAE / GetLearnedTE: (classType, tabType), (classType) or () -- names must be valid.
    int Learned(lua_State* L, bool ae) {
        const char* name = ae ? "GetLearnedAE" : "GetLearnedTE";
        Build* b = ActiveBuild();

        if (ValidateInput(L, { LUA_TSTRING, LUA_TSTRING })) {
            const uint32_t cls = EnumByName(CheckString(L, 1), kClassTypes, 0x2F);
            const uint32_t tab = EnumByName(CheckString(L, 2), kTabTypes, 0x5F);

            if (!cls || !tab) {
                return Usage(L, (std::string(name) + "(classType, tabType) expects valid class and tab names").c_str());
            }

            PushInt(L, I(ae ? b->TabAE(cls, tab, 0) : b->TabTE(cls, tab, 0)));
            return 1;
        }

        std::string cname;

        if (ReadString(L, cname)) {
            const uint32_t cls = EnumByName(cname.c_str(), kClassTypes, 0x2F);

            if (!cls) {
                return Usage(L, (std::string(name) + "(classType) expects a valid class name").c_str());
            }

            PushInt(L, I(ae ? b->ClassAE(cls, 0) : b->ClassTE(cls, 0)));
            return 1;
        }

        PushInt(L, I(ae ? b->GlobalAE(0) : b->GlobalTE(0)));
        return 1;
    }

    int GetLearnedAE(lua_State* L) {
        return Learned(L, true);
    }

    int GetLearnedTE(lua_State* L) {
        return Learned(L, false);
    }

    int KnowsConnectedNodesFor(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.KnowsConnectedNodesFor(entryId)");
        }

        Build* b = PendingOrError(L, "KnowsConnectedNodesFor");

        if (!b) {
            return 0;
        }

        if (!FindRow(id)) {
            return Usage(L, "KnowsConnectedNodesFor: invalid entryId");
        }

        PushBool(L, b->ValidateLearn(0, id, { 0xD }, {}) == 0);
        return 1;
    }

    int HasRuneUI(lua_State* L) {   // C_Spell.HasRuneUI: FUN_10171f40 (+0x410, the rune-spell count)
        PushBool(L, !RuneSpellSet().empty());
        return 1;
    }

    // GetKnown*Entries: the entries of the given kind in the pending and the active build (a set of rows).
    std::vector<Row> KnownRows(bool (*kind)(Row), int cls, int tab, bool skipDropped) {
        std::vector<Row> rows;
        auto add = [&](const Build* b) {
            if (!b) {
                return;
            }

            for (const Entry& e : b->entries) {
                if (!e.row || !kind(e.row)) {
                    continue;
                }

                if (cls >= 0 && (RowU32(e.row, 0x80) != static_cast<uint32_t>(cls) || (tab != 1 && RowU32(e.row, 0x84) != static_cast<uint32_t>(tab)))) {
                    continue;
                }

                if (skipDropped) {
                    if (const Request* q = InFlight(e.id)) {
                        if (q->rank == 0) {
                            continue;
                        }
                    }
                }

                if (std::find(rows.begin(), rows.end(), e.row) == rows.end()) {
                    rows.push_back(e.row);
                }
            }
        };
        add(M().pending);
        add(ActiveBuild());
        return rows;
    }

    // LAB_101802b0 orders only "locked before unlocked" (on the active build); the original sorts a set of row
    // pointers, so ties fall in address order -- rows lie in id order, so id order.
    std::vector<Row> SortLockedFirst(std::vector<Row> rows) {
        std::sort(rows.begin(), rows.end(), [](Row a, Row b) { return RowU32(a, 0) < RowU32(b, 0); });
        const Build* active = ActiveBuild();
        std::stable_sort(rows.begin(), rows.end(), [active](Row a, Row b) {
            return active->LockedOf(RowU32(a, 0)) && !active->LockedOf(RowU32(b, 0));
        });
        return rows;
    }

    // FUN_10180240: required level, then tab, then id.
    bool LevelTabOrder(Row a, Row b) {
        const uint32_t la = ActiveRequiredLevel(a);
        const uint32_t lb = ActiveRequiredLevel(b);

        if (la != lb) {
            return la < lb;
        }

        if (RowU32(a, 0x84) != RowU32(b, 0x84)) {
            return RowU32(a, 0x84) < RowU32(b, 0x84);
        }

        return RowU32(a, 0) < RowU32(b, 0);
    }

    int KnownEntries(lua_State* L, bool (*kind)(Row)) {
        PushEntryList(L, SortLockedFirst(KnownRows(kind, -1, -1, false)));
        return 1;
    }

    int KnownEntriesForClass(lua_State* L, bool (*kind)(Row), const char* usage) {
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TSTRING })) {
            return Usage(L, usage);
        }

        const uint32_t cls = EnumArg(L, 1, kClassTypes, 0x2F);
        const uint32_t tab = EnumArg(L, 2, kTabTypes, 0x5F);
        std::vector<Row> rows = KnownRows(kind, static_cast<int>(cls), static_cast<int>(tab), true);
        std::sort(rows.begin(), rows.end(), LevelTabOrder);
        PushEntryList(L, rows);
        return 1;
    }

    int GetKnownSpellEntries(lua_State* L) {
        return KnownEntries(L, KindSpell);
    }

    int GetKnownTalentEntries(lua_State* L) {
        return KnownEntries(L, KindTalent);
    }

    int GetKnownSpellEntriesForClass(lua_State* L) {
        return KnownEntriesForClass(L, KindSpell, "Usage: C_CharacterAdvancement.GetKnownSpellEntriesForClass(classType, tabType)");
    }

    int GetKnownTalentEntriesForClass(lua_State* L) {
        return KnownEntriesForClass(L, KindTalent, "Usage: C_CharacterAdvancement.GetKnownTalentEntriesForClass(classType, tabType)");
    }

    // ---- qualities, known spells, tags, pickup -----------------------------------------------------------------
    int GetQualityCount(lua_State* L) {   // FUN_10179c50
        std::string name;

        if (!ReadString(L, name)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetQualityCount(qualityName)");
        }

        int q;

        if (!QualityByName(name, q)) {
            return Usage(L, "GetQualityCount: invalid quality name");
        }

        if (!M().pending) {
            return Usage(L, "GetQualityCount: pending build is not available");
        }

        PushInt(L, I(QualitySumOf(*M().pending, q)));
        return 1;
    }

    // FUN_10179f50. Normal mode: the realm's CONFIG_..._QUALITIES_MAX_<Q>_ABILITIES_<expansion> (8/8/4/2 by default),
    // 99 for the other qualities. Draft: Epic 6, Legendary 3, else nil. Wildcard: nil.
    int GetQualityLimit(lua_State* L) {
        std::string name;

        if (!ReadString(L, name)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetQualityLimit(qualityName)");
        }

        int q;

        if (!QualityByName(name, q)) {
            return Usage(L, "GetQualityLimit: invalid quality name");
        }

        const uint32_t mode = CoA::GameMode::Mode();

        if ((mode >> 3) & 1) {
            if (q == 4 || q == 5) {
                PushInt(L, q == 4 ? 6 : 3);
            } else {
                lua_pushnil(L);
            }

            return 1;
        }

        if ((mode >> 6) & 1) {
            lua_pushnil(L);
            return 1;
        }

        const char* stem = q == 2 ? "UNCOMMON" : q == 3 ? "RARE" : q == 4 ? "EPIC" : q == 5 ? "LEGENDARY" : nullptr;

        if (!stem) {
            PushInt(L, 99);
            return 1;
        }

        const uint32_t ruleset = CoA::RealmInfo::Get().ruleset;   // FUN_1008e2d0 +8
        const char* exp = ruleset == 0 ? "CLASSIC" : ruleset == 1 ? "THE_BURNING_CRUSADE" : "WRATH_OF_THE_LICH_KING";
        const std::string key = std::string("CONFIG_CHARACTER_ADVANCEMENT_QUALITIES_MAX_") + stem + "_ABILITIES_" + exp;
        PushInt(L, static_cast<int32_t>(ConfigInt(key.c_str(), q == 4 ? 4 : q == 5 ? 2 : 8)));   // FUN_10196ab0
        return 1;
    }

    // FUN_101c60e0: the wildcard quality of a spell at rank index `i` -- the row's wildcard quality moved up by the
    // rank, 0 outside Poor..Heirloom.
    uint8_t QualityAtRank(Row r, uint32_t i) {
        int q = 0;
        const bool valid = QualityByName(RowStr(r, 0x60), q);
        const uint8_t v = static_cast<uint8_t>(static_cast<uint8_t>(i) - 1 + (valid ? q : 0));
        return v > 8 ? 0 : v;
    }

    // LAB_10180010: GetKnownSpells order -- by quality (per rank on a wildcard build), else by spell id.
    bool KnownSpellOrder(uint32_t a, uint32_t b) {
        Row ra = RowBySpell(a);
        Row rb = RowBySpell(b);

        if (!ra || !rb) {
            return a < b;
        }

        const Build* active = ActiveBuild();

        if (active->wildcard) {   // FUN_101556b0
            auto at = [](Row r, uint32_t spell) {
                const uint32_t n = MaxRank(r);

                for (uint32_t i = 0; i < n; ++i) {
                    if (RowU32(r, 0x14 + i * 4) == spell) {
                        return QualityAtRank(r, i);
                    }
                }

                return static_cast<uint8_t>(0);
            };
            return at(ra, a) < at(rb, b);
        }

        return static_cast<uint8_t>(QualityIndexOf(*active, ra)) < static_cast<uint8_t>(QualityIndexOf(*active, rb));
    }

    // FUN_101784b0: the active build's spells at their ranks -- abilities sorted, then talents and talent abilities.
    int GetKnownSpells(lua_State* L) {
        std::vector<uint32_t> abilities;
        std::vector<uint32_t> talents;

        for (const Entry& e : ActiveBuild()->entries) {
            Row r = FindRow(e.id);

            if (!r) {
                continue;
            }

            const int type = RowType(r);

            if (type == 1) {
                abilities.push_back(SpellAtRankOf(r, e.rank));
            } else if (type == 2 || type == 4) {
                talents.push_back(SpellAtRankOf(r, e.rank));
            }
        }

        std::sort(abilities.begin(), abilities.end(), KnownSpellOrder);
        std::sort(talents.begin(), talents.end(), KnownSpellOrder);
        abilities.insert(abilities.end(), talents.begin(), talents.end());
        PushIntArray(L, abilities);
        return 1;
    }

    // FUN_1017c640: `to` lists `from` among its connections (+0xE4, 15 ids) and the pending build has `from` (at max
    // rank when `from` carries flag 0x200000).
    int IsConnectionAllowed(lua_State* L) {
        if (!ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER })) {
            return Usage(L, "Usage: C_CharacterAdvancement.IsConnectionAllowed(fromEntryId, toEntryId)");
        }

        const uint32_t from = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        const uint32_t to = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        Build* p = M().pending;

        if (!p) {
            return Usage(L, "IsConnectionAllowed: pending build is not available");
        }

        bool ok = false;
        Row rf = FindRow(from);
        Row rt = FindRow(to);

        if (rf && rt) {
            bool listed = false;

            for (uint32_t off = 0xE4; off < 0x120 && !listed; off += 4) {
                listed = RowU32(rt, off) == from;
            }

            if (listed) {
                const uint32_t id = RowU32(rf, 0);
                ok = RowHasFlag(rf, 0x200000) ? p->RankOf(id) == MaxRank(rf) && p->Has(id) : p->Has(id);
            }
        }

        PushBool(L, ok);
        return 1;
    }

    // C_CharacterAdvancement.GetClassInfo(spellId) -- FUN_10176850: the class-type and tab-type names of the entry
    // listing the spell (FUN_101498d0); one nil when no entry lists it.
    int GetClassInfoBySpell(lua_State* L) {
        uint32_t spell;

        if (!ReadNumber(L, spell)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetClassInfo(spellId)");
        }

        Row r = RowBySpell(spell);

        if (!r) {
            lua_pushnil(L);
            return 1;
        }

        PushStr(L, ClassName(RowU32(r, 0x80)).c_str());
        PushStr(L, TabName(RowU32(r, 0x84)).c_str());
        return 2;
    }

    int CanUseBrowser(lua_State* L) {   // FUN_10175d50
        PushBool(L, ConfigBool("CONFIG_ABILITY_BROWSER_ENABLED") && !((CoA::GameMode::Mode() >> 3) & 1));
        return 1;
    }

    // SpellTagTypes.dbc: +0 id, +4 parent, +8 sort order, +0xC hidden, +0x68 / +0x6C / +0x70 strings.
    CoA::Dbc::Table& TagTypes() {
        return CoA::Dbc::Get("DBFilesClient/SpellTagTypes.dbc");
    }

    bool TagOrder(uint32_t a, uint32_t b) {   // LAB_101804b0
        const uint8_t* ra = TagTypes().Row(a);
        const uint8_t* rb = TagTypes().Row(b);
        return !ra || !rb ? a < b : RowU32(ra, 8) < RowU32(rb, 8);
    }

    std::vector<uint32_t> TagChildren(uint32_t parent) {
        std::vector<uint32_t> ids;
        CoA::Dbc::Table& t = TagTypes();

        for (uint32_t i = 0; i < t.Count(); ++i) {
            if (const uint8_t* r = t.RowAt(i)) {
                if (RowU32(r, 0xC) == 0 && RowU32(r, 4) == parent) {
                    ids.push_back(RowU32(r, 0));
                }
            }
        }

        std::sort(ids.begin(), ids.end(), TagOrder);
        return ids;
    }

    int GetRootSpellTagTypes(lua_State* L) {   // FUN_1017aa50
        PushIntArray(L, TagChildren(0));
        return 1;
    }

    int GetSpellTagTypes(lua_State* L) {   // FUN_1017ad00
        uint32_t parent;

        if (!ReadNumber(L, parent)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetSpellTagTypes(parentTypeId)");
        }

        const std::vector<uint32_t> ids = TagChildren(parent);

        if (ids.empty()) {
            lua_pushnil(L);
        } else {
            PushIntArray(L, ids);
        }

        return 1;
    }

    int GetSpellTagTypeDisplayInfo(lua_State* L) {   // FUN_1017ab50
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetSpellTagTypeDisplayInfo(typeId)");
        }

        CoA::Dbc::Table& t = TagTypes();
        const uint8_t* r = t.Row(id);

        if (!r || RowU32(r, 0xC) != 0) {
            return PushNils(L, 3);
        }

        lua_pushstring(L, t.Str(r, 0x6C));
        const char* icon = t.Str(r, 0x68);

        if (*icon) {
            lua_pushstring(L, icon);
        } else {
            lua_pushnil(L);
        }

        lua_pushstring(L, t.Str(r, 0x70));
        return 3;
    }

    // FUN_1008e4f0(spell, 0) on the player: the highest rank of the spell's chain the player knows (the client
    // spellbook 0x7260E0, then the CA known set), else the spell itself if known, else 0.
    uint32_t KnownRankOf(uint32_t spell) {
        const Deps& d = Dependencies();
        auto knows = [&](uint32_t s) { return (d.clientKnowsSpell && d.clientKnowsSpell(s)) || KnownSpellSet().count(s) != 0; };
        const std::vector<uint32_t>& chain = Chain(spell);

        if (chain.empty()) {
            return knows(spell) ? spell : 0;
        }

        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            if (knows(*it)) {
                return *it;
            }
        }

        return 0;
    }

    int PickupSpell(lua_State* L) {   // handler_PickupSpell
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.PickupSpell(entryId)");
        }

        Row r = FindRow(id);

        if (!r) {
            return Usage(L, "PickupSpell: invalid entryId");
        }

        uint32_t rank = ActiveBuild()->RankOf(RowU32(r, 0));

        if (!rank) {
            if (!RowHasFlag(r, 0x20000)) {
                return 0;
            }

            rank = 1;
        }

        if (!UnitExists(ActivePlayerGuid())) {
            return Usage(L, "PickupSpell: no active player");
        }

        const uint32_t spell = SpellAtRankOf(r, rank);

        if (!spell) {
            return Usage(L, "PickupSpell: entryId has no spell at current rank");
        }

        if (const uint32_t known = KnownRankOf(spell)) {
            const Deps& d = Dependencies();

            // The spellbook (count 0xBE8D98, ids 0xBE6D88); 0x53BAF0(slot, 0) picks the slot up.
            if (!d.pickupFromSpellbook || !d.pickupFromSpellbook(known)) {
                return Usage(L, "PickupSpell: spell not found in spellbook");
            }
        }

        return 0;
    }

    // ---- categories (CharacterAdvancementCategories.dbc: +4 order, +8 required level, +0xC flag, +0x10 / +0x14 /
    // +0x18 strings) ---------------------------------------------------------------------------------------------
    CoA::Dbc::Table& Categories() {
        return CoA::Dbc::Get("DBFilesClient/CharacterAdvancementCategories.dbc");
    }

    // FUN_10176450: every category id, by order (LAB_10180110) or by required level then order (LAB_10180170) when the
    // optional argument is true.
    int GetCategories(lua_State* L) {
        const int t = lua_type(L, 1);

        if (t > 0 && t != LUA_TBOOLEAN) {   // FUN_10309e50: nothing, nil or a boolean
            return Usage(L, "Usage: C_CharacterAdvancement.GetCategories([sortByRequiredLevel])");
        }

        const bool byLevel = t > 0 && lua_toboolean(L, 1) != 0;
        CoA::Dbc::Table& c = Categories();
        std::vector<uint32_t> ids;

        for (uint32_t id = c.MinId(); c.Loaded() && id <= c.MaxId(); ++id) {
            if (c.Row(id)) {
                ids.push_back(id);
            }
        }

        std::sort(ids.begin(), ids.end(), [&](uint32_t a, uint32_t b) {
            const uint8_t* ra = c.Row(a);
            const uint8_t* rb = c.Row(b);

            if (byLevel && RowU32(ra, 8) != RowU32(rb, 8)) {
                return RowU32(ra, 8) < RowU32(rb, 8);
            }

            return RowU32(ra, 4) < RowU32(rb, 4);
        });
        PushIntArray(L, ids);
        return 1;
    }

    // handler_GetCategoryDisplayInfo: required level, +0x14, +0x10, +0x18, flag; five nils if unknown.
    int GetCategoryDisplayInfo(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return Usage(L, "Usage: C_CharacterAdvancement.GetCategoryDisplayInfo(categoryId)");
        }

        CoA::Dbc::Table& c = Categories();
        const uint8_t* r = c.Row(id);

        if (!r) {
            return PushNils(L, 5);
        }

        PushInt(L, I(RowU32(r, 8)));
        lua_pushstring(L, c.Str(r, 0x14));
        lua_pushstring(L, c.Str(r, 0x10));
        lua_pushstring(L, c.Str(r, 0x18));
        PushBool(L, RowU32(r, 0xC) != 0);
        return 5;
    }

    int CA_GetCreditAmount(lua_State* L) {
        if (!ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        PushInt(L, I(Credit(static_cast<uint8_t>(ToInt(CheckNumber(L, 1))))));
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_CharacterAdvancement", "IsPendingBuildAvailable", IsPendingBuildAvailable),
        COA_NATIVE("C_CharacterAdvancement", "IsActiveBuildAvailable", IsActiveBuildAvailable),
        COA_NATIVE("C_CharacterAdvancement", "GetGlobalAEInvestment", GetGlobalAEInvestment),
        COA_NATIVE("C_CharacterAdvancement", "GetGlobalTEInvestment", GetGlobalTEInvestment),
        COA_NATIVE("C_CharacterAdvancement", "GetRemainingAE", GetRemainingAE),
        COA_NATIVE("C_CharacterAdvancement", "GetRemainingTE", GetRemainingTE),
        COA_NATIVE("C_CharacterAdvancement", "GetExpectedAE", GetExpectedAE),
        COA_NATIVE("C_CharacterAdvancement", "GetExpectedTE", GetExpectedTE),
        COA_NATIVE("C_CharacterAdvancement", "GetClassAEInvestment", GetClassAEInvestment),
        COA_NATIVE("C_CharacterAdvancement", "GetClassTEInvestment", GetClassTEInvestment),
        COA_NATIVE("C_CharacterAdvancement", "GetClassPointInvestment", GetClassPointInvestment),
        COA_NATIVE("C_CharacterAdvancement", "GetTabAEInvestment", GetTabAEInvestment),
        COA_NATIVE("C_CharacterAdvancement", "GetTabTEInvestment", GetTabTEInvestment),
        COA_NATIVE("C_CharacterAdvancement", "GetActiveSpecID", GetActiveSpecID),
        COA_NATIVE("C_CharacterAdvancement", "IsLockedID", IsLockedID),
        COA_NATIVE("C_CharacterAdvancement", "GetEntryBySpellID", GetEntryBySpellID),
        COA_NATIVE("C_CharacterAdvancement", "GetEntryByInternalID", GetEntryByInternalID),
        COA_NATIVE("C_CharacterAdvancement", "GetInternalID", GetInternalID),
        COA_NATIVE("C_CharacterAdvancement", "IsAbilitySpellID", IsAbilitySpellID),
        COA_NATIVE("C_CharacterAdvancement", "IsTalentSpellID", IsTalentSpellID),
        COA_NATIVE("C_CharacterAdvancement", "IsTalentAbilitySpellID", IsTalentAbilitySpellID),
        COA_NATIVE("C_CharacterAdvancement", "IsAbilityID", IsAbilityID),
        COA_NATIVE("C_CharacterAdvancement", "IsTalentID", IsTalentID),
        COA_NATIVE("C_CharacterAdvancement", "IsTalentAbilityID", IsTalentAbilityID),
        COA_NATIVE("C_CharacterAdvancement", "IsMastery", IsMastery),
        COA_NATIVE("C_CharacterAdvancement", "GetTalentEssenceCost", GetTalentEssenceCost),
        COA_NATIVE("C_CharacterAdvancement", "GetAbilityEssenceCost", GetAbilityEssenceCost),
        COA_NATIVE("C_CharacterAdvancement", "GetClassName", GetClassName),
        COA_NATIVE("C_CharacterAdvancement", "GetTabName", GetTabName),
        COA_NATIVE("C_CharacterAdvancement", "GetPendingGlobalAEInvestment", GetPendingGlobalAEInvestment),
        COA_NATIVE("C_CharacterAdvancement", "GetPendingGlobalTEInvestment", GetPendingGlobalTEInvestment),
        COA_NATIVE("C_CharacterAdvancement", "GetPendingRemainingAE", GetPendingRemainingAE),
        COA_NATIVE("C_CharacterAdvancement", "GetPendingRemainingTE", GetPendingRemainingTE),
        COA_NATIVE("C_CharacterAdvancement", "GetPendingExpectedAE", GetPendingExpectedAE),
        COA_NATIVE("C_CharacterAdvancement", "GetPendingExpectedTE", GetPendingExpectedTE),
        COA_NATIVE("C_CharacterAdvancement", "GetPendingClassAEInvestment", GetPendingClassAEInvestment),
        COA_NATIVE("C_CharacterAdvancement", "GetPendingClassTEInvestment", GetPendingClassTEInvestment),
        COA_NATIVE("C_CharacterAdvancement", "GetPendingClassPointInvestment", GetPendingClassPointInvestment),
        COA_NATIVE("C_CharacterAdvancement", "GetPendingTabAEInvestment", GetPendingTabAEInvestment),
        COA_NATIVE("C_CharacterAdvancement", "IsPendingEntryID", IsPendingEntryID),
        COA_NATIVE("C_CharacterAdvancement", "GetPendingRankByEntryID", GetPendingRankByEntryID),
        COA_NATIVE("C_CharacterAdvancement", "GetLowestInvestmentRequired", GetLowestInvestmentRequired),
        COA_NATIVE("C_CharacterAdvancement", "GetActiveChrSpec", GetActiveChrSpec),
        COA_NATIVE("C_CharacterAdvancement", "GetEntriesByClass", GetEntriesByClass),
        COA_NATIVE("C_CharacterAdvancement", "GetSpellsByClass", GetSpellsByClass),
        COA_NATIVE("C_CharacterAdvancement", "GetTalentsByClass", GetTalentsByClass),
        COA_NATIVE("C_CharacterAdvancement", "GetMasteriesByClass", GetMasteriesByClass),
        COA_NATIVE("C_CharacterAdvancement", "GetImplicitByClass", GetImplicitByClass),
        COA_NATIVE("C_CharacterAdvancement", "GetAllEntries", GetAllEntries),
        // A bare global in the original (FUN_10183e30 registers it after the table), despite its usage text, and as
        // CA_IsSpellKnown (same handler 0x1017CC90).
        COA_NATIVE("", "IsSpellIDKnown", IsSpellIDKnown),
        COA_NATIVE("", "CA_IsSpellKnown", IsSpellIDKnown),
        COA_NATIVE("C_CharacterAdvancement", "IsKnownID", IsKnownID),
        COA_NATIVE("C_CharacterAdvancement", "IsKnownSpellID", IsKnownSpellID),
        COA_NATIVE("C_CharacterAdvancement", "UnitKnownID", UnitKnownID),
        COA_NATIVE("C_CharacterAdvancement", "UnitTalentRankByID", UnitTalentRankByID),
        COA_NATIVE("C_CharacterAdvancement", "GetTalentRankByID", GetTalentRankByID),
        COA_NATIVE("C_CharacterAdvancement", "GetTalentRankBySpellID", GetTalentRankBySpellID),
        COA_NATIVE("C_CharacterAdvancement", "GetLearnedAE", GetLearnedAE),
        COA_NATIVE("C_CharacterAdvancement", "GetLearnedTE", GetLearnedTE),
        COA_NATIVE("C_CharacterAdvancement", "KnowsConnectedNodesFor", KnowsConnectedNodesFor),
        COA_NATIVE("C_CharacterAdvancement", "GetKnownSpellEntries", GetKnownSpellEntries),
        COA_NATIVE("C_CharacterAdvancement", "GetKnownTalentEntries", GetKnownTalentEntries),
        COA_NATIVE("C_CharacterAdvancement", "GetKnownSpellEntriesForClass", GetKnownSpellEntriesForClass),
        COA_NATIVE("C_CharacterAdvancement", "GetKnownTalentEntriesForClass", GetKnownTalentEntriesForClass),
        COA_NATIVE("C_Spell", "HasRuneUI", HasRuneUI),
        COA_NATIVE("C_CharacterAdvancement", "GetQualityCount", GetQualityCount),
        COA_NATIVE("C_CharacterAdvancement", "GetQualityLimit", GetQualityLimit),
        COA_NATIVE("C_CharacterAdvancement", "GetKnownSpells", GetKnownSpells),
        COA_NATIVE("C_CharacterAdvancement", "IsConnectionAllowed", IsConnectionAllowed),
        COA_NATIVE("C_CharacterAdvancement", "GetClassInfo", GetClassInfoBySpell),
        COA_NATIVE("C_CharacterAdvancement", "CanUseBrowser", CanUseBrowser),
        COA_NATIVE("C_CharacterAdvancement", "GetRootSpellTagTypes", GetRootSpellTagTypes),
        COA_NATIVE("C_CharacterAdvancement", "GetSpellTagTypes", GetSpellTagTypes),
        COA_NATIVE("C_CharacterAdvancement", "GetSpellTagTypeDisplayInfo", GetSpellTagTypeDisplayInfo),
        COA_NATIVE("C_CharacterAdvancement", "PickupSpell", PickupSpell),
        COA_NATIVE("C_CharacterAdvancement", "GetCategories", GetCategories),
        COA_NATIVE("C_CharacterAdvancement", "GetCategoryDisplayInfo", GetCategoryDisplayInfo),
        COA_NATIVE("C_CharacterAdvancement", "GetQualityInfo", GetQualityInfo),
        COA_NATIVE("C_CharacterAdvancement", "GetPendingTabTEInvestment", GetPendingTabTEInvestment),
        COA_NATIVE("", "CA_GetCreditAmount", CA_GetCreditAmount),
    };
}

void CoA::CA::detail::InitNatives() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
