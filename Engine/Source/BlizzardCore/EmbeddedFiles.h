#ifndef ENGINE_SOURCE_BLIZZARDCORE_EMBEDDEDFILES_H
#define ENGINE_SOURCE_BLIZZARDCORE_EMBEDDEDFILES_H

#include <cstdint>

// CoA: the Esteria tree compiles small files into the executable; here there are none.
inline bool EmbeddedFileFind(const char*, const uint8_t**, uint32_t*) { return false; }

#endif
