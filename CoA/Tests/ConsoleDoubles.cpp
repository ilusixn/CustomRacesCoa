#include "Engine/Source/ConsoleBase/IConsole.h"

// Test doubles for the client console (WoW/Source/Console), which ConsoleBase's CVar code calls. Linking the real
// console would pull in the whole client.

void ConsoleCommandExecute(const char* commandLine, int32_t addToHistory) {
}

void ConsolePostClose() {
}

void ConsolePrintf(const char* format, ...) {
}

void ConsoleWrite(const char* str, COLOR_T color) {
}

void ConsoleWriteA(const char* format, COLOR_T color, ...) {
}
