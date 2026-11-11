#ifndef WOW_SOURCE_GLUE_CHARACTERSELECTIONDISPLAY_H
#define WOW_SOURCE_GLUE_CHARACTERSELECTIONDISPLAY_H

#include <cstddef>
#include <cstdint>
#include <string>
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/WowServices/RealmConnection.h"

class CCharacterComponent;
class CM2Model;

class CharacterSelectionDisplay {
    public:
    /* 0000 */ CHARACTER_INFO m_characterInfo;
    /* 0188 */ CCharacterComponent* m_component;
    /* 018C */ CM2Model* m_petModel;
    /* 0190 */ float m_petAlpha;
    /* 0194 */ uint32_t unused_0194;

    static void SetCharacterFacing(float rotation);

    // Two Forms: the form a Worgen or Dracthyr is not showing, standing beside the selected character.
    static CCharacterComponent* m_formMirror;
    static void ClearFormMirror();
    static void ShowFormMirror(CharacterSelectionDisplay& display);
    // The Twins (mod-subclass race 96): the brother beside the selected character, in his own gear when the server
    // sent it (MISC_PLAYER_DATA_PAYLOAD id 'TWNS'), otherwise in the character's.
    static bool m_formMirrorIsTwin;
    static void ShowTwinMirror(CharacterSelectionDisplay& display);
    static void KeepTwinMirror(CharacterSelectionDisplay& display);
    // Two-Players (mod-subclass): a body left in its Mutate Me look ('MUTD' payload) shows that look here too.
    static void KeepMutatedLook(CharacterSelectionDisplay& display);
    static void ClearMutatedLook();
    static int32_t SetSelectedCharacterInfo(CHARACTER_INFO* info);

    CharacterSelectionDisplay();
    ~CharacterSelectionDisplay();
};

#endif
