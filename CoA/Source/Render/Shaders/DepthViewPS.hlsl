// Depth debug view (ps_3_0): samples the INTZ depth texture (point). c0.x = 1 (coaRenderDebug 1): sqrt(sqrt(
// saturate((1 - depth) * 5))) as grey, near = bright (sqrt twice rather than pow(x, 0.25): no log(0) at the clear
// value 1). c0.x = 2: diagnostic float4(uv.x, uv.y, depth still at the clear value ? 1 : 0, 1) -- the red/green
// gradient shows that TEXCOORD0 reaches the pixel shader, blue marks texels never written by the world.
sampler2D s0 : register(s0);
float4 mode : register(c0);

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    float d = tex2D(s0, uv).r;
    float g = sqrt(sqrt(saturate((1.0 - d) * 5.0)));
    float4 depthView = float4(g, g, g, 1.0);
    float4 diagnostic = float4(uv.x, uv.y, d >= 0.999999 ? 1.0 : 0.0, 1.0);
    return mode.x > 1.5 ? diagnostic : depthView;
}
