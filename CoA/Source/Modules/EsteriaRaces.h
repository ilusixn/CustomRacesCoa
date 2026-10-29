#ifndef COA_SOURCE_MODULES_ESTERIARACES_H
#define COA_SOURCE_MODULES_ESTERIARACES_H

#include <cstdint>

// Ported from EsteriaAppearance.dll (the 32-bit client's native appearance helper): custom race ids, their skill-line race
// and the hooks the decompiled client calls (each one replaces a patched site of the original Wow.exe).
namespace CoA::Esteria {
    // The Esteria race id the catalogs know a client race id by (127 = not an Esteria race).
    unsigned RaceOf(unsigned race);
    // Race id the skill-line / spell race masks should see.
    unsigned SkillRace(unsigned race);
    // CycleCharCustomization("EA_...", n): the Esteria commands (first argument not a number); returns the Lua result count.
    int Cycle(void* lua);

    // Hooks the decompiled client calls (each replaces one patched site of the original Wow.exe; see "CoA: Esteria..." comments).
    void Init();
    void NetInit();                                             // the Esteria extension opcodes (body sliders, Retail look, form ack)
    void LeaveWorld();
    void InstallCrashTrace();
    void Skin(void* shared);                                    // CM2Shared*: after its skin profile loaded
    unsigned WideSource(const void* section, const void* skin); // M2SkinSection*, M2SkinProfile*
    unsigned DrawStart(const void* section, void* instance);    // M2SkinSection*, CM2Model*
    void Geometry(void* character);                             // CCharacterComponent*
    void CreateExtra(void* packet);                             // CDataStore*: CMSG_CHAR_CREATE about to be sent
    void EnumExtra(void* packet);                               // CDataStore*: SMSG_ENUM_CHARACTERS_RESULT
    void SelectExtra(void* character);                          // CCharacterComponent* of a character-select entry
    void UnitExtra(void* unit);                                 // CGUnit_C*
    void ForgetCharacter(void* character);
    void AppearanceContext(void* character);
    void SectionArguments(unsigned* arguments);                 // {table, race, sex, variation, index, colour}; writes index/colour
    bool DirectSection(void* character, unsigned operation, unsigned const* arguments);
    unsigned SectionCount(unsigned const* arguments);           // 0xFFFFFFFF = not handled

    namespace BodySliders {
        void SavePending();   // the creator's Finish: keep the Body page values for the new character
        void Revert();        // back to the last saved sliders
        void SaveNow();       // the in-world Body panel's Save button
        void SyncPlayer();    // the local player in the world: restore / persist the sliders by character name
        void Init();
    }
}

#endif
