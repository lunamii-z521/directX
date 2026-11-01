cbuffer ObjectConstants : register(b0)
{
    matrix g_model;
    matrix g_vp;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

// --- Vertex Shader ---
PSInput VSMain(VSInput input)
{
    PSInput output;
    matrix mvp = mul(g_model, g_vp);
    output.position = mul(float4(input.position, 1.0f), mvp);
    output.color = input.color;
    output.uv = input.uv;
    return output;
}

// --- Pixel Shader ---
float4 PSMain(PSInput input) : SV_Target
{
    // 1. Sample the texture at the given UV coordinate using the sampler rules
    float4 texColor = g_texture.Sample(g_sampler, input.uv);

    // 2. Combine texture color with vertex color
    return texColor * input.color;
}