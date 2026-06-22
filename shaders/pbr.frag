#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in mat3 inTBN;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 projection;
    mat4 view;
    mat4 model;
    vec3 camPos;
} ubo;

layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessMap;
layout(set = 1, binding = 3) uniform sampler2D aoMap;

struct Light {
    vec3 position;
    vec3 color;
    float intensity;
};

layout(set = 0, binding = 1) uniform LightUBO {
    Light lights[4];
    int numLights;
} lightData;

layout(push_constant) uniform MaterialPC {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
} material;

// 常量
const float PI = 3.14159265359;

// ----------------------------------------------------------------------------
// 法线分布函数 (GGX/Trowbridge-Reitz)
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// ----------------------------------------------------------------------------
// 几何遮蔽函数 (Smith-Schlick-GGX)
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// ----------------------------------------------------------------------------
// 菲涅尔方程 (Schlick 近似)
// ----------------------------------------------------------------------------
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// ----------------------------------------------------------------------------
// 主要 PBR 计算函数
// ----------------------------------------------------------------------------
vec3 CalculatePBR(vec3 albedo, float metallic, float roughness, float ao,
                  vec3 N, vec3 V, vec3 L, vec3 lightColor, float lightIntensity)
{
    vec3 H = normalize(V + L);
    vec3 radiance = lightColor * lightIntensity;

    // 基础反射率 - 非金属为 0.04，金属使用 albedo
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    vec3 specular = numerator / max(denominator, 0.001);

    // 漫反射部分
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * radiance * NdotL;
}

void main()
{
    // 采样纹理
    vec4 albedo = texture(albedoMap, inUV) * material.baseColorFactor;
    vec3 normalTex = texture(normalMap, inUV).rgb;
    vec3 mrSample = texture(metallicRoughnessMap, inUV).rgb;
    float ao = texture(aoMap, inUV).r;

    float metallic = mrSample.b * material.metallicFactor;  // 注意：不同格式可能通道不同
    float roughness = mrSample.g * material.roughnessFactor;

    // 法线贴图转换到世界空间
    vec3 N = normalize(inNormal);
    vec3 T = normalize(inTBN[0]);
    vec3 B = normalize(inTBN[1]);
    vec3 normal = normalize(normalTex * 2.0 - 1.0);
    vec3 worldNormal = normalize(T * normal.x + B * normal.y + N * normal.z);

    vec3 V = normalize(ubo.camPos - inWorldPos);

    // 初始化输出颜色
    vec3 Lo = vec3(0.0);


    Light light;

    light.intensity = 2.0;
    light.position = vec3(0.0, 2.0, 0.0);
    light.color = vec3(1.0);

    vec3 L = normalize(light.position - inWorldPos);

    // 距离衰减
    float distance = length(light.position - inWorldPos);
    float attenuation = 1.0 / (distance * distance);

    // 计算 PBR 光照
    Lo += CalculatePBR(albedo.rgb, metallic, roughness, ao,
                       worldNormal, V, L,
                       light.color, light.intensity * attenuation);



    // 环境光照 (简化的 IBL)
    vec3 ambient = albedo.rgb * 0.05 * ao;

    vec3 color = ambient + Lo;

    // HDR 色调映射
    color = color / (color + vec3(1.0));

    // Gamma 校正
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, albedo.a);
}