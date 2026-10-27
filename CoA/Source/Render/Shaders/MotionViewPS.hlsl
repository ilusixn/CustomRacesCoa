// Motion debug view (ps_3_0, with FullscreenVS). s0 = motion (rg = prevUV - curUV) or, in mode 1, the translucency
// mask. c0.x = 0: hue from the motion direction, brightness = saturate(length(m) * 64). c0.x = 1: grey blit of s0.r
// (debug 4, the mask).
sampler2D s0 : register(s0);
float4 mode : register(c0);

float3 Hue(float h) {
    return saturate(float3(abs(h * 6.0 - 3.0) - 1.0, 2.0 - abs(h * 6.0 - 2.0), 2.0 - abs(h * 6.0 - 4.0)));
}

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    float4 s = tex2D(s0, uv);
    float2 m = s.rg;
    float len = length(m);
    // atan2(0, 0) is undefined (NaN on some GPUs): no motion takes hue 0, and its brightness is 0 anyway.
    float h = len > 1e-12 ? atan2(m.y, m.x) * 0.15915494 + 0.5 : 0.0;
    float3 motion = Hue(h) * saturate(len * 64.0);
    return float4(mode.x > 0.5 ? s.rrr : motion, 1.0);
}
