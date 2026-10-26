#ifndef COA_SOURCE_MODULES_SCENETRANSFORM_H
#define COA_SOURCE_MODULES_SCENETRANSFORM_H

// AscSceneTransform.cpp (installer FUN_101aeac0):
//   0x95EED0 CSimpleMovieFrame::Render: FUN_101ae900 takes the frame's rect q (0x489230 on this +0x20) and keeps
//            {DDCToNDCHeight(q0), DDCToNDCWidth(q1), DDCToNDCHeight(q2), DDCToNDCWidth(q3)} (0x10BCA734 .. 0x10BCA740);
//            it saves the projection (device +0xF88) and the view (0x4080E0), runs the original, then sets them back
//            (0x6A9B40 / 0x6A9E00).
//   0x681F60 GxXformSetViewport: called from inside the movie render (return 0x95EF4A), FUN_101aed40 replaces its
//            first four arguments by the kept values in the order 738, 740, 734, 73C: the movie draws in its frame.
namespace CoA::SceneTransform {
    struct Deps {
        float (*toNdcHeight)(float ddc) = nullptr;   // 0x47C060
        float (*toNdcWidth)(float ddc) = nullptr;    // 0x47C050
    };

    Deps& Dependencies();
    // rect: the CRect as stored (minY, minX, maxY, maxX).
    void Keep(const float* rect);
    void ViewportArgs(float& minX, float& maxX, float& minY, float& maxY);

    void Init();
    // Tests only.
    void Reset();
}

#endif
