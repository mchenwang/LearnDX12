#include "common.hlsl"

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    // float4 lightPos : POSITION0;
    float4 worldPos : POSITION;
    float3 normal : NORMAL;
    float3 textureColor : COLOR;
    float2 uv : TEXCOORD;
};

PSInput VSMain(VSInput input)
{
    PSInput o;
    o.position = mul(float4(input.position, 1.f), MVPCB.MVP);
    // o.normal = input.normal;
    o.normal = normalize(mul(float4(input.normal, 0.f), MVPCB.ModelNegaTrans).xyz);
    o.worldPos = mul(float4(input.position, 1.f), MVPCB.ModelMatrix);
    // o.textureColor = o.normal.xyz*0.5+0.5;
    o.textureColor = float3(0.5f, 0.5f, 0.5f);
    o.uv = input.uv;
    return o;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 color = g_texture.Sample(g_sampler, input.uv).rgb;
    
    float3 viewDir = normalize(passCB.eyePos.xyz - input.worldPos.xyz);
    float r = length(passCB.lightPos.xyz - input.worldPos.xyz);
    float3 lightDir = normalize(passCB.lightPos.xyz - input.worldPos.xyz);
    float3 halfVec = normalize(lightDir + viewDir);
    color += input.textureColor * passCB.lightIntensity / (r*r) * max(0.f, dot(input.normal, lightDir));
    color += input.textureColor * passCB.lightIntensity / (r*r) * pow(max(0.f, dot(input.normal, halfVec)), passCB.spotPower);
    
    float4 lightPos = mul(input.worldPos, passCB.lightVp);
    
    float shadowFactor = 1.f;
    float bias = 0.0;
    bias = max(0.005, 0.05 * (1.0 - dot(input.normal, lightDir)));
    float lightNearZ = g_shadowMap.Sample(g_sampler, (lightPos.xy)).r;
    // double bias = std::max(0.005, 0.05 * (1.0 - f.normal * (light.pos - f.world_pos).normalized()));
                
    if (lightNearZ < lightPos.z - bias) shadowFactor = 0.3;
    // return float4(lightPos.zzz, 1.f);
    return float4(color * shadowFactor, 1.f);
}