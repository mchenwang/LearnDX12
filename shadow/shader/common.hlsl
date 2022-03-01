struct ModelViewProjection
{
    matrix MVP;
    matrix ModelNegaTrans;
    matrix ModelMatrix;
};
ConstantBuffer<ModelViewProjection> MVPCB : register(b0);

struct PassData
{
    matrix lightVp;
    float4 lightPos;
    // float3 lightDir;
    float4 eyePos;
    float3 ambientColor;
    float lightIntensity;
    float3 lightColor;
    float spotPower;
};
ConstantBuffer<PassData> passCB : register(b1);

Texture2D g_texture : register(t0);
Texture2D g_shadowMap : register(t1);
SamplerState g_sampler : register(s2);