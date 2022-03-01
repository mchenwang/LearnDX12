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
    float2 uv : TEXCOORD;
};

VSOutput VSMain(VSInput vin)
{
	VSOutput vout;

    // vout.position = mul(float4(vin.position, 1.0f), MVPCB.MVP);
    vout.position = float4(vin.position, 1.0f);

	vout.uv = vin.uv;
	
    return vout;
}

float4 PSMain(VSOutput pin) : SV_Target
{
    return float4(g_shadowMap.Sample(g_sampler, pin.uv).rrr, 1.0f);
}
