// Copies the world's INTZ depth (render size, point sampled) into the bound depth surface (display size) through
// DEPTH, so world names drawn after DLSS are occluded as the world was. Colour writes are off; the colour is unused.
//   s0  INTZ depth (stored values, already in the world viewport's depth range)
sampler2D depthTex : register(s0);

struct PSOut {
    float4 color : COLOR0;
    float depth : DEPTH;
};

PSOut main(float2 uv : TEXCOORD0) {
    PSOut o;
    o.color = float4(0.0, 0.0, 0.0, 0.0);
    o.depth = tex2D(depthTex, uv).r;
    return o;
}
