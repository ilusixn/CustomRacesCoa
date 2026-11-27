// DLSS depth remap (ps_3_0): samples the world's INTZ depth (point) and writes the D3D-native [0, 1] depth DLSS is
// tagged with into an R32F target. The Z buffer holds minZ + ndc * (maxZ - minZ) for the world viewport's range, so
// out = saturate(stored * c0.x + c0.y) with c0 = (1 / (maxZ - minZ), -minZ / (maxZ - minZ)) (Dlss::DepthRemapFor); sky /
// clear depth at or beyond maxZ maps to 1.
sampler2D s0 : register(s0);
float4 remap : register(c0);

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    float d = tex2D(s0, uv).r;
    return float4(saturate(d * remap.x + remap.y), 0.0, 0.0, 1.0);
}
