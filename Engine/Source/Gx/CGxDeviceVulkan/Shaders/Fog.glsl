// Fixed-function state shares the unused high pixel registers with translated companions.
layout(set = 1, binding = 0, std140) uniform FixedFog
{
    vec4 p[20];
    vec4 misc;
    vec4 v[64];   // texgen rows (CGxDeviceVulkan::ITexGen): flags, World*View, the stage matrices
} ff;

// D3D9 texgen for stage s: camera-space position, normal or reflection vector, then the stage matrix.
// flags & 3: 0 none, 1 position, 2 reflection, 3 normal; flags >> 2: 0 raw, 1 matrix, 2 projected.
bool TexGen(uint s, vec3 pos, vec3 nrm, out vec2 uv)
{
    uint f = uint(ff.v[0][s] + 0.5);
    uint kind = f & 3u;
    if (kind == 0u)
        return false;
    mat4 wv = mat4(ff.v[1], ff.v[2], ff.v[3], ff.v[4]);
    vec3 P = (wv * vec4(pos, 1.0)).xyz;
    vec3 N = mat3(wv) * nrm;
    N = dot(N, N) > 1e-12 ? normalize(N) : vec3(0.0, 0.0, 1.0);
    vec3 g = P;
    if (kind == 2u)
    {
        vec3 E = dot(P, P) > 1e-12 ? normalize(-P) : vec3(0.0, 0.0, 1.0);
        g = 2.0 * dot(E, N) * N - E;
    }
    else if (kind == 3u)
        g = N;
    uint type = f >> 2;
    if (type == 0u)
    {
        uv = g.xy;
        return true;
    }
    uint b = 5u + 4u * s;
    vec4 o = mat4(ff.v[b], ff.v[b + 1u], ff.v[b + 2u], ff.v[b + 3u]) * vec4(g, 1.0);
    uv = type == 2u && abs(o.z) > 1e-6 ? o.xy / o.z : o.xy;
    return true;
}

float FogFactor(float eyeDepth)
{
    return ff.p[15].w > 0.5
        ? clamp((ff.p[14].y - eyeDepth) / (ff.p[14].y - ff.p[14].x), 0.0, 1.0)
        : 1.0;
}
