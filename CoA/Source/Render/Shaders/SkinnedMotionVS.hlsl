// Skinned / rigid model motion vertex shader (vs_3_0), reproducing RenderMotion::ModelMotion. Input is the M2Vertex
// layout (POSITION float3, BLENDWEIGHT ubyte4n, BLENDINDICES ubyte4; normal / texcoords unused).
// Bone palette = vertex texture sampler 0, A32B32G32R32F 256x2, point: row 0 = this frame's bones, row 1 = last
// frame's. Bone i occupies texels 3i .. 3i+2, texel k = the k-th register the engine puts in c31+ (the transposed
// column k of the bone matrix: x = a_k, y = b_k, z = c_k, w = d_k), so bone-space output component k is
// dot(float4(pos, 1), texel k). Matrices below are uploaded as 4 rows (clip = v.x*row0 + v.y*row1 + v.z*row2 + v.w*row3).
//   c0..c3   rows of the render projection (device's current, possibly jittered, m_projNative) -> POSITION
//   c4..c7   rows of cur.proj, unjittered
//   c8..c11  rows of prev.proj
// TEXCOORD0 = current clip .xyw (cur.proj, this frame's bones), TEXCOORD1 = previous clip .xyw (prev.proj, last
// frame's bones); MotionPS divides and subtracts.
sampler2D bones : register(s0);
float4 renderProj[4] : register(c0);
float4 curProj[4] : register(c4);
float4 prevProj[4] : register(c8);

struct VSOut {
    float4 pos : POSITION;
    float3 curClip : TEXCOORD0;
    float3 prevClip : TEXCOORD1;
};

float4 Mul(float4 v, float4 r0, float4 r1, float4 r2, float4 r3) {
    return v.x * r0 + v.y * r1 + v.z * r2 + v.w * r3;
}

float4 Fetch(float index, float k, float row) {
    return tex2Dlod(bones, float4((3.0 * index + k + 0.5) / 256.0, (row + 0.5) / 2.0, 0.0, 0.0));
}

// Weighted bone-space position (w = 1) for one palette row.
float4 Skin(float3 position, float4 weights, float4 indices, float row) {
    float4 p = float4(position, 1.0);
    float3 acc = float3(0, 0, 0);
    for (int i = 0; i < 4; ++i) {
        float4 t0 = Fetch(indices[i], 0.0, row);
        float4 t1 = Fetch(indices[i], 1.0, row);
        float4 t2 = Fetch(indices[i], 2.0, row);
        acc += weights[i] * float3(dot(p, t0), dot(p, t1), dot(p, t2));
    }
    return float4(acc, 1.0);
}

VSOut main(float3 position : POSITION, float4 weights : BLENDWEIGHT, float4 indices : BLENDINDICES) {
    VSOut o;
    float4 cur = Skin(position, weights, indices, 0.0);
    float4 prev = Skin(position, weights, indices, 1.0);
    o.pos = Mul(cur, renderProj[0], renderProj[1], renderProj[2], renderProj[3]);
    o.curClip = Mul(cur, curProj[0], curProj[1], curProj[2], curProj[3]).xyw;
    o.prevClip = Mul(prev, prevProj[0], prevProj[1], prevProj[2], prevProj[3]).xyw;
    return o;
}
