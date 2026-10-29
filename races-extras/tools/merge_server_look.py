"""Server half of Esteria's retail look / body shape / two forms wire, merged into the CoA AzerothCore fork."""
import sys, shutil
sys.path.insert(0, r'C:\CoA-Build\kv')
from merge_util import Merger
E = r'C:\CoA-Build\sources\esteriawow\repo\src\server'
S = r'C:\CoA-Build\core\src\server'

def rd(p):
    return open(p, 'rb').read().decode('utf-8').replace('\r\n', '\n')

# shared wire headers, with CoA race ids (Worgen 13, Dracthyr 54/58)
for n in ('RetailLookWire.h', 'BodyShapeWire.h'):
    shutil.copy(E + r'\shared\\' + n, S + r'\shared\\' + n)
m = Merger(S + r'\shared\RetailLookWire.h')
m.replace('return race == 12 ? 1 : race == 28 ? 3 : (race == 77 || race == 78) ? 5 : 0;',
          'return race == 13 ? 1 : (race == 54 || race == 58) ? 3 : 0; // CoA: Worgen, Dracthyr')
m.save()

# opcodes
m = Merger(S + r'\game\Server\Protocol\Opcodes.h')
m.replace('    TC9_SMSG_READY_FOR_REDIRECT                     = 0x520,\n',
          '    TC9_SMSG_READY_FOR_REDIRECT                     = 0x520,\n'
          '    SMSG_ESTERIA_BODY_SHAPE                         = 0x523,\n'
          '    SMSG_ESTERIA_RETAIL_LOOK                        = 0x524,\n'
          '    CMSG_ESTERIA_FORM_LOOK                          = 0x525,\n'
          '    SMSG_ESTERIA_FORM_LOOK_ACK                      = 0x526,\n')
m.save()
m = Merger(S + r'\game\Server\Protocol\Opcodes.cpp')
m.replace('    /*0x520*/ DEFINE_SERVER_OPCODE_HANDLER(TC9_SMSG_READY_FOR_REDIRECT, STATUS_NEVER);\n',
          '    /*0x520*/ DEFINE_SERVER_OPCODE_HANDLER(TC9_SMSG_READY_FOR_REDIRECT, STATUS_NEVER);\n'
          '    DEFINE_SERVER_OPCODE_HANDLER(SMSG_ESTERIA_BODY_SHAPE, STATUS_NEVER);\n'
          '    DEFINE_SERVER_OPCODE_HANDLER(SMSG_ESTERIA_RETAIL_LOOK, STATUS_NEVER);\n'
          '    DEFINE_HANDLER(CMSG_ESTERIA_FORM_LOOK, STATUS_AUTHED, PROCESS_THREADUNSAFE, &WorldSession::HandleEsteriaFormLookOpcode);\n'
          '    DEFINE_SERVER_OPCODE_HANDLER(SMSG_ESTERIA_FORM_LOOK_ACK, STATUS_NEVER);\n')
m.save()

# DB statements
m = Merger(S + r'\database\Database\Implementation\CharacterDatabase.h')
m.replace('    CHAR_SEL_ENUM_DECLINED_NAME,\n',
          '    CHAR_SEL_ENUM_DECLINED_NAME,\n    CHAR_SEL_ENUM_BODY_SHAPE,\n    CHAR_SEL_ENUM_RETAIL_LOOK,\n'
          '    CHAR_SEL_BODY_SHAPE,\n    CHAR_REP_BODY_SHAPE,\n    CHAR_DEL_BODY_SHAPE,\n'
          '    CHAR_SEL_RETAIL_LOOK,\n    CHAR_REP_RETAIL_LOOK,\n    CHAR_DEL_RETAIL_LOOK,\n'
          '    CHAR_SEL_FORM_LOOK,\n    CHAR_REP_FORM_LOOK,\n    CHAR_DEL_FORM_LOOK,\n')
m.save()
est = rd(E + r'\database\Database\Implementation\CharacterDatabase.cpp')
a = est.index('    PrepareStatement(CHAR_SEL_ENUM_BODY_SHAPE,')
b = est.index('    PrepareStatement(CHAR_UPD_EYE_LIGHTNING,')
stm = est[a:b]
m = Merger(S + r'\database\Database\Implementation\CharacterDatabase.cpp')
m.replace('    PrepareStatement(CHAR_SEL_ENUM_DECLINED_NAME,', stm + '    PrepareStatement(CHAR_SEL_ENUM_DECLINED_NAME,')
m.save()

# WorldSession.h
m = Merger(S + r'\game\Server\WorldSession.h')
m.replace('#include "AuthDefines.h"\n', '#include "AuthDefines.h"\n#include "BodyShapeWire.h"\n#include "RetailLookWire.h"\n')
m.replace('    uint64 HaranirExtra = 0;                            // CoA: Esteria Haranir appearance extension\n',
          '    uint64 HaranirExtra = 0;                            // CoA: Esteria Haranir appearance extension\n'
          '    bool HasBodyShape = false;\n    CharacterBodyShape::Values BodyShape{};\n'
          '    bool HasRetailLook = false;\n    CharacterRetailLook::Look RetailLook;\n')
m.replace('    void HandleCharEnum(PreparedQueryResult result);',
          '    void HandleCharEnum(PreparedQueryResult result, PreparedQueryResult bodyShape, PreparedQueryResult retailLook);\n'
          '    void HandleEsteriaFormLookOpcode(WorldPacket& recvPacket);')
m.save()

# Player.h
m = Merger(S + r'\game\Entities\Player\Player.h')
m.replace('    PLAYER_LOGIN_QUERY_LOAD_OFFLINE_ACHIEVEMENTS_UPDATES = 38,\n',
          '    PLAYER_LOGIN_QUERY_LOAD_OFFLINE_ACHIEVEMENTS_UPDATES = 38,\n    PLAYER_LOGIN_QUERY_LOAD_BODY_SHAPE                   = 39,\n'
          '    PLAYER_LOGIN_QUERY_LOAD_RETAIL_LOOK                  = 40,\n    PLAYER_LOGIN_QUERY_LOAD_FORM_LOOK                    = 41,\n')
m.replace('#include "ArenaTeam.h"\n', '#include "ArenaTeam.h"\n#include "BodyShapeWire.h"\n#include "RetailLookWire.h"\n')
eh = rd(E + r'\game\Entities\Player\Player.h')
a = eh.index('    // Body sliders (see BodyShapeWire.h)')
b = eh.index('    void SendTransferAborted(')
m.replace('    void SendInitialPacketsAfterAddToMap();\n', '    void SendInitialPacketsAfterAddToMap();\n' + eh[a:b])
a = eh.index('    CharacterBodyShape::Values _bodyShape{};')
b = eh.index('_parkedLook{};', a) + len('_parkedLook{};\n')
members = eh[a:b]
# put the members right before the closing of the class: after a stable private member we can find
m.replace('class Player : public Unit, public GridObject<Player>\n', 'class Player : public Unit, public GridObject<Player>\n', optional=True)
m.s = m.s.rstrip('\n')
m.save()
open(r'C:\CoA-Build\kv\_members.txt', 'w').write(members)

# Player.cpp: functions + delete
ep = rd(E + r'\game\Entities\Player\Player.cpp')
a = ep.index('void Player::LoadBodyShape(PreparedQueryResult result)')
b = ep.index('void Player::SendInitialPacketsAfterAddToMap()')
funcs = ep[a:b]
m = Merger(S + r'\game\Entities\Player\Player.cpp')
m.replace('void Player::SendInitialPacketsAfterAddToMap()\n{\n',
          funcs + 'void Player::SendInitialPacketsAfterAddToMap()\n{\n    SendBodyShapeTo(this);\n    SendRetailLookTo(this);\n')
m.replace('''                stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SETTINGS);
                stmt->SetData(0, lowGuid);
                trans->Append(stmt);
''', '''                stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_SETTINGS);
                stmt->SetData(0, lowGuid);
                trans->Append(stmt);

                for (auto id : { CHAR_DEL_BODY_SHAPE, CHAR_DEL_RETAIL_LOOK, CHAR_DEL_FORM_LOOK })
                {
                    stmt = CharacterDatabase.GetPreparedStatement(id);
                    stmt->SetData(0, lowGuid);
                    trans->Append(stmt);
                }
''')
m.save()

m = Merger(S + r'\game\Entities\Player\PlayerStorage.cpp')
m.replace('    _LoadCharacterSettings(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_CHARACTER_SETTINGS));\n',
          '    _LoadCharacterSettings(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_CHARACTER_SETTINGS));\n'
          '    LoadBodyShape(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_BODY_SHAPE));\n'
          '    LoadRetailLook(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_RETAIL_LOOK));\n'
          '    LoadFormLook(holder.GetPreparedResult(PLAYER_LOGIN_QUERY_LOAD_FORM_LOOK));\n')
m.save()

m = Merger(S + r'\game\Entities\Player\PlayerUpdates.cpp')
m.replace('''inline void UpdateVisibilityOf_helper(Player* player, Player* target,
                                      std::vector<Unit*>& v)
{
    player->GetObjectVisibilityContainer().LinkWorldObjectVisibility(target);
    v.push_back(target);
''', '''inline void UpdateVisibilityOf_helper(Player* player, Player* target,
                                      std::vector<Unit*>& v)
{
    player->GetObjectVisibilityContainer().LinkWorldObjectVisibility(target);
    v.push_back(target);
    target->SendBodyShapeTo(player);
    target->SendRetailLookTo(player);
''')
m.replace('''            target->SendUpdateToPlayer(this);
            GetObjectVisibilityContainer().LinkWorldObjectVisibility(target);

            // target aura''', '''            target->SendUpdateToPlayer(this);
            GetObjectVisibilityContainer().LinkWorldObjectVisibility(target);

            if (Player* other = target->ToPlayer())
            {
                other->SendBodyShapeTo(this);
                other->SendRetailLookTo(this);
            }

            // target aura''')
m.save()

# CharacterHandler.cpp
ec = rd(E + r'\game\Handlers\CharacterHandler.cpp')
a = ec.index('// Two Forms: the client commits both forms')
b = ec.index('void WorldSession::HandleCharFactionOrRaceChange(')
formhandler = ec[a:b]
m = Merger(S + r'\game\Handlers\CharacterHandler.cpp')
m.replace('''    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_ACHIEVEMENT_OFFLINE_UPDATES);
    stmt->SetData(0, rawGUID);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_OFFLINE_ACHIEVEMENTS_UPDATES, stmt);
''', '''    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_ACHIEVEMENT_OFFLINE_UPDATES);
    stmt->SetData(0, rawGUID);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_OFFLINE_ACHIEVEMENTS_UPDATES, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_BODY_SHAPE);
    stmt->SetData(0, rawGUID);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BODY_SHAPE, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_RETAIL_LOOK);
    stmt->SetData(0, rawGUID);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_RETAIL_LOOK, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_FORM_LOOK);
    stmt->SetData(0, rawGUID);
    res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_FORM_LOOK, stmt);
''')
# enum
m.replace('void WorldSession::HandleCharEnum(PreparedQueryResult result)\n{\n', '''void WorldSession::HandleCharEnum(PreparedQueryResult result, PreparedQueryResult bodyShape, PreparedQueryResult retailLook)
{
    using RetailPayload = std::array<uint8, CharacterRetailLook::PayloadSize>;
    std::unordered_map<uint32, RetailPayload> retailLooks, parkedLooks; // parked: the Two Forms look that is not showing
    if (retailLook)
    {
        do
        {
            auto const payload = (*retailLook)[1].Get<Binary, CharacterRetailLook::PayloadSize>();
            CharacterRetailLook::Look look;
            if (CharacterRetailLook::ReadPayload(payload.data(), payload.size(), look))
                ((*retailLook)[2].Get<uint8>() ? parkedLooks : retailLooks)[(*retailLook)[0].Get<uint32>()] = payload;
        } while (retailLook->NextRow());
    }
    std::unordered_map<uint32, std::array<uint8, CharacterBodyShape::PayloadSize>> bodyShapes;
    if (bodyShape)
    {
        do
        {
            auto const payload = (*bodyShape)[1].Get<Binary, CharacterBodyShape::PayloadSize>();
            CharacterBodyShape::Values values{};
            if (CharacterBodyShape::ReadPayload(payload.data(), payload.size(), values) && !CharacterBodyShape::AllDefault(values))
                bodyShapes[(*bodyShape)[0].Get<uint32>()] = payload;
        } while (bodyShape->NextRow());
    }
    ByteBuffer bodyShapeBlock, retailLookBlock, parkedLookBlock;
    uint32 bodyShapeCount = 0, retailLookCount = 0, parkedLookCount = 0;

''')
m.replace('''                _legitCharacters.insert(guid);
                ++num;
                uint64 extra = (*result)[result->GetFieldCount() - 1].Get<uint64>();''', '''                _legitCharacters.insert(guid);
                ++num;
                if (auto const look = retailLooks.find(guid.GetCounter()); look != retailLooks.end())
                {
                    retailLookBlock << guid.GetRawValue();
                    retailLookBlock.append(look->second.data(), look->second.size());
                    ++retailLookCount;
                }
                if (auto const look = parkedLooks.find(guid.GetCounter()); look != parkedLooks.end())
                {
                    parkedLookBlock << guid.GetRawValue();
                    parkedLookBlock.append(look->second.data(), look->second.size());
                    ++parkedLookCount;
                }
                if (auto const shape = bodyShapes.find(guid.GetCounter()); shape != bodyShapes.end())
                {
                    bodyShapeBlock << guid.GetRawValue();
                    bodyShapeBlock.append(shape->second.data(), shape->second.size());
                    ++bodyShapeCount;
                }
                uint64 extra = (*result)[result->GetFieldCount() - 1].Get<uint64>();''')
m.replace('''        data << haranirCount << uint32(0x32455848);     // HXE2: GUID + uint64 extension
    }
''', '''        data << haranirCount << uint32(0x32455848);     // HXE2: GUID + uint64 extension
    }
    if (bodyShapeCount)
    {
        data.append(bodyShapeBlock);
        data << bodyShapeCount << CharacterBodyShape::EnumMagic; // BSE1
    }
    if (retailLookCount)
    {
        data.append(retailLookBlock);
        data << retailLookCount << CharacterRetailLook::EnumMagic; // RLE1
    }
    if (parkedLookCount)
    {
        data.append(parkedLookBlock);
        data << parkedLookCount << CharacterRetailLook::ParkedMagic; // RLP1
    }
''')
m.replace('''    _queryProcessor.AddCallback(CharacterDatabase.AsyncQuery(stmt).WithPreparedCallback(std::bind(&WorldSession::HandleCharEnum, this, std::placeholders::_1)));''',
'''    _queryProcessor.AddCallback(CharacterDatabase.AsyncQuery(stmt).WithChainingPreparedCallback(
        [this](QueryCallback& callback, PreparedQueryResult result)
        {
            auto* bodyShapeStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ENUM_BODY_SHAPE);
            bodyShapeStmt->SetData(0, GetAccountId());
            callback.WithChainingPreparedCallback(
                [this, result](QueryCallback& lastCallback, PreparedQueryResult bodyShape)
                {
                    auto* lookStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ENUM_RETAIL_LOOK);
                    lookStmt->SetData(0, GetAccountId());
                    lookStmt->SetData(1, GetAccountId());
                    lastCallback.WithPreparedCallback(
                        [this, result, bodyShape](PreparedQueryResult retailLook)
                        {
                            HandleCharEnum(result, bodyShape, retailLook);
                        });
                    lastCallback.SetNextQuery(CharacterDatabase.AsyncQuery(lookStmt));
                });
            callback.SetNextQuery(CharacterDatabase.AsyncQuery(bodyShapeStmt));
        }));''')
# create: read tails
m.replace('''    // CoA: Esteria's Haranir append a uint64 appearance and the HRC1 magic to the stock packet
    if (UsesHaranirAppearance(createInfo->Race))
    {''', '''    // CoA: Esteria's retail look / body slider footers ride at the very end of the packet
    if (!CharacterRetailLook::ReadTail(recvData, createInfo->HasRetailLook, createInfo->RetailLook) ||
        (createInfo->HasRetailLook && (createInfo->RetailLook.race != createInfo->Race || createInfo->RetailLook.sex != createInfo->Gender)) ||
        !CharacterBodyShape::ReadTail(recvData, createInfo->HasBodyShape, createInfo->BodyShape))
    {
        SendCharCreate(CHAR_CREATE_FAILED);
        return;
    }

    // CoA: Esteria's Haranir append a uint64 appearance and the HRC1 magic to the stock packet
    if (UsesHaranirAppearance(createInfo->Race))
    {''')
m.s = m.s.replace('void WorldSession::HandleCharFactionOrRaceChange(WorldPacket& recvData)', formhandler + 'void WorldSession::HandleCharFactionOrRaceChange(WorldPacket& recvData)', 1)
m.save()
print('members to place in Player.h private section: C:/CoA-Build/kv/_members.txt')
