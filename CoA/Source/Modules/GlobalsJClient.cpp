#include "CoA/Source/Modules/GlobalsJ.h"
#include "Engine/Source/FrameScript/FrameScript.h"

void CoA::GlobalsJ::InstallClient() {
    // AscRuntime::ReplaceFunction(0x819730, ScriptErrorHandler).
    g_frameScriptErrorHook = CoA::GlobalsJ::OnErrorHandler;
}
