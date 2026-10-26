#include "CoA/Source/Modules/GlobalsJ.h"
#include "CoA/Source/Modules/AttachFixes.h"
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <share.h>
#include <unordered_map>
#include <vector>

namespace {
    // ---- script error handler -------------------------------------------------------------------------------
    int32_t s_errorCallstackHeight = 0;   // 0x10BE3088

    int GetErrorCallstackHeight(lua_State* L) {
        CoA::Script::PushInt(L, s_errorCallstackHeight);
        return 1;
    }

    // ---- CanPerformAction -----------------------------------------------------------------------------------
    // Action 0x19 answers whether Script_RunScript is running; every other action is the client's own gate.
    int32_t s_insideProtectedCall = 0;

    int CanPerformAction(lua_State* L) {
        int32_t action = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1));

        if (action == 0x19) {
            CoA::Script::PushBool(L, s_insideProtectedCall != 0);
            return 1;
        }

        CoA::Script::PushBool(L, CoA::ClientState::CanPerformAction(action));
        return 1;
    }

    // ---- known / secure addons ------------------------------------------------------------------------------
    struct KnownAddon {
        std::string name;
        bool secure;
    };

    std::vector<KnownAddon> s_knownAddons;

    // GetKnownAddons / GetSecureAddons: every (secure) name that does not contain "Blizzard", pushed as separate
    // return values.
    int PushAddonNames(lua_State* L, bool secureOnly) {
        int n = 0;

        for (const KnownAddon& addon : s_knownAddons) {
            if (secureOnly && !addon.secure) {
                continue;
            }

            if (addon.name.size() >= 8 && addon.name.find("Blizzard") != std::string::npos) {
                continue;
            }

            CoA::Script::PushStr(L, addon.name.c_str());
            ++n;
        }

        return n;
    }

    int GetKnownAddons(lua_State* L) {
        return PushAddonNames(L, false);
    }

    int GetSecureAddons(lua_State* L) {
        return PushAddonNames(L, true);
    }

    // An exact name match, Blizzard ones included.
    int IsKnownAddon(lua_State* L) {
        std::string name = CoA::Script::CheckString(L, 1);
        CoA::Script::PushBool(L, CoA::GlobalsJ::IsKnownAddon(name.c_str()));
        return 1;
    }

    // ---- custom WTF files and saved CVars -------------------------------------------------------------------
    // ReadCustomWTF(name) / WriteCustomWTF(name, text) (FUN_10259d10 / FUN_1025a1d0): text mode. Both need the
    // client's action gate 0x11; without it they return 1 value having pushed nothing (the caller gets the top of
    // the stack). Read: "" when the directory is missing or the file cannot be read. Write: false for text over
    // 0x6400000 bytes or when the directory cannot be created, else whether the stream stayed good.
    int ReadCustomWTF(lua_State* L) {
        if (!CoA::ClientState::CanPerformAction(0x11)) {
            return 1;
        }

        std::string name = CoA::Script::CheckString(L, 1);
        std::string text;

        if (GetFileAttributesA("WTF\\Custom") != INVALID_FILE_ATTRIBUTES) {
            if (FILE* f = _fsopen(CoA::GlobalsJ::CustomWtfPath(name).c_str(), "r", _SH_DENYNO)) {
                char buffer[4096];
                size_t n;

                while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                    text.append(buffer, n);
                }

                fclose(f);
            }
        }

        CoA::Script::PushStr(L, text.c_str());
        return 1;
    }

    int WriteCustomWTF(lua_State* L) {
        if (!CoA::ClientState::CanPerformAction(0x11)) {
            return 1;
        }

        std::string name = CoA::Script::CheckString(L, 1);
        std::string text = CoA::Script::CheckString(L, 2);

        if (text.size() > 0x6400000) {
            CoA::Script::PushBool(L, false);
            return 1;
        }

        if (GetFileAttributesA("WTF\\Custom") == INVALID_FILE_ATTRIBUTES && !CreateDirectoryA("WTF\\Custom", nullptr)) {
            CoA::Script::PushBool(L, false);
            return 1;
        }

        bool good = false;

        if (FILE* f = _fsopen(CoA::GlobalsJ::CustomWtfPath(name).c_str(), "w", _SH_DENYNO)) {
            good = fwrite(text.data(), 1, text.size(), f) == text.size();
            good = fclose(f) == 0 && good;
        }

        CoA::Script::PushBool(L, good);
        return 1;
    }

    // RegisterSavedCVar(name, default [, perCharacter]) (FUN_10312910): refused (false) while an addon runs
    // (lua_tainted) that is not a known secure one; otherwise CVar::Register with no help text, flags 0x80000001
    // (0x80000021 when a third argument is true) -> true. The original passes category 5 to FUN_10114510, which
    // zeroes its trailing arguments before jumping to CVar::Register, so the client registers category 0.
    int RegisterSavedCVar(lua_State* L) {
        if (lua_tainted) {
            bool blocked = true;

            for (const KnownAddon& addon : s_knownAddons) {
                if (addon.name == lua_tainted) {
                    blocked = !addon.secure;
                    break;
                }
            }

            if (blocked) {
                CoA::Script::PushBool(L, false);
                return 1;
            }
        }

        std::string name = CoA::Script::CheckString(L, 1);
        std::string value = CoA::Script::CheckString(L, 2);
        uint32_t flags = 0x80000001;

        if (lua_gettop(L) == 3) {
            flags = lua_toboolean(L, 3) ? 0x80000021 : 0x80000001;
        }

        CVar::Register(name.c_str(), "", flags, value.c_str(), nullptr, 0, false, nullptr, false);
        CoA::Script::PushBool(L, true);
        return 1;
    }

    // ---- character select ---------------------------------------------------------------------------------
    // Maps 0x10BDB5FC / 0x10BDB61C, keyed by the character's list index; never cleared.
    struct SelectionMail {
        bool mail = false;
        bool storeMail = false;
    };

    struct SelectionGameMode {
        uint32_t a = 0;
        uint32_t b = 0;
        uint32_t c = 0;
        bool d = false;
        uint8_t e = 0;
    };

    std::unordered_map<uint32_t, SelectionMail> s_selectionMail;
    std::unordered_map<uint32_t, SelectionGameMode> s_selectionGameMode;

    int32_t OnSelectionMail(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {   // handler_0x0770
        uint32_t index = CoA::Read::U32(msg);
        SelectionMail& m = s_selectionMail[index];
        m.mail = CoA::Read::U8(msg) != 0;
        m.storeMail = CoA::Read::U8(msg) != 0;
        return 1;
    }

    int32_t OnSelectionGameMode(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {   // FUN_100d3910
        uint32_t index = CoA::Read::U32(msg);
        SelectionGameMode& g = s_selectionGameMode[index];
        g.a = CoA::Read::U32(msg);
        g.b = CoA::Read::U32(msg);
        g.c = CoA::Read::U32(msg);
        g.d = CoA::Read::U8(msg) != 0;
        g.e = CoA::Read::U8(msg);
        return 1;
    }

    int GetCharacterSelectionMailData(lua_State* L) {
        uint32_t index;

        if (!CoA::Script::ReadNumber(L, index)) {
            return 0;
        }

        auto it = s_selectionMail.find(index);

        if (it == s_selectionMail.end()) {
            lua_pushnil(L);
            return 1;
        }

        CoA::Script::PushBool(L, it->second.mail);
        CoA::Script::PushBool(L, it->second.storeMail);
        return 2;
    }

    int GetCharacterSelectionGameModeData(lua_State* L) {
        uint32_t index;

        if (!CoA::Script::ReadNumber(L, index)) {
            return 0;
        }

        auto it = s_selectionGameMode.find(index);

        if (it == s_selectionGameMode.end()) {
            lua_pushnil(L);
            return 1;
        }

        const SelectionGameMode& g = it->second;
        CoA::Script::PushInt(L, static_cast<int32_t>(g.a));
        CoA::Script::PushInt(L, static_cast<int32_t>(g.b));
        CoA::Script::PushInt(L, static_cast<int32_t>(g.c));
        CoA::Script::PushBool(L, g.d);
        CoA::Script::PushInt(L, g.e);
        return 5;
    }

    // FUN_100dd280(order): CMSG 0x772 {string order, NUL included}.
    int SetCharacterSelectionSortOrder(lua_State* L) {
        std::string order;

        if (CoA::Script::ReadString(L, order)) {
            CoA::Packet(0x772).Data(order.c_str(), static_cast<uint32_t>(order.size() + 1)).Send();
        }

        return 0;
    }

    // ---- combat log filter ------------------------------------------------------------------------------------------
    bool s_combatLogDisableAll = false;          // 0x10D3D749
    std::vector<uint32_t> s_combatLogDisabled;   // 0x10D3D74C (nothing ever clears either)

    int CombatLogDisableAll(lua_State*) {
        s_combatLogDisableAll = true;
        return 0;
    }

    int CombatLogDisable(lua_State* L) {
        s_combatLogDisabled.push_back(static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1))));
        return 0;
    }

    // ---- attack animation / distance, game-object fading ---------------------------------------------------------
    int32_t s_attackAnimation = 0;   // 0x10D3D810
    float s_attackDistance = 0.0f;   // 0x10D3D7EC
    uint8_t s_fadeOutGameObjects = 1;   // 0x10BCC8D9

    int SetAttackAnimation(lua_State* L) {
        s_attackAnimation = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1));
        return 0;
    }

    int SetAttackDistance(lua_State* L) {
        s_attackDistance = static_cast<float>(CoA::Script::CheckNumber(L, 1));
        return 0;
    }

    // Tests the argument's TYPE, not its value: nil, a boolean (either) or a number turn fading on.
    int SetFadeOutGameObjects(lua_State* L) {
        const int type = lua_type(L, 1);
        s_fadeOutGameObjects = (type == LUA_TNIL || type == LUA_TBOOLEAN || type == LUA_TNUMBER) ? 1 : 0;
        return 0;
    }

    // ---- GM transactions (TransactionSystemMgr, FUN_10a13e80 -> 0x10D3C570) -----------------------------------------
    // Record (FUN_10a13fa0): u32 id (the key), u32, sized string, u32, sized string, u32, u32 kept as a byte, four u32.
    struct Transaction {
        uint32_t id = 0;
        uint32_t b = 0;
        std::string s1;
        uint32_t c = 0;
        std::string s2;
        uint32_t d = 0;
        uint8_t e = 0;
        uint32_t f[4] = { 0, 0, 0, 0 };
    };

    std::unordered_map<uint32_t, Transaction> s_transactions;
    std::vector<uint32_t> s_transactionOrder;   // the map's iteration order (insertion)

    std::string ReadSized(CDataStore* msg) {   // FUN_100d3680: u32 length, then the bytes
        const uint32_t n = CoA::Read::U32(msg);
        std::string text;

        for (uint32_t i = 0; i < n && !CoA::Read::Failed(msg); ++i) {
            text.push_back(static_cast<char>(CoA::Read::U8(msg)));
        }

        return text;
    }

    uint32_t ParseTransaction(CDataStore* msg) {
        Transaction t;
        t.id = CoA::Read::U32(msg);
        t.b = CoA::Read::U32(msg);
        t.s1 = ReadSized(msg);
        t.c = CoA::Read::U32(msg);
        t.s2 = ReadSized(msg);
        t.d = CoA::Read::U32(msg);
        t.e = static_cast<uint8_t>(CoA::Read::U32(msg));

        for (uint32_t& v : t.f) {
            v = CoA::Read::U32(msg);
        }

        if (!s_transactions.count(t.id)) {
            s_transactionOrder.push_back(t.id);
        }

        s_transactions[t.id] = t;
        return t.id;
    }

    int32_t OnTransactionCreated(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {   // handler_0x09b7
        const uint32_t id = ParseTransaction(msg);
        CoA::Events::Signal("ASCENSION_CUSTOM_GM_TRANSACTION_CREATED", "%u", id);
        CoA::Events::Signal("ASCENSION_CUSTOM_GM_TRANSACTION_LIST_UPDATED", "%b", 0);
        return 1;
    }

    // The original fires "GM_TRANSACTION_REMOVED", which no table registers, so nothing is signalled; the registered
    // ASCENSION_CUSTOM_GM_TRANSACTION_REMOVED is never fired. Kept.
    int32_t OnTransactionClosed(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {   // FUN_10a142e0
        const uint32_t id = static_cast<uint32_t>(CoA::Read::U64(msg));

        if (!s_transactions.count(id)) {
            return 1;
        }

        s_transactions.erase(id);
        s_transactionOrder.erase(std::remove(s_transactionOrder.begin(), s_transactionOrder.end(), id), s_transactionOrder.end());
        CoA::Events::Signal("GM_TRANSACTION_REMOVED", "%u", id);
        return 1;
    }

    int32_t OnTransactionList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {   // FUN_10a14590
        s_transactions.clear();
        s_transactionOrder.clear();

        for (uint32_t n = CoA::Read::U32(msg); n && !CoA::Read::Failed(msg); --n) {
            ParseTransaction(msg);
        }

        CoA::Events::Signal("ASCENSION_CUSTOM_GM_TRANSACTION_LIST_UPDATED", "%b", 1);
        return 1;
    }

    int GM_GetTransactionIdList(lua_State* L) {
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        int32_t i = 0;

        for (uint32_t id : s_transactionOrder) {
            CoA::Script::PushNum(L, static_cast<double>(++i));
            CoA::Script::PushInt(L, static_cast<int32_t>(id));
            lua_settable(L, -3);
        }

        return 1;
    }

    int GM_GetTransactionContent(lua_State* L) {
        const uint32_t id = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));
        auto it = s_transactions.find(id);

        if (it == s_transactions.end()) {
            return 0;
        }

        const Transaction& t = it->second;
        CoA::Script::PushInt(L, static_cast<int32_t>(t.id));
        CoA::Script::PushInt(L, static_cast<int32_t>(t.b));
        CoA::Script::PushStr(L, t.s1.c_str());
        CoA::Script::PushInt(L, static_cast<int32_t>(t.c));
        CoA::Script::PushStr(L, t.s2.c_str());
        CoA::Script::PushInt(L, static_cast<int32_t>(t.d));
        CoA::Script::PushInt(L, t.e);

        for (uint32_t v : t.f) {
            CoA::Script::PushInt(L, static_cast<int32_t>(v));
        }

        return 0xB;
    }

    // ---- asset queries, battlefield list, feedback ---------------------------------------------------------------------
    // RequestSingleAssetQuery(type, id) (FUN_100dcee0): CMSG 0x775 {u32 type, u32 id}.
    int RequestSingleAssetQuery(lua_State* L) {
        int32_t type = 0;
        int32_t id = 0;

        if (CoA::Script::ReadInt2(L, type, id)) {
            CoA::Packet(0x775).U32(static_cast<uint32_t>(type)).U32(static_cast<uint32_t>(id)).Send();
        }

        return 0;
    }

    // RequestMultipleAssetQuery(type, id, ...) (FUN_100dcbd0): CMSG 0x776 {u32 type, u32 count, u32 id * count}.
    int RequestMultipleAssetQuery(lua_State* L) {
        const int top = lua_gettop(L);

        if (top < 2) {
            return 0;
        }

        CoA::Packet packet(0x776);
        packet.U32(static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)))).U32(static_cast<uint32_t>(top - 1));

        for (int i = 2; i <= top; ++i) {
            packet.U32(static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, i))));
        }

        packet.Send();
        return 0;
    }

    // FUN_100d8c00(bgType [, fromWhere [, unk]]): CMSG_BATTLEFIELD_LIST (0x23C) {u32, u8, u8}.
    int GetBattlefieldList(lua_State* L) {
        const uint32_t bg = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));
        const uint8_t from = lua_type(L, 2) > 0 ? static_cast<uint8_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2))) : 0;
        const uint8_t unk = lua_type(L, 3) > 0 ? static_cast<uint8_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 3))) : 0;
        CoA::Packet(0x23C).U32(bg).U8(from).U8(unk).Send();
        return 0;
    }

    // FUN_100dd3c0(n [, text]): CMSG 0x735 {u32 n, u8 hasText, [text + NUL]}, then true; an empty text counts as none.
    int SubmitGameFeedback(lua_State* L) {
        const bool hasText = lua_type(L, 2) > 0;

        if (hasText ? !CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TSTRING }) : !CoA::Script::ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        const uint32_t n = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));
        const std::string text = hasText ? CoA::Script::CheckString(L, 2) : std::string();
        CoA::Packet packet(0x735);
        packet.U32(n).U8(text.empty() ? 0 : 1);

        if (!text.empty()) {
            packet.Data(text.c_str(), static_cast<uint32_t>(text.size() + 1));
        }

        packet.Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    // ---- AscensionPvPMgr (0x10BDBAD4): arena reward info ------------------------------------------------------------------
    // SMSG 0x925 (FUN_100e04a0) fills seven u32s in wire order +4, +8, +0xC, +0x1C, +0x14, +0x18, +0x10 and signals
    // ASCENSION_CUSTOM_PVP_REWARD_INFO_AVAILABLE; CMSG 0x53E asks for it.
    struct PvPRewardInfo {
        uint32_t f04 = 0;
        uint32_t f08 = 0;
        uint32_t bracket = 0;
        uint32_t f10 = 0;
        uint32_t f14 = 0;
        uint32_t f18 = 0;
        uint32_t f1C = 0;
    } s_pvpReward;

    int32_t OnPvPRewardInfo(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        s_pvpReward.f04 = CoA::Read::U32(msg);
        s_pvpReward.f08 = CoA::Read::U32(msg);
        s_pvpReward.bracket = CoA::Read::U32(msg);
        s_pvpReward.f1C = CoA::Read::U32(msg);
        s_pvpReward.f14 = CoA::Read::U32(msg);
        s_pvpReward.f18 = CoA::Read::U32(msg);
        s_pvpReward.f10 = CoA::Read::U32(msg);
        CoA::Events::Signal("ASCENSION_CUSTOM_PVP_REWARD_INFO_AVAILABLE", nullptr);
        return 1;
    }

    int Ascension_QueryArenaRewardInfo(lua_State*) {   // FUN_100e07f0
        CoA::Packet(0x53E).Send();
        return 0;
    }

    // FUN_100e0660: +8, +4, the bracket's name (else UNEXPECTED_ENUM_VALUE_<n>), +0x1C, +0x14, +0x18, +0x10.
    int Ascension_GetArenaRewardInfo(lua_State* L) {
        static const char* const kBrackets[] = { "BRACKET_NONE", "BRACKET_COMBATANT", "BRACKET_CHALLENGER", "BRACKET_RIVAL",
            "BRACKET_DUELIST", "BRACKET_GLADIATOR" };
        const PvPRewardInfo& r = s_pvpReward;
        CoA::Script::PushInt(L, static_cast<int32_t>(r.f08));
        CoA::Script::PushInt(L, static_cast<int32_t>(r.f04));
        const std::string bracket = r.bracket < 6 ? kBrackets[r.bracket] : "UNEXPECTED_ENUM_VALUE_" + std::to_string(r.bracket);
        CoA::Script::PushStr(L, bracket.c_str());
        CoA::Script::PushInt(L, static_cast<int32_t>(r.f1C));
        CoA::Script::PushInt(L, static_cast<int32_t>(r.f14));
        CoA::Script::PushInt(L, static_cast<int32_t>(r.f18));
        CoA::Script::PushInt(L, static_cast<int32_t>(r.f10));
        return 7;
    }

    // ---- GetAdditionalRoleRequirementsByClassID ------------------------------------------------------------------------
    // FUN_10a6e990(class): each of the ChrClassesRoles.dbc row's three slots whose role & 0xE is set, {Role,
    // ConditionType, ConditionTypeName, ConditionValue, Met} (the condition through AttachFixes' role condition, which
    // is false without a player). An empty table for an unknown class.
    int GetAdditionalRoleRequirementsByClassID(lua_State* L) {
        const uint32_t cls = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));
        lua_createtable(L, 0, 0);
        const uint8_t* row = CoA::Dbc::Get("DBFilesClient\\ChrClassesRoles.dbc").Row(cls);

        if (!row) {
            return 1;
        }

        bool (*condition)(int32_t, uint32_t) = CoA::AttachFixes::Dependencies().roleCondition;
        int32_t index = 1;

        for (int i = 0; i < 3; ++i) {
            const uint32_t role = CoA::Dbc::Table::U32(row, 8 + i * 4) & 0xE;

            if (!role) {
                continue;
            }

            const int32_t type = CoA::Dbc::Table::I32(row, 0x14 + i * 4);
            const uint32_t value = CoA::Dbc::Table::U32(row, 0x20 + i * 4);
            const bool met = condition && condition(type, value);
            CoA::Script::PushInt(L, index++);
            lua_createtable(L, 0, 0);
            CoA::Script::SetField(L, "Role", static_cast<int32_t>(role));
            CoA::Script::SetField(L, "ConditionType", type);
            lua_pushstring(L, "ConditionTypeName");
            lua_pushstring(L, type == 0 ? "None" : type == 1 ? "SpellKnown" : type == 2 ? "AuraActive" : "Unknown");
            lua_settable(L, -3);
            CoA::Script::SetField(L, "ConditionValue", static_cast<int32_t>(value));
            lua_pushstring(L, "Met");
            CoA::Script::PushBool(L, met);
            lua_settable(L, -3);
            lua_settable(L, -3);
        }

        return 1;
    }

    // ---- GetCreatureInfoInstant / IsQuestSequenced ------------------------------------------------------------------------
    // FUN_10209b30(entry): exactly one numeric argument; the bridged creature record as {creatureID, name}, else nil.
    int GetCreatureInfoInstant(lua_State* L) {
        if (lua_gettop(L) == 1 && lua_isnumber(L, 1)) {
            const uint32_t entry = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));

            if (const CoA::BridgeStore::CreatureRecord* r = CoA::BridgeStore::CreatureByEntry(entry)) {
                lua_createtable(L, 0, 0);
                lua_checkstack(L, 2);
                CoA::Script::SetField(L, "creatureID", static_cast<int32_t>(r->entry));
                CoA::Script::PushStr(L, "name");
                CoA::Script::PushStr(L, r->name);
                lua_settable(L, -3);
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // FUN_1021eb20(quest): the quest addon store's record +8 & 1, or nil while it is not loaded. The store comes with the
    // QuestAddon module; until then nil.
    int IsQuestSequenced(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        static_cast<void>(CoA::Script::CheckNumber(L, 1));
        lua_pushnil(L);
        return 1;
    }

    // ---- FindScriptEvents(text) (FUN_100d8410) ----
    // Every event name (FUN_10278d20: the custom list, then the client's 600 from FUN_10073fe0's table) whose
    // lower-case form contains the lower-case text -- all of them for "".
    const char* const kClientEventNames[] = {
#include "CoA/Source/Modules/ScriptEventNames.generated.inc"
    };

    std::string Lower(std::string v) {
        for (char& c : v) {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }

        return v;
    }

    int FindScriptEvents(lua_State* L) {
        std::string text;

        if (!CoA::Script::ReadString(L, text)) {
            return 0;
        }

        text = Lower(text);
        std::vector<const char*> all = CoA::Events::CustomNames();
        all.insert(all.end(), std::begin(kClientEventNames), std::end(kClientEventNames));
        std::vector<const char*> found;

        for (const char* name : all) {
            if (text.empty() || Lower(name).find(text) != std::string::npos) {
                found.push_back(name);
            }
        }

        lua_createtable(L, 0, static_cast<int>(found.size()));

        for (uint32_t i = 0; i < found.size(); ++i) {
            CoA::Script::PushNum(L, i + 1);
            CoA::Script::PushStr(L, found[i]);
            lua_settable(L, -3);
        }

        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "FindScriptEvents", FindScriptEvents),
        COA_NATIVE("", "Ascension_GetArenaRewardInfo", Ascension_GetArenaRewardInfo),
        COA_NATIVE("", "Ascension_QueryArenaRewardInfo", Ascension_QueryArenaRewardInfo),
        COA_NATIVE("", "CombatLogDisable", CombatLogDisable),
        COA_NATIVE("", "CombatLogDisableAll", CombatLogDisableAll),
        COA_NATIVE("", "GM_GetTransactionContent", GM_GetTransactionContent),
        COA_NATIVE("", "GM_GetTransactionIdList", GM_GetTransactionIdList),
        COA_NATIVE("", "GetAdditionalRoleRequirementsByClassID", GetAdditionalRoleRequirementsByClassID),
        COA_NATIVE("", "GetBattlefieldList", GetBattlefieldList),
        COA_NATIVE("", "GetCreatureInfoInstant", GetCreatureInfoInstant),
        COA_NATIVE("", "IsQuestSequenced", IsQuestSequenced),
        COA_NATIVE("", "RequestMultipleAssetQuery", RequestMultipleAssetQuery),
        COA_NATIVE("", "RequestSingleAssetQuery", RequestSingleAssetQuery),
        COA_NATIVE("", "SetAttackAnimation", SetAttackAnimation),
        COA_NATIVE("", "SetAttackDistance", SetAttackDistance),
        COA_NATIVE("", "SetFadeOutGameObjects", SetFadeOutGameObjects),
        COA_NATIVE("", "SubmitGameFeedback", SubmitGameFeedback),
        COA_NATIVE("", "GetCharacterSelectionMailData", GetCharacterSelectionMailData),
        COA_NATIVE("", "GetCharacterSelectionGameModeData", GetCharacterSelectionGameModeData),
        COA_NATIVE("", "SetCharacterSelectionSortOrder", SetCharacterSelectionSortOrder),
        COA_NATIVE("", "GetErrorCallstackHeight", GetErrorCallstackHeight),
        COA_NATIVE("", "CanPerformAction", CanPerformAction),
        COA_NATIVE("", "GetKnownAddons", GetKnownAddons),
        COA_NATIVE("", "GetSecureAddons", GetSecureAddons),
        COA_NATIVE("", "IsKnownAddon", IsKnownAddon),
        COA_NATIVE("", "ReadCustomWTF", ReadCustomWTF),
        COA_NATIVE("", "WriteCustomWTF", WriteCustomWTF),
        COA_NATIVE("", "RegisterSavedCVar", RegisterSavedCVar),
    };
}

int32_t CoA::GlobalsJ::OnKnownAddonList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    s_knownAddons.clear();

    for (uint32_t n = CoA::Read::U32(msg); n && !CoA::Read::Failed(msg); --n) {
        KnownAddon addon;
        addon.name = CoA::Read::Str(msg);
        addon.secure = CoA::Read::U8(msg) != 0;

        if (CoA::Read::Failed(msg)) {
            break;
        }

        s_knownAddons.push_back(addon);
    }

    return 1;
}

bool CoA::GlobalsJ::IsKnownAddon(const char* name) {
    for (const KnownAddon& addon : s_knownAddons) {
        if (addon.name == name) {
            return true;
        }
    }

    return false;
}

bool CoA::GlobalsJ::IsSecureAddon(const char* name) {
    for (const KnownAddon& addon : s_knownAddons) {
        if (addon.name == name) {
            return addon.secure;
        }
    }

    return false;
}

std::string CoA::GlobalsJ::CustomWtfPath(const std::string& name) {
    return "WTF\\Custom\\" + name + ".json";
}

void CoA::GlobalsJ::SetInsideRunScript(bool inside) {
    s_insideProtectedCall = inside ? 1 : 0;
}

bool CoA::GlobalsJ::InsideRunScript() {
    return s_insideProtectedCall != 0;
}

void CoA::GlobalsJ::OnErrorHandler(lua_State* L, int32_t entering) {
    if (!entering) {
        s_errorCallstackHeight = 0;
        return;
    }

    // CoA64 diagnostic (not in the original; only with -extlog): the error text, so world-entry errors can be measured.
    if (CoA::Log::Enabled()) {
        const char* message = lua_tostring(L, -1);
        CoA::Log::Printf("Lua error: %s", message ? message : "(no message)");

        CoA::Log::Printf("  taint: %s", lua_tainted ? lua_tainted : "(secure)");

        // The innermost frames (and, past a deep stack, the outermost ones), so a recursion shows its loop.
        lua_Debug f;
        int depth = 0;

        while (lua_getstack(L, depth, &f)) {
            ++depth;
        }

        for (int i = 0; i < depth; ++i) {
            if (i == 8 && depth > 16) {
                CoA::Log::Printf("  ... %d frames ...", depth - 16);
                i = depth - 9;
            }

            if (lua_getstack(L, i, &f) && lua_getinfo(L, "Sln", &f)) {
                CoA::Log::Printf("  #%d %s:%d %s", i, f.short_src, f.currentline, f.name ? f.name : "?");
            }
        }
    }

    lua_Debug ar;

    while (lua_getstack(L, s_errorCallstackHeight, &ar)) {
        ++s_errorCallstackHeight;
    }
}

bool CoA::GlobalsJ::CombatLogAllowed(uint32_t eventType, uint32_t spell, uint32_t v58, uint32_t v5C, uint32_t v60) {
    if (s_combatLogDisableAll) {
        return false;
    }

    auto quiet = [&]() {
        return (CoA::AttachFixes::CustomAttr(spell, 0x14) & 4) != 0;
    };

    if ((eventType == 0xA || eventType == 0x23) && quiet() && v58 == v5C) {
        return false;
    }

    if ((eventType == 9 || eventType == 0x22) && quiet() && v5C == v60) {
        return false;
    }

    return std::find(s_combatLogDisabled.begin(), s_combatLogDisabled.end(), eventType) == s_combatLogDisabled.end();
}

int32_t CoA::GlobalsJ::AttackAnimation() {
    return s_attackAnimation;
}

float CoA::GlobalsJ::AttackDistance() {
    return s_attackDistance;
}

bool CoA::GlobalsJ::FadeOutGameObjects() {
    return s_fadeOutGameObjects != 0;
}

void CoA::GlobalsJ::Reset() {
    s_combatLogDisableAll = false;
    s_combatLogDisabled.clear();
    s_attackAnimation = 0;
    s_attackDistance = 0.0f;
    s_fadeOutGameObjects = 1;
    s_transactions.clear();
    s_transactionOrder.clear();
    s_pvpReward = PvPRewardInfo();
    s_knownAddons.clear();
    s_errorCallstackHeight = 0;
    s_insideProtectedCall = 0;
}

void CoA::GlobalsJ::Init() {
    COA_PACKET(0x94E, CoA::GlobalsJ::OnKnownAddonList);
    COA_PACKET(0x770, OnSelectionMail);
    COA_PACKET(0x771, OnSelectionGameMode);
    COA_PACKET(0x9B7, OnTransactionCreated);
    COA_PACKET(0x9B8, OnTransactionClosed);
    COA_PACKET(0x9B9, OnTransactionList);
    COA_PACKET(0x925, OnPvPRewardInfo);
    COA_EVENT("ASCENSION_CUSTOM_GM_TRANSACTION_CREATED");
    COA_EVENT("ASCENSION_CUSTOM_GM_TRANSACTION_LIST_UPDATED");
    COA_EVENT("ASCENSION_CUSTOM_PVP_REWARD_INFO_AVAILABLE");
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
