// MotionPS for alpha-keyed M2 batches (ps_3_0): texels the world cut out (alpha below the engine's reference) keep
// the camera motion; the rest write the model motion exactly as MotionPS.
//   s0    the batch's first texture (texcoord 0, no texture transform)
//   c0.x  1 = coverage debug view (coaRenderDebug 5); c1.x = alpha reference (RenderMotion::KeyAlphaRef)
sampler2D diffuse : register(s0);
float4 coverage : register(c0);
float4 alphaRef : register(c1);

float4 main(float3 curClip : TEXCOORD0, float3 prevClip : TEXCOORD1, float2 uv : TEXCOORD2) : COLOR0 {
    clip(tex2D(diffuse, uv).a - alphaRef.x);
    float curInvW = abs(curClip.z) > 1e-12 ? 1.0 / curClip.z : 1.0;
    float prevInvW = abs(prevClip.z) > 1e-12 ? 1.0 / prevClip.z : 1.0;
    float2 curUV = float2((curClip.x * curInvW + 1.0) * 0.5, (1.0 - curClip.y * curInvW) * 0.5);
    float2 prevUV = float2((prevClip.x * prevInvW + 1.0) * 0.5, (1.0 - prevClip.y * prevInvW) * 0.5);
    return coverage.x > 0.5 ? float4(0.05, 0.0, 0.0, 1.0) : float4(prevUV - curUV, 0.0, 1.0);
}
