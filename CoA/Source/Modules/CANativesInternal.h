#ifndef COA_SOURCE_MODULES_CANATIVESINTERNAL_H
#define COA_SOURCE_MODULES_CANATIVESINTERNAL_H

// What the CA native files share (CANatives.cpp, CAEdit.cpp, CASuggest.cpp, CALinks.cpp): the argument readers, the
// entry table and the row predicates the bindings use (the reconstruction's AscCAMgr.cpp lines 1395..1546 and the
// helpers next to the bindings).
#include "CoA/Source/Modules/CAInternal.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include <string>
#include <vector>

struct lua_State;

namespace CoA::CA::detail {
    int Usage(lua_State* L, const char* msg);   // luaL_error

    // FUN_1016b250 / FUN_1016b650: a class / tab type given by name (the enum value is index + 1, 0 logged unknown).
    uint32_t EnumArg(lua_State* L, int idx, const char* const* names, uint32_t count);
    // FUN_10168b90 / FUN_10168e40: the same from a C string ("Unexpected Value: %s" when unknown).
    uint32_t EnumByName(const char* name, const char* const* names, uint32_t count);
    bool ClassMinArgs(lua_State* L, uint32_t& cls, uint32_t& minimum);                    // FUN_10166520
    bool ClassTabMinArgs(lua_State* L, uint32_t& cls, uint32_t& tab, uint32_t& minimum);  // FUN_10166680

    std::string ClassName(uint32_t cls);   // ClassTypes +4, else INVALID_CLASS_TYPE_<n>
    std::string TabName(uint32_t tab);     // TabTypes +4, else INVALID_TAB_TYPE_<n>

    void PushEntry(lua_State* L, Row r);                             // FUN_10172cf0 (nil without a row)
    void PushEntryList(lua_State* L, const std::vector<Row>& rows);  // FUN_10172df0
    void PushIntArray(lua_State* L, const std::vector<uint32_t>& v); // FUN_1009a460

    // The pending build or a Lua error "<name>: pending build is not available".
    Build* PendingOrError(lua_State* L, const char* name);
    const Request* InFlight(uint32_t id);   // FUN_10a31720: a learn / unlearn request in flight

    bool HiddenBase(Row r, bool skipClass);   // FUN_101c67c0
    bool Hidden(Row r, bool skipClass);       // FUN_101c6bd0
    uint32_t ActiveRequiredLevel(Row r);      // FUN_10153af0 on the active build
    bool KindSpell(Row r);                    // FUN_101c6770: Ability or TalentAbility
    bool KindTalent(Row r);                   // FUN_101c6e80: Talent
    bool QualityByName(const std::string& name, int& q);   // FUN_10168cc0

    CoA::Dbc::Table& ChrSpecs();     // DBFilesClient\ChrSpecs.dbc
    uint32_t SpecOf(const Build& b);   // FUN_1016f480: the first ChrSpecs row whose entry (+0x70) the build has

    bool UnitTokenGuid(const char* token, uint64_t& guid);
    uint32_t RankNumber(uint32_t spell);

    // Each native file's binding table, added from CoA::CA::Init.
    void InitNatives();   // CANatives.cpp
}

#endif
