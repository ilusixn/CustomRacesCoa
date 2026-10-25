// AscEventPackets.cpp's handlers that need the client (the rest are in EventPackets.cpp).
//   0x58E / 0x58F / 0x590 u32 n, then the stock handler ReceiveCreature / ReceiveSingleItem / ReceiveQuest n times on
//                         the same packet (FUN_100d3ad0 / FUN_100d3dc0 / FUN_100d4290); 0 as soon as one fails
//   0x904 SMSG_PACKET_BROADCAST (FUN_102295e0): u64 source, u64 target, float range, u16 opcode, u16 size, payload.
//                         Dropped when the player is the source or the target; with a source, it must exist, and
//                         (range >= FLT_EPSILON) the player must be within range + 10 of it (2D). The payload is then
//                         dispatched as a received packet ([u16 opcode][payload], NetClient::ProcessMessage).
//   0x675 (FUN_10a4f0c0)  u64 guid, u8 online: the player -> GROUP_JOINED / GROUP_LEFT when it has a unit token;
//                         another unit -> UNIT_CONNECTION("%s%b", its first token, online)
//   0x68F (FUN_10a4fdd0)  u32 encounter, u32 state; the DungeonEncounter row must exist. d = the current map's
//                         Map.dbc +0x44 (m_maxPlayers), or 0.
//                         1 -> ENCOUNTER_START("%u%s%u%u", id, name, difficulty, d)
//                         2 -> ENCOUNTER_END("%u%s%u%u%u", id, name, difficulty, d, 0)
//                         3 -> ENCOUNTER_END(..., 1), then BOSS_KILL("%u%s", id, name)
//   0x668 (FUN_10a4f5c0)  C string name, C string value, u8 set, u8 flag: a server CVar override. set: the saved map
//                         (0x10D3DC48) takes the CVar's current value (overwriting an earlier save), then the CVar gets
//                         `value`; else the saved value is put back and the entry erased. Unknown CVars are ignored.
//   0x742 (FUN_10a4fb30)  u32 spell, u32 value: for a known spell, ERR_USE_LOCKED_WITH_SPELL_KNOWN_SI formatted with
//                         (spell name, value) is signalled as stock event 0xBB ("%s")
// and FUN_100d3550 (after every ClientIdle): ASCENSION_PLAYER_LEVEL_UP(new, old) whenever the player's level (0 without
// a player) differs from the last one seen (0x10BDB688).
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    using StockHandler = int32_t (*)(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);

    int32_t Repeat(StockHandler handler, void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        const uint32_t n = CoA::Read::U32(msg);

        for (uint32_t i = 0; i < n; ++i) {
            if (!handler(param, static_cast<NETMESSAGE>(opcode), time, msg)) {
                return 0;
            }
        }

        return 1;
    }

    int32_t On58E(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        return Repeat(DBCacheReceiveCreature, param, opcode, time, msg);
    }

    int32_t On58F(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        return Repeat(DBCacheReceiveSingleItem, param, opcode, time, msg);
    }

    int32_t On590(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        return Repeat(DBCacheReceiveQuest, param, opcode, time, msg);
    }

    int32_t On904(void*, uint32_t, uint32_t time, CDataStore* msg) {
        const WOWGUID player = ClntObjMgrGetActivePlayer();
        WOWGUID source;
        WOWGUID target;
        source.m_guid = CoA::Read::U64(msg);
        target.m_guid = CoA::Read::U64(msg);
        const float range = CoA::Read::F32(msg);
        const uint16_t opcode = CoA::Read::U16(msg);
        const uint16_t size = CoA::Read::U16(msg);

        if (player == source || player == target) {
            return 1;
        }

        if (source.m_guid) {
            CGObject_C* src = ClntObjMgrObjectPtr(source, static_cast<OBJECT_TYPE>(0xFFFF), __FILE__, __LINE__);

            if (!src) {
                return 1;
            }

            if (range >= 1.1920929e-07f) {
                CGObject_C* me = ClntObjMgrObjectPtr(player, static_cast<OBJECT_TYPE>(0xFFFF), __FILE__, __LINE__);
                const float r = range + 10.0f;

                if (r <= 1.1920929e-07f || !me) {
                    return 1;
                }

                const C3Vector a = src->GetPosition();
                const C3Vector b = me->GetPosition();
                const float dx = b.x - a.x;
                const float dy = b.y - a.y;

                if (!(r * r >= dy * dy + dx * dx)) {
                    return 1;
                }
            }
        }

        std::vector<uint8_t> data(size + 2u);
        memcpy(data.data(), &opcode, 2);
        CoA::Read::Data(msg, data.data() + 2, size);
        CDataStore store(data.data(), static_cast<uint32_t>(data.size()));
        ClientServices::Connection()->ProcessMessage(time, &store, 1);
        return 1;
    }

    int32_t On675(void*, uint32_t, uint32_t, CDataStore* msg) {
        WOWGUID guid;
        guid.m_guid = CoA::Read::U64(msg);
        const uint32_t online = CoA::Read::U8(msg);
        int32_t count = 0;
        char** tokens = Script_GetTokenIDsFromGUID(guid, &count);

        if (guid == ClntObjMgrGetActivePlayer()) {
            if (count) {
                CoA::Events::Signal(online ? "GROUP_JOINED" : "GROUP_LEFT", nullptr);
            }

            return 1;
        }

        if (count) {
            CoA::Events::Signal("UNIT_CONNECTION", "%s%b", tokens[0], online);
        }

        return 1;
    }

    int32_t On68F(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint32_t id = CoA::Read::U32(msg);
        const uint32_t state = CoA::Read::U32(msg);
        const DungeonEncounterRec* row = g_dungeonEncounterDB.GetRecord(static_cast<int32_t>(id));

        if (!row) {
            return 1;
        }

        auto players = []() -> uint32_t {
            const MapRec* map = g_mapDB.GetRecord(s_mapId);
            return map ? static_cast<uint32_t>(map->m_maxPlayers) : 0u;
        };

        const uint32_t difficulty = static_cast<uint32_t>(row->m_difficulty);

        if (state == 2) {
            CoA::Events::Signal("ENCOUNTER_END", "%u%s%u%u%u", id, row->m_name, difficulty, players(), 0u);
        } else if (state == 1) {
            CoA::Events::Signal("ENCOUNTER_START", "%u%s%u%u", id, row->m_name, difficulty, players());
        } else if (state == 3) {
            CoA::Events::Signal("ENCOUNTER_END", "%u%s%u%u%u", id, row->m_name, difficulty, players(), 1u);
            CoA::Events::Signal("BOSS_KILL", "%u%s", id, row->m_name);
        }

        return 1;
    }

    // FUN_10114c40 -> CVar::Set: refused when name and value together exceed 0xF9 characters.
    void SetCVar(CVar* cvar, const char* value, bool setNeedsSave) {
        if (strlen(value) + strlen(cvar->m_key.m_str) > 0xF9) {
            return;
        }

        cvar->Set(value, true, false, false, setNeedsSave);
    }

    std::unordered_map<std::string, std::string> s_cvarOverrides;   // 0x10D3DC48

    int32_t On668(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string name = CoA::Read::Str(msg);
        const std::string value = CoA::Read::Str(msg);
        const uint8_t set = CoA::Read::U8(msg);
        const uint8_t flag = CoA::Read::U8(msg);
        CVar* cvar = CVar::Lookup(name.c_str());

        if (!cvar) {
            return 1;
        }

        if (set) {
            const char* current = cvar->m_stringValue.m_str;
            s_cvarOverrides[name] = current ? current : "";
            SetCVar(cvar, value.c_str(), flag != 0);
            return 1;
        }

        auto it = s_cvarOverrides.find(name);

        if (it == s_cvarOverrides.end()) {
            return 1;
        }

        SetCVar(cvar, it->second.c_str(), flag != 0);
        s_cvarOverrides.erase(it);
        return 1;
    }

    int32_t On742(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint32_t spell = CoA::Read::U32(msg);
        const uint32_t value = CoA::Read::U32(msg);
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return 1;
        }

        const char* format = FrameScript_GetText("ERR_USE_LOCKED_WITH_SPELL_KNOWN_SI", -1, static_cast<FRAMESCRIPT_GENDER>(1));
        const int n = _snprintf(nullptr, 0, format, rec.m_name, value);   // FUN_100b82f0 sizes, then formats

        if (n < 0) {
            throw std::runtime_error("Error during formatting.");
        }

        std::string text(static_cast<size_t>(n), '\0');
        _snprintf(&text[0], text.size(), format, rec.m_name, value);
        FrameScript_SignalEvent(0xBB, "%s", text.c_str());
        return 1;
    }

    uint32_t s_lastLevel = 0;   // 0x10BDB688

    void WatchLevel() {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        const uint32_t level = player ? player->m_unit->m_unitFieldLevel : 0;

        if (level == s_lastLevel) {
            return;
        }

        CoA::Events::Signal("ASCENSION_PLAYER_LEVEL_UP", "%u%u", level, s_lastLevel);
        s_lastLevel = level;
    }
}

void CoA::EventPacketsClient::Init() {
    COA_PACKET(0x58E, On58E);
    COA_PACKET(0x58F, On58F);
    COA_PACKET(0x590, On590);
    COA_PACKET(0x675, On675);
    COA_PACKET(0x68F, On68F);
    COA_PACKET(0x668, On668);
    COA_PACKET(0x742, On742);
    COA_PACKET(0x904, On904);
    CoA::Lifecycle::OnAfterIdle(WatchLevel);
    // GROUP_JOINED / GROUP_LEFT / UNIT_CONNECTION / ENCOUNTER_START / ENCOUNTER_END / BOSS_KILL are not DLL events: they
    // fire only when the client table has them, as in the original.
    COA_EVENT("ASCENSION_PLAYER_LEVEL_UP");
}
