struct ModelViewProjection
{
    matrix MVP;
};
ConstantBuffer<ModelViewProjection> MVPCB : register(b0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput o;
    o.position = mul(float4(input.position, 1.f), MVPCB.MVP);
    o.color = float4(input.normal.xyz * 0.5 + 0.5, 1.0);
    return o;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}