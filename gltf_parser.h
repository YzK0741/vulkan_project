//
// Created by 小叶 on 2026/2/9.
//

#ifndef VULKAN_PROJECT_GLTF_PARSER_H
#define VULKAN_PROJECT_GLTF_PARSER_H

#include <string_view>
#include "module.h"

struct gltf_data {
    model_data model;
    std::vector<unsigned char> texture_data;
    int texture_width, texture_height;
    VkFormat texture_format;
};

std::vector<gltf_data> load_gltf(std::string_view path);

#endif //VULKAN_PROJECT_GLTF_PARSER_H