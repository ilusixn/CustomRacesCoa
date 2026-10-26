// AscCursor.cpp's client half: the texture loads, the cursor statics, the software cursor's render layer (FUN_10a1eb80)
// and the CGxDevice::CursorSetVisible hook pointer.
#include "CoA/Source/Modules/Cursor.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Event/EvtInput.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxTexFlags.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gxu/GxuFont.h"
#include "Engine/Source/Gxu/GxuFontUtil.h"
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Scrn/Scrn.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "Engine/Source/Tempest/CRect.h"
#include "WoW/Source/UIUtil/Cursor.h"

namespace {
    HLAYER s_layer = nullptr;   // 0x10D3C7A0

    // 0xAB6400 (LoadingScreen.cpp's s_texCoord, the same four values).
    const C2Vector kTexCoords[4] = { C2Vector(0.0f, 1.0f), C2Vector(1.0f, 1.0f), C2Vector(0.0f, 0.0f), C2Vector(1.0f, 0.0f) };
    const uint16_t kIndices[4] = { 0, 1, 2, 3 };   // {0x10000, 0x30002}

    void* Load(const char* path) {   // 0x4B9760(path, 0x201, 0xAF5718, 1)
        CStatus status;
        return TextureCreate(path, CGxTexFlags(), &status, 1);
    }

    // FUN_10a1eb80: the current cursor texture as a quad at the mouse position (top-left at the pointer).
    void Paint(void*, const RECTF*, const RECTF*, float) {
        void* texture = CoA::Cursor::DrawTexture();

        if (!CoA::Cursor::Show()) {
            return;
        }

        CGxTex* gx = TextureGetGxTex(static_cast<CTexture*>(texture), 0, nullptr);

        if (!gx) {
            return;
        }

        int32_t x = 0;
        int32_t y = 0;
        OsInputGetMousePosition(&x, &y);
        CRect rect;
        g_theGxDevicePtr->CapsWindowSize(rect);

        if (!x || !y) {
            return;
        }

        if (!(rect.maxX > static_cast<float>(x)) || !(rect.maxY > static_cast<float>(y))) {
            return;
        }

        float px = 0.0f;
        float py = 0.0f;
        ConvertPosition(x, y, &px, &py);
        const float top = py - CoA::Cursor::Height();
        const float right = CoA::Cursor::Width() + px;
        const C3Vector quad[4] = { C3Vector(px, top, 0.0f), C3Vector(right, top, 0.0f), C3Vector(px, py, 0.0f), C3Vector(right, py, 0.0f) };
        const CImVector colour = CImVector(0xFF, 0xFF, 0xFF, 0xFF);

        g_theGxDevicePtr->RsPush();
        GxRsSet(GxRs_Texture0, static_cast<void*>(gx));
        GxTexSetWrap(gx, static_cast<EGxTexWrapMode>(0), static_cast<EGxTexWrapMode>(0));
        GxRsSet(GxRs_Lighting, 0);
        GxRsSet(GxRs_Fog, 0);
        GxRsSet(GxRs_BlendingMode, 2);
        GxRsSet(static_cast<EGxRenderState>(7), 1);
        GxPrimLockVertexPtrs(4, quad, sizeof(C3Vector), nullptr, 0, &colour, 0, nullptr, 0, kTexCoords, sizeof(C2Vector), nullptr, 0);
        GxDrawLockedElements(GxPrim_TriangleStrip, 4, kIndices);
        GxPrimUnlockVertexPtrs();
        g_theGxDevicePtr->RsPop();
    }

    void AddRenderLayer() {   // 0x4A8BB0(0, 2.0, 3, 0, FUN_10a1eb80, &handle); a previous handle is overwritten
        ScrnLayerCreate(nullptr, 2.0f, 3, nullptr, Paint, &s_layer);
    }

    void ReleaseRenderLayer() {   // 0x47BF30
        if (s_layer) {
            HandleClose(reinterpret_cast<HOBJECT>(s_layer));
            s_layer = nullptr;
        }
    }

    void DeviceCursorVisible(int32_t visible) {   // 0x68E750 on *0xC5DF88: the active device's override
        g_theGxDevicePtr->CursorSetVisible(visible);
    }

    float ScreenWidth() {   // (float)*0xC7D2C8
        return static_cast<float>(g_widthPixels);
    }
}

void CoA::CursorClient::Init() {
    CoA::Cursor::Deps& d = CoA::Cursor::Dependencies();
    d.load = Load;
    d.cursorName = CoA_CursorName;
    d.mode = CoA_CursorGetMode;
    d.setMode = CoA_CursorStoreMode;
    d.itemOnCursor = CoA_CursorHasItem;
    d.addRenderLayer = AddRenderLayer;
    d.releaseRenderLayer = ReleaseRenderLayer;
    d.deviceCursorVisible = DeviceCursorVisible;
    d.screenWidth = ScreenWidth;
    d.pixelToScreenHeight = PixelToScreenHeight;

    // AscRuntime::Detour(0x683640), as the Engine hook pointer.
    g_cursorSetVisibleHook = CoA::Cursor::CursorVisible;
}
