// AscSceneTransform.cpp's client half: the Engine hooks in CSimpleMovieFrame::Render (Engine does not link CoA).
#include "CoA/Source/Modules/SceneTransform.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Frame/CSimpleMovieFrame.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CRect.h"

extern void (*g_movieFrameRenderHook)(CSimpleMovieFrame* frame, bool after);
extern void (*g_movieFrameViewportHook)(float& minX, float& maxX, float& minY, float& maxY);

namespace {
    // FUN_101ae900's saved transforms (the projection from device +0xF88, the view from 0x4080E0).
    C44Matrix s_projection;
    C44Matrix s_view;

    void Render(CSimpleMovieFrame* frame, bool after) {
        if (!after) {
            CRect rect;
            static_cast<CLayoutFrame*>(frame)->GetRect(&rect);
            const float q[4] = { rect.minY, rect.minX, rect.maxY, rect.maxX };
            CoA::SceneTransform::Keep(q);
            s_view = C44Matrix();
            s_projection = C44Matrix();
            GxXformView(s_view);
            s_projection = g_theGxDevicePtr->m_projection;
            return;
        }

        g_theGxDevicePtr->XformSetProjection(s_projection);   // 0x6A9B40
        g_theGxDevicePtr->XformSetView(s_view);               // 0x6A9E00
    }
}

void CoA::SceneTransformClient::Init() {
    CoA::SceneTransform::Deps& d = CoA::SceneTransform::Dependencies();
    d.toNdcHeight = DDCToNDCHeight;
    d.toNdcWidth = DDCToNDCWidth;
    g_movieFrameRenderHook = Render;
    g_movieFrameViewportHook = CoA::SceneTransform::ViewportArgs;
}
