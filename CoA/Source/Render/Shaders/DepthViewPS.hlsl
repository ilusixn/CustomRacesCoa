// Depth debug view (ps_3_0): samples the INTZ depth texture (point) and shows pow(saturate((1 - depth) * 5), 0.25) as grey,
// near = bright. Not linearised; good enough for a visual check.
sampler2D s0 : register(s0);

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    float g = pow(saturate((1.0 - tex2D(s0, uv).r) * 5.0), 0.25);
    return float4(g, g, g, 1.0);
}
