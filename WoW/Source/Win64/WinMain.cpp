#if defined(WOWDECOMP_ARCH_64)

#include <windows.h>
#include <cstdlib>
#include <float.h>
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Client.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "CoA/Source/CoA.h"

// STATIC: 0x00B311E8
static LPVOID lpFiber;

static void InvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t reserved) {
}

static bool ReturnFalse() {
    return false;
}

extern "C" int WinMainCRTStartup();

// OFFSET: 0x00401000
extern "C" int start() {
    StormRtlInitialize();
    return WinMainCRTStartup();
}

// OFFSET: 0x00406D70
int main() {
    _set_invalid_parameter_handler(InvalidParameterHandler);
    _clearfp();
    _control87(0x9001F & ~(_MCW_PC | _MCW_IC), _MCW_DN | _MCW_EM | _MCW_RC);
    ReturnFalse();

    return CommonMain();
}

// OFFSET: 0x0040B7D0
static VOID CALLBACK MainFiberProc(LPVOID parameter) {
    // CoA: Ascension.exe cave 0x4E5CB0 (entered from 0x40B7D0) -- `mov dword [0xB6B474], 1` before main()
    CGlueMgr::m_scandllOkayToLogIn = 1;
    CGlueMgr::m_scandllShouldBlockPlaying = 0;
    // CoA: the cave's LoadLibraryA("Extensions.dll") -- the CoA code is linked in
    CoA::Attach();

    *static_cast<int*>(parameter) = main();
    SwitchToFiber(lpFiber);
}

// OFFSET: 0x0040B7F0
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    SetProcessDPIAware();
    lpFiber = ConvertThreadToFiber(nullptr);

    if (!lpFiber) {
        return -1;
    }

    int result = -1;
    LPVOID fiber = CreateFiberEx(0, 0x800000, 0, MainFiberProc, &result);

    if (!fiber) {
        return -1;
    }

    SwitchToFiber(fiber);
    DeleteFiber(fiber);

    return result;
}

#endif
