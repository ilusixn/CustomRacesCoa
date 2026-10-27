// Static (camera) motion vectors (ps_3_0), reproducing RenderMotion::StaticMotion. Fullscreen pass: reconstructs each
// pixel's camera-relative position from the INTZ depth and reprojects it through the previous frame's camera.
// Matrices are the C++ row-vector Mat4 (clip = float4(p, 1) * M), uploaded as 4 rows, one float4 constant per row, so
// v * M = v.x * row0 + v.y * row1 + v.z * row2 + v.w * row3 (no HLSL matrix packing involved).
//   s0      INTZ depth, point sampled
//   c0..c3  rows of inverse(cur.view * cur.proj)
//   c4..c7  rows of prev.view * prev.proj
//   c8.xyz  cur.pos - prev.pos
//   c9.xyz  (depthMin, 1 / (depthMax - depthMin), depthMax); the caller skips the pass when depthMax == depthMin
// Output float4(prevUV - curUV, 0, 1) with UV u right, v down.
sampler2D s0 : register(s0);
float4 invCur[4] : register(c0);
float4 prevVP[4] : register(c4);
float4 eyeDelta : register(c8);
float4 depthRange : register(c9);

float4 Mul(float4 v, float4 r0, float4 r1, float4 r2, float4 r3) {
    return v.x * r0 + v.y * r1 + v.z * r2 + v.w * r3;
}

// Divide that leaves tiny / zero w alone instead of producing inf / NaN.
float SafeInvW(float w) {
    return abs(w) > 1e-12 ? 1.0 / w : 1.0;
}

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    float stored = tex2D(s0, uv).r;
    float ndcD3d = (stored - depthRange.x) * depthRange.y;
    float4 ndc = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, ndcD3d * 2.0 - 1.0, 1.0);
    // Far plane / clear value (sky): a direction at infinity, so the eye translation does not apply (w = 0).
    bool atInfinity = stored >= depthRange.z - 1e-6;

    float4 p = Mul(ndc, invCur[0], invCur[1], invCur[2], invCur[3]);
    float invW = SafeInvW(p.w);
    float3 world = p.xyz * invW + (atInfinity ? float3(0, 0, 0) : eyeDelta.xyz);

    float4 prevClip = Mul(float4(world, atInfinity ? 0.0 : 1.0), prevVP[0], prevVP[1], prevVP[2], prevVP[3]);
    float prevInvW = SafeInvW(prevClip.w);
    float2 prevUV = float2((prevClip.x * prevInvW + 1.0) * 0.5, (1.0 - prevClip.y * prevInvW) * 0.5);
    return float4(prevUV - uv, 0.0, 1.0);
}
