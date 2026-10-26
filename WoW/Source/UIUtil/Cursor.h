#ifndef WOW_SOURCE_UIUTIL_CURSOR_H
#define WOW_SOURCE_UIUTIL_CURSOR_H

#include <cstdint>
#include "WoW/Source/UIUtil/Types.h"
#include "WoW/Source/Object/WGUID.h"

struct MipBits;

int32_t CopyCursorImage(uint32_t* image, MipBits* mipImages, uint32_t width, uint32_t height);
void CursorInitialize();
void CursorDestroy();

void CursorReleaseItemMips();
void CursorSetHeldItem(WOWGUID item);
CURSORMODE CursorGetResetMode();
void CursorResetMode();
void CursorSetMode(CURSORMODE mode);
void CursorSetResetMode(CURSORMODE mode);
CURSORMODE CursorGetModeFromName(const char* name);
int32_t CursorSetFromName(const char* name);
void CursorDropItem();
void UpdateCursor();
void CursorGrabMoney(uint32_t amount);
void CursorGrabSpell(const char* texture);
void CursorGrabVirtualItem(int32_t displayId);

// CoA: AscCursor.cpp -- the statics the DLL reads and writes directly (CursorClient.cpp).
uint32_t CoA_CursorGetMode();
void CoA_CursorStoreMode(uint32_t mode);
bool CoA_CursorHasItem();
const char* CoA_CursorName(uint32_t mode);

#endif
