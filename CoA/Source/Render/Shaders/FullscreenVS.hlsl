// Fullscreen-quad vertex shader (vs_3_0). Input is clip-space position; c0.xy is the D3D9 half-pixel offset
// (-1/width, +1/height), applied as pos.xy += c0.xy * pos.w.
float4 c0 : register(c0);

struct VSOut {
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

VSOut main(float4 pos : POSITION, float2 uv : TEXCOORD0) {
    VSOut o;
    o.pos = pos;
    o.pos.xy += c0.xy * pos.w;
    o.uv = uv;
    return o;
}
