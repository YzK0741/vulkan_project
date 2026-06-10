//
// Created by 小叶 on 2026/2/9.
//

#ifndef VULKAN_PROJECT_GLTF_PARSER_H
#define VULKAN_PROJECT_GLTF_PARSER_H

#include <future>
#include <string_view>
#include <map>
#include "../vulkan_runtime/module.h"

struct gltf_data {
    model_data model;
    std::vector<unsigned char> texture_data;
    int texture_width, texture_height;
    VkFormat texture_format;
};

// PBR 材质参数结构
struct PBR_material {
    // 基础颜色
    glm::vec4 base_color_factor = glm::vec4(1.0f);
    int base_color_texture = -1;

    // 金属粗糙度
    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
    int metallic_roughness_texture = -1;

    // 法线贴图
    int normal_texture = -1;
    float normal_scale = 1.0f;

    // 环境光遮蔽
    int occlusion_texture = -1;
    float occlusion_strength = 1.0f;

    // 自发光
    glm::vec3 emissive_factor = glm::vec3(0.0f);
    int emissive_texture = -1;

    // Alpha 模式
    enum AlphaMode { OPAQUE_, MASK, BLEND };
    AlphaMode alpha_mode = OPAQUE_;
    float alpha_cutoff = 0.5f;

    // 双面渲染
    bool double_sided = false;
};

struct basic_pbr_gltf_data {
    PBR_material material;
    pbr_model_data model;
    std::map<std::string, texture> textures;
};

void print_model_info(std::string_view path);

std::vector<gltf_data> load_gltf(std::string_view path);

std::future<std::vector<gltf_data>> load_gltf_async(std::string_view path);

basic_pbr_gltf_data load_gltf_pbr(std::string_view path);

std::future<basic_pbr_gltf_data> load_gltf_pbr_async(std::string_view path);

#endif //VULKAN_PROJECT_GLTF_PARSER_H