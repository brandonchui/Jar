module GeometryPass;

#ifdef ENABLE_BINDLESS
import common;
#endif

struct Transform
{
    float4x4 wvp;
    float4x4 world;
    float4x4 worldInvTrans;
};

ConstantBuffer<Transform> g_transform : register(b0);

struct MaterialConstants
{
    float4 albedoColor;
    float3 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalStrength;
    float ambientOcclusionStrength;
    uint flags;
    uint hasAlbedoTexture;
    uint hasNormalTexture;
    uint hasMetallicTexture;
    uint hasRoughnessTexture;
    uint hasAmbientOcclusionTexture;
    uint hasEmissiveTexture;
    float2 pad;
};

cbuffer MaterialCB : register(b1)
{
    MaterialConstants material;
}

#ifdef ENABLE_BINDLESS
struct MaterialResources
{
    Texture2D.Handle albedoTexture;
    Texture2D.Handle normalTexture;
    Texture2D.Handle metallicTexture;
    Texture2D.Handle roughnessTexture;
};

ConstantBuffer<MaterialResources> g_resources : register(b2);

#define ALBEDO_TEXTURE g_resources.albedoTexture
#define NORMAL_TEXTURE g_resources.normalTexture
#define METALLIC_TEXTURE g_resources.metallicTexture
#define ROUGHNESS_TEXTURE g_resources.roughnessTexture

#else
Texture2D albedoTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D metallicTexture : register(t2);
Texture2D roughnessTexture : register(t3);

#define ALBEDO_TEXTURE albedoTexture
#define NORMAL_TEXTURE normalTexture
#define METALLIC_TEXTURE metallicTexture
#define ROUGHNESS_TEXTURE roughnessTexture

#endif

SamplerState textureSampler : register(s0);

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 tangent : TANGENT;
    float4 color : COLOR;
};

struct VertexOutput {
    float4 position : SV_Position;
    float3 worldPos : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 tangent : TANGENT;
    float4 color : COLOR;
};

struct PSOutput
{
    float4 albedoAO   : SV_Target0;
    float4 normalRough : SV_Target1;
    float4 material    : SV_Target2;
    float4 emissive    : SV_Target3;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;

    float4 localPosition = float4(input.position, 1.0f);
    output.position = mul(g_transform.wvp, localPosition);

    float4 worldPos = mul(g_transform.world, localPosition);
    output.worldPos = worldPos.xyz;

    float3 worldNormal = mul((float3x3)g_transform.worldInvTrans, input.normal);
    output.normal = normalize(worldNormal);

    float3 worldTangent = mul((float3x3)g_transform.world, input.tangent.xyz);
    output.tangent = float4(normalize(worldTangent), input.tangent.w);

    output.texCoord = input.texCoord;
    output.color = input.color;

    return output;
}

[shader("fragment")]
PSOutput fragmentMain(VertexOutput input)
{
    PSOutput output;

    float4 albedo = ALBEDO_TEXTURE.Sample(textureSampler, input.texCoord);
    albedo *= material.albedoColor;

    float metallic = material.metallicFactor;
    if (material.hasMetallicTexture != 0) {
        float4 metallicSample = METALLIC_TEXTURE.Sample(textureSampler, input.texCoord);
        metallic *= max(max(metallicSample.r, metallicSample.g), metallicSample.b);
    }

    float roughness = material.roughnessFactor;
    if (material.hasRoughnessTexture != 0)
        roughness *= ROUGHNESS_TEXTURE.Sample(textureSampler, input.texCoord).r;

    float ao = material.ambientOcclusionStrength;

    float3 normal = normalize(input.normal);
    if (material.hasNormalTexture != 0 && material.normalStrength > 0.0) {
        float3 normalMapSample = NORMAL_TEXTURE.Sample(textureSampler, input.texCoord).rgb;
        float3 tangentNormal = normalMapSample * 2.0 - 1.0;
        tangentNormal.xy *= material.normalStrength;
        tangentNormal = normalize(tangentNormal);

        float3 N = normalize(input.normal);
        float3 T = normalize(input.tangent.xyz);
        T = normalize(T - dot(T, N) * N);
        float3 B = cross(N, T) * input.tangent.w;

        normal = normalize(tangentNormal.x * T + tangentNormal.y * B + tangentNormal.z * N);
    }

    output.albedoAO = float4(albedo.rgb, ao);
    output.normalRough = float4(normal, roughness);
    output.material = float4(metallic, 0, 0, 0);
    output.emissive = float4(material.emissiveFactor, 0);

    return output;
}
