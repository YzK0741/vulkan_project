//
// Created by 小叶 on 2026/6/5.
//

#ifndef VULKAN_PROJECT_SHADERS_H
#define VULKAN_PROJECT_SHADERS_H

#include <span>

namespace shaders {
    std::span<const unsigned char> get_basic_vertex_shader_byte();
    std::span<const unsigned char> get_basic_fragment_shader_byte();
    std::span<const unsigned char> get_pbr_vertex_shader_byte();
    std::span<const unsigned char> get_pbr_fragment_shader_byte();
}


#endif //VULKAN_PROJECT_SHADERS_H
