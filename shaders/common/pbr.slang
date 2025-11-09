// PBR functions for the common module

implementing common;

public static const float PI = 3.14159265359;

public float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

public float distributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0001);
}

public float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0001);
}

public float geometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Cook Torrance BRDF
public void pbrBRDF(
    float3 N,
    float3 V,
    float3 L,
    float3 albedo,
    float metallic,
    float roughness,
    out float3 diffuse,
    out float3 specular
)
{
    float3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(V, N), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    float3 F = fresnelSchlick(HdotV, F0);

    // Specular BRDF
    float3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL;
    specular = numerator / max(denominator, 0.0001);

    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;

    kD *= (1.0 - metallic);

    diffuse = (kD * albedo) / PI;
}

// Convenience function for evaluating PBR with radiance
public float3 evaluatePBR(
    float3 N,
    float3 V,
    float3 L,
    float3 albedo,
    float metallic,
    float roughness,
    float3 radiance
)
{
    float3 diffuse, specular;
    pbrBRDF(N, V, L, albedo, metallic, roughness, diffuse, specular);

    float NdotL = max(dot(N, L), 0.0);
    return (diffuse + specular) * radiance * NdotL;
}
