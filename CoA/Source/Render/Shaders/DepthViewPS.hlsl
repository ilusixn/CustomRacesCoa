// Depth debug view (ps_3_0): samples the INTZ depth texture (point). c0.x = 1 (coaRenderDebug 1): the depth as grey,
// 1 / (1 + yards / 16), near = bright. The Z buffer holds minZ + ndc * (maxZ - minZ) for the world viewport's range
// (c1 = minZ, 1 / (maxZ - minZ)); the NDC depth is linearised with the camera's near / far (c2 = near * far, far,
// far - near, 1 / 16). c0.x = 2: diagnostic float4(uv.x, uv.y, depth still at the clear value ? 1 : 0, 1) -- the
// red/green gradient shows that TEXCOORD0 reaches the pixel shader, blue marks texels never written by the world.
sampler2D s0 : register(s0);
float4 mode : register(c0);
float4 range : register(c1);
float4 camera : register(c2);

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    float d = tex2D(s0, uv).r;
    float ndc = saturate((d - range.x) * range.y);
    float yards = camera.x / max(camera.y - ndc * camera.z, 0.0001);
    float g = 1.0 / (1.0 + yards * camera.w);
    float4 depthView = float4(g, g, g, 1.0);
    float4 diagnostic = float4(uv.x, uv.y, d >= 0.999999 ? 1.0 : 0.0, 1.0);
    return mode.x > 1.5 ? diagnostic : depthView;
}
