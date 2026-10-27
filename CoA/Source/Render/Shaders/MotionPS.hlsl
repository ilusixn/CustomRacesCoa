// Model motion pixel shader (ps_3_0): perspective divide of the interpolated current / previous clip .xyw from
// SkinnedMotionVS, converted to UV (u right, v down). Output float4(prevUV - curUV, 0, 1).
//   c0.x  1 = coverage debug view (coaRenderDebug 5): output a constant motion (0.05, 0) instead; 0 = normal
float4 coverage : register(c0);

float4 main(float3 curClip : TEXCOORD0, float3 prevClip : TEXCOORD1) : COLOR0 {
    float curInvW = abs(curClip.z) > 1e-12 ? 1.0 / curClip.z : 1.0;
    float prevInvW = abs(prevClip.z) > 1e-12 ? 1.0 / prevClip.z : 1.0;
    float2 curUV = float2((curClip.x * curInvW + 1.0) * 0.5, (1.0 - curClip.y * curInvW) * 0.5);
    float2 prevUV = float2((prevClip.x * prevInvW + 1.0) * 0.5, (1.0 - prevClip.y * prevInvW) * 0.5);
    return coverage.x > 0.5 ? float4(0.05, 0.0, 0.0, 1.0) : float4(prevUV - curUV, 0.0, 1.0);
}
