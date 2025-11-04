#include "PBR.slang"

// GBuffer inputs
Texture2D gAlbedoAO : register(t0);
Texture2D gNormalRoughness : register(t1);
Texture2D gMetallicFlags : register(t2);
Texture2D gEmissive : register(t3);
Texture2D gDepth : register(t4);
SamplerState gSampler : register(s0);

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

cbuffer LightingInfo : register(b0)
{
    float4x4 invViewProj;
    float3 eyePosition;
    uint numActiveLights;
    float3 ambientLight;
    float padding;
};

StructuredBuffer<SpotLight> spotLights : register(t1, space1);

struct VertexOutput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexID : SV_VertexID)
{
    VertexOutput output;

    switch(vertexID)
    {
        case 0:
            // Top left
            output.position = float4(-1.0,  1.0, 0.5, 1.0);
            output.texCoord = float2( 0.0,  0.0);
            break;
        case 1:
            // bottom left
            output.position = float4(-1.0, -3.0, 0.5, 1.0);
            output.texCoord = float2( 0.0,  2.0);
            break;
        case 2:
        default:
            // Top right
            output.position = float4( 3.0,  1.0, 0.5, 1.0);
            output.texCoord = float2( 2.0,  0.0);
            break;
    }
 
    return output;
}

float3 createWorldPosition(float2 uv, float depth, float4x4 invViewProj)
{
    float4 clipSpacePos;
    clipSpacePos.x = uv.x * 2.0 - 1.0;
    clipSpacePos.y = (1.0 - uv.y) * 2.0 - 1.0;
    clipSpacePos.z = depth;
    clipSpacePos.w = 1.0;

    float4 worldPos = mul(invViewProj, clipSpacePos);
    worldPos.xyz /= worldPos.w;

    return worldPos.xyz;
}

// Using same function from Lit.slang when I had forward renderer
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

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float4 albedoAO = gAlbedoAO.Sample(gSampler, input.texCoord);
    float4 normalRough = gNormalRoughness.Sample(gSampler, input.texCoord);
    float4 metallicFlags = gMetallicFlags.Sample(gSampler, input.texCoord);
    float4 emissive = gEmissive.Sample(gSampler, input.texCoord);
    float depth = gDepth.Sample(gSampler, input.texCoord).r;

    float3 albedo = albedoAO.rgb;
    float ao = albedoAO.a;
    float3 normal = normalize(normalRough.xyz);
    float roughness = normalRough.a;
    float metallic = metallicFlags.r;

    float3 worldPos = createWorldPosition(input.texCoord, depth, invViewProj);

    float3 viewDir = normalize(eyePosition - worldPos);

    float3 finalColor = ambientLight * albedo;

    finalColor += emissive.rgb;

    // Accumulate lights
    for (uint i = 0; i < numActiveLights; ++i) {
        float3 diffuse, specular;
        calculateSpotLight(spotLights[i], worldPos, normal, viewDir, albedo, metallic, roughness, diffuse, specular);
        finalColor += diffuse + specular;
    }

    finalColor = saturate(finalColor);


    return float4(finalColor, 1.0);
}
