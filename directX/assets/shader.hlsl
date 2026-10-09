struct VSInput
{
    float3 position : POSITION;
    float4 color    : COLOR;
};
struct PSInput
{
    float4 position : SV_Position;
    float4 color    : COLOR;
};

// VS
PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = float4(input.position, 1.f);
    output.color = input.color;
    return output;
}

// PS
float4 PSMain(PSInput input) : SV_Target
{
    return input.color;
}