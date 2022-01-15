struct ModelViewProjection
{
    matrix MVP;
};
ConstantBuffer<ModelViewProjection> MVPCB : register(b0);

// cbuffer ModelViewProjection
// {
//     float4x4
// }

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
    PSInput o;

    o.position = mul(MVPCB.MVP, position);
    o.color = color;

    return o;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}