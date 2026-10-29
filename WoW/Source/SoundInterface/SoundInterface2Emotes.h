#ifndef WOW_SOURCE_SOUNDINTERFACE_SOUNDINTERFACE2EMOTES_H
#define WOW_SOURCE_SOUNDINTERFACE_SOUNDINTERFACE2EMOTES_H

#include <cstddef>
#include <cstdint>

struct TEXTEMOTELOOKUP {
    /* 0000 */ int32_t m_soundIds[129][2]; // CoA: stock 22 races; custom race ids go up to 128
    /* 00B0 */ int32_t m_emotesTextId;

    TEXTEMOTELOOKUP();
};

#endif
