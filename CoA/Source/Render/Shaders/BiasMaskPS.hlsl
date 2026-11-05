// Translucency mask (ps_3_0, with FullscreenVS): s0 = final world colour, s1 = opaque snapshot. White where any
// colour channel differs by more than 2/255 (something translucent was blended over the opaque scene), else black.
sampler2D s0 : register(s0);
sampler2D s1 : register(s1);

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    float3 d = abs(tex2D(s0, uv).rgb - tex2D(s1, uv).rgb);
    float changed = max(d.r, max(d.g, d.b)) > 2.0 / 255.0 ? 1.0 : 0.0;
    return float4(changed, changed, changed, 1.0);
}
