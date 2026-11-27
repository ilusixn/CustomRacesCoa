// Model motion pixel shader (ps_3_0): perspective divide of the interpolated current / previous clip .xyw from
// SkinnedMotionVS, converted to UV (u right, v down). Output float4(prevUV - curUV, 0, 1).
//   c0.x  1 = coverage debug view (coaRenderDebug 5): output a constant motion (0.05, 0) instead; 0 = normal
float4 coverage : register(c0);

// RenderMotion::ClipMotion (kMinClipW 1e-4, kMotionLimit 2): never Inf / NaN, which DLSS spreads as black blocks.
float2 ClipMotion(float3 cur, float3 prev) {
    bool valid = cur.z > 1e-4 && prev.z > 1e-4;
    float cw = valid ? cur.z : 1.0;
    float pw = valid ? prev.z : 1.0;
    float2 m = float2(prev.x / pw - cur.x / cw, cur.y / cw - prev.y / pw) * 0.5;
    valid = valid && abs(m.x) < 1e30 && abs(m.y) < 1e30;
    return valid ? clamp(m, -2.0, 2.0) : float2(2.0, 2.0);
}

float4 main(float3 curClip : TEXCOORD0, float3 prevClip : TEXCOORD1) : COLOR0 {
    return coverage.x > 0.5 ? float4(0.05, 0.0, 0.0, 1.0) : float4(ClipMotion(curClip, prevClip), 0.0, 1.0);
}
