#include "common.hlsl"

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    // float4 worldPos : POSITION;
};

VSOutput VSMain(VSInput i)
{
    VSOutput o;
    
    float4 worldPos = mul(float4(i.position, 1.f), MVPCB.ModelMatrix);
    
    o.position = mul(worldPos, MVPCB.MVP);
    // o.worldPos = mul(float4(i.position, 1.f), MVPCB.ModelMatrix);
    return o;
}

void PSMain(VSOutput i)
{
    float d = i.position.z;
}