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

struct pbr_gltf_data {
    pbr_model_data model;
    std::map<std::string, pbr_texture> textures;
    glm::vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
};

void print_model_info(std::string_view path);

std::vector<gltf_data> load_gltf(std::string_view path);

std::future<std::vector<gltf_data>> load_gltf_async(std::string_view path);

std::vector<pbr_gltf_data> load_pbr_gltf(std::string_view path);
std::future<std::vector<pbr_gltf_data>> load_pbr_gltf_async(std::string_view path);

#endif //VULKAN_PROJECT_GLTF_PARSER_H