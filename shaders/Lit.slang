#include "PBR.slang"

struct Transform
{
    float4x4 wvp;           
    float4x4 world;         
    float4x4 worldInvTrans;
};

ConstantBuffer<Transform> g_transform : register(b0);

struct SpotLight
{
    float3 position;
    float range;
    float3 direction;
    float innerConeAngle;
    float3 color;
    float outerConeAngle;
    float intensity;
    float falloff;
    float2 padding;
};

cbuffer LightingInfo : register(b1, space0)
{
    float3 eyePosition;
    uint numActiveLights;
    float3 ambientLight;
    float padding;
};

StructuredBuffer<SpotLight> spotLights : register(t1, space1);

struct Material
{
    Texture2D albedoTexture;
    Texture2D normalTexture;
    Texture2D metallicTexture;
    Texture2D roughnessTexture;
    SamplerState textureSampler;

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

ParameterBlock<Material> material;

void calculateSpotLight(SpotLight light, float3 worldPos, float3 normal, float3 viewDir,
                       float3 albedo, float metallic, float roughness,
                       out float3 diffuse, out float3 specular)
{
    diffuse = float3(0, 0, 0);
    specular = float3(0, 0, 0);

    float3 lightDir = light.position - worldPos;
    float distance = length(lightDir);

    if (distance > light.range)
        return;

    lightDir = normalize(lightDir);

    float attenuation = 1.0 / (1.0 + light.falloff * distance * distance);

    float spotCos = dot(-lightDir, normalize(light.direction));
    float innerCos = cos(light.innerConeAngle);
    float outerCos = cos(light.outerConeAngle);

    if (spotCos < outerCos)
        return;

    float spotIntensity = smoothstep(outerCos, innerCos, spotCos);

    float3 radiance = light.color * light.intensity * attenuation * spotIntensity;

    pbrBRDF(normal, viewDir, lightDir, albedo, metallic, roughness, diffuse, specular);

    float NdotL = max(dot(normal, lightDir), 0.0);
    diffuse *= radiance * NdotL;
    specular *= radiance * NdotL;
}

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

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
    VertexOutput output;

    float4 localPosition = float4(input.position, 1.0f);

    output.position = mul(g_transform.wvp, localPosition);

    float4 worldPos = mul(g_transform.world, localPosition);
    output.worldPos = worldPos.xyz;

    float3 worldNormal = mul((float3x3)g_transform.worldInvTrans, input.normal);
    output.normal = normalize(worldNormal);

    float3 worldTangent = mul((float3x3)g_transform.world, input.tangent.xyz);
    output.tangent = float4(normalize(worldTangent), input.tangent.w);  // Preserve handedness in w

    output.texCoord = input.texCoord;
    output.color = input.color;

    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    float4 albedo = material.albedoTexture.Sample(material.textureSampler, input.texCoord);
    albedo *= material.albedoColor;

    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;

    if (material.hasMetallicTexture != 0) {
        float4 metallicSample = material.metallicTexture.Sample(material.textureSampler, input.texCoord);
        metallic *= max(max(metallicSample.r, metallicSample.g), metallicSample.b);
    }

    if (material.hasRoughnessTexture != 0)
        roughness *= material.roughnessTexture.Sample(material.textureSampler, input.texCoord).r;  // RED channel for roughness

    float3 normal = normalize(input.normal);

    if (material.hasNormalTexture != 0 && material.normalStrength > 0.0) {
        float3 normalMapSample = material.normalTexture.Sample(material.textureSampler, input.texCoord).rgb;

        float3 tangentNormal = normalMapSample * 2.0 - 1.0;
        tangentNormal.xy *= material.normalStrength;
        tangentNormal = normalize(tangentNormal);

        float3 N = normalize(input.normal);  
        float3 T = normalize(input.tangent.xyz);

        T = normalize(T - dot(T, N) * N);

        float3 B = cross(N, T) * input.tangent.w;

        normal = normalize(tangentNormal.x * T + tangentNormal.y * B + tangentNormal.z * N);
    }
    float3 viewDir = normalize(eyePosition - input.worldPos);

    float3 finalColor = ambientLight * albedo.rgb;

    for (uint i = 0; i < numActiveLights; i++) {
        float3 diffuse, specular;
        calculateSpotLight(spotLights[i], input.worldPos, normal, viewDir, albedo.rgb, metallic, roughness, diffuse, specular);

        finalColor += diffuse + specular;
    }

    finalColor = saturate(finalColor);

    return float4(finalColor, albedo.a);
}
