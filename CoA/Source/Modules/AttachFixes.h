#ifndef COA_SOURCE_MODULES_ATTACHFIXES_H
#define COA_SOURCE_MODULES_ATTACHFIXES_H

#include <cstdint>
#include <string>

// AscAttachFixes.cpp: the attach init's own client fixes (hook census group 10a66d40), as marked source edits. What the
// hooks decide without client objects lives here; AttachFixesClient.cpp has the parts that read client objects.
namespace CoA::AttachFixes {
    // 0x424B50 (SFile::OpenEx): a failed open logged once to C_Logger MissingFiles (channel 6).
    void LogMissingFileOnce(const char* name);

    // 0x70CDF0 (CGGameObject_C::GetObjectName): a name holding "$CN" has its first three characters (position 0, not
    // where the token is, as the original) replaced by guidName. False (unchanged) without a token or a guidName.
    bool ReplaceNameToken(std::string& name, const char* guidName);

    // 0x81B380 (FrameScript_Object::RegisterScriptEvent): while the world UI loads, an event name the client does not
    // know is queued as a custom event and the event table rebuilt. True when added.
    bool AddUnknownEventWhileLoading(const char* name);

    // 0x6B1080 (ClientServices::GetCharacterClass) as the three LFG-role call sites see it: a stock class standing for
    // the roles ChrClassesRoles.dbc (and its conditions) grants; the class itself when the row grants none.
    uint8_t LfgRoleClass(uint8_t playerClass);

    // SMSG 0x65C: the "talk to me" marker model for (kind, state); nullptr leaves the current one.
    const char* MarkerModel(uint32_t kind, int32_t state);
    // The marker path in force while SMSG 0x65C rebuilds one object's marker; "" otherwise. 0x81F970
    // (CM2Scene::DuplicateModel) creates this model instead of duplicating its source when it is not "".
    const char*& CurrentMarkerModel();

    // 0x76DDE0 (CalculateAuctionDeposit), replaced: 1 for an item of quality 6 (template +0x14) or a run time other than
    // 720 / 1440 / 2880; else trunc(depositRate * sellPrice / 100 * runTime / 240 * rate) when that is at least 1, else 1.
    int32_t AuctionDeposit(uint32_t depositRate, uint32_t sellPrice, uint32_t runTime, uint32_t itemQuality, float rate);

    // CMSG 0x741 {u64 guid}: the server is told an NPC interaction (gossip, quest giver, trainer, merchant) closed.
    void SendGuidCleared(uint64_t guid);
    // CMSG 0x51F {"Ascension", what, "External call to <what>. Hacker/botter."}.
    void SendTamperReport(const char* what);

    // 0x6E7B00 (CGPlayer_C::AddKnownSpell) / 0x542D4E (the action-bar placement in CGSpellBook::AddKnownSpell) /
    // 0x5AB120 (CGActionBar::PutActionInSlot). Begin and End wrap the player's AddKnownSpell.
    enum class Placement { Stock, Always, Never };
    void BeginAddKnownSpell(uint32_t spell, int32_t spellClassSet, int32_t progressionRank);
    void EndAddKnownSpell();
    Placement PlacementJump();
    // True once when the next PutActionInSlot is replaced by ClearCursor(1, 1).
    bool TakeSkipPutAction();

    // 0x730050 (UnitLevelUpdateHandler) / 0x6F9260 (CEffect::AddSpecialEffect): the next special effect after a
    // unit's level update is swallowed once.
    void UnitLevelUpdated(bool isUnit);
    bool TakeSkipSpecialEffect();

    // 0x8A65E0 (SpellRec::Read): aura types past 0x13C become 4; 0x6B / 0x6C become 4 (misc value 0) once their misc
    // value passes 30.
    void ClampSpellEffect(int32_t& auraType, int32_t& miscValue);

    // 0x53D580 (CGSpellBook::CanShowAura): spells refused in a battleground or arena.
    bool RefusedInPvP(uint32_t spell);

    // 0x720010 (CGUnit_C::ChatBubbleShow): the squared distance the client compares with (stock 625), set by each
    // chat bubble and read until the next by ChatBubbleShow, CGChatBubbleFrame::Update and FootstepAnimEventHit.
    float& ChatBubbleRangeSq();
    void SetChatBubbleRange(int32_t type, int32_t mapInstanceType);

    // What the hooks ask modules not ported yet. Each module installs its entry when it is ported; until then the
    // answer is 0 / false (the custom attribute rows, a unit's addon field: unchanged).
    struct Deps {
        // AscCA::SpellCustomAttrRow: the dwords at +0x10 / +0x14 / +0x18 of the spell's row.
        bool (*spellCustomAttr)(uint32_t spell, uint32_t& attr10, uint32_t& attr14, uint32_t& attr18) = nullptr;
        // AscObjectAddon::Unit(guid)[index].value.
        bool (*unitAddonField)(uint64_t guid, uint32_t index, uint32_t& value) = nullptr;
        // AscChallenge::ActiveHasRule.
        bool (*challengeHasRule)(uint32_t rule) = nullptr;
        // FUN_10a4e970: a ChrClassesRoles.dbc condition (0 always, 1 spell known, 2 aura); false without a player.
        bool (*roleCondition)(int32_t type, uint32_t value) = nullptr;
        // CGGameUI::m_loggingIn && CGlueMgr::m_suspended; the client's event id (-1 unknown); FrameScript_CreateEvents
        // over the stock world list.
        bool (*worldLoading)() = nullptr;
        int32_t (*eventId)(const char* name) = nullptr;
        void (*rebuildEvents)() = nullptr;
    };

    Deps& Dependencies();
    // The custom attribute dword at 0x10 / 0x14 / 0x18 of the spell's row; 0 without one.
    uint32_t CustomAttr(uint32_t spell, uint32_t offset);
}

#endif
