// 片段着色器 - fragment_shader.frag
#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
     vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
     vec3 normal = normalize(fragNormal);
     float diff = max(dot(normal, lightDir), 0.0);

     vec3 ambient = 0.2 * vec3(1.0, 1.0, 1.0);
     vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);

     vec3 color = ambient + diffuse;
     vec4 texColor = texture(texSampler, fragTexCoord);

     // 如果纹理存在，使用纹理颜色，否则使用白色
     outColor = texture(texSampler, fragTexCoord);
}