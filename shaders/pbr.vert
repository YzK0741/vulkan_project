#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outWorldPos;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out mat3 outTBN;

layout(set = 0, binding = 0) uniform UBO {
    mat4 projection;
    mat4 view;
    vec3 camPos;
} ubo;

layout(push_constant) uniform PushConsts {
    mat4 model;
} pushConsts;

void main() {
    // 传递 UV 坐标
    outUV = inUV;

    // 计算世界坐标位置
    vec4 worldPos = pushConsts.model * vec4(inPos, 1.0);
    outWorldPos = worldPos.xyz;

    // 转换法线到世界空间
    // 使用逆转置矩阵来正确变换法线（处理非均匀缩放）
    // 由于我们假设 model 矩阵是均匀缩放的，可以直接使用 model 矩阵
    vec4 worldNormal = pushConsts.model * vec4(inNormal, 0.0);
    outNormal = normalize(worldNormal.xyz);

    // 构建 TBN 矩阵
    vec3 T = normalize((pushConsts.model * vec4(inTangent, 0.0)).xyz);
    vec3 N = normalize(outNormal);
    // 重新计算副法线以确保正交性
    vec3 B = normalize(cross(N, T));
    // 如果需要处理镜像 UV，可以调整副法线方向
    // B = B * (dot(cross(N, T), B) < 0.0 ? -1.0 : 1.0);

    outTBN = mat3(T, B, N);

    // 计算裁剪空间位置
    gl_Position = ubo.projection * ubo.view * worldPos;
}