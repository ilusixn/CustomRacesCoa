// MotionPS for alpha-keyed M2 batches (ps_3_0): texels the world cut out (alpha below the engine's reference) keep
// the camera motion; the rest write the model motion exactly as MotionPS.
//   s0    the batch's first texture (texcoord 0, no texture transform)
//   c0.x  1 = coverage debug view (coaRenderDebug 5); c1.x = alpha reference (RenderMotion::KeyAlphaRef)
sampler2D diffuse : register(s0);
float4 coverage : register(c0);
float4 alphaRef : register(c1);

// As MotionPS.
float2 ClipMotion(float3 cur, float3 prev) {
    bool valid = cur.z > 1e-4 && prev.z > 1e-4;
    float cw = valid ? cur.z : 1.0;
    float pw = valid ? prev.z : 1.0;
    float2 m = float2(prev.x / pw - cur.x / cw, cur.y / cw - prev.y / pw) * 0.5;
    valid = valid && abs(m.x) < 1e30 && abs(m.y) < 1e30;
    return valid ? clamp(m, -2.0, 2.0) : float2(2.0, 2.0);
}

float4 main(float3 curClip : TEXCOORD0, float3 prevClip : TEXCOORD1, float2 uv : TEXCOORD2) : COLOR0 {
    clip(tex2D(diffuse, uv).a - alphaRef.x);
    return coverage.x > 0.5 ? float4(0.05, 0.0, 0.0, 1.0) : float4(ClipMotion(curClip, prevClip), 0.0, 1.0);
}
