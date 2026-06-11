//
// Created by 小叶 on 2026/6/5.
//

#include "shaders.h"

#include "../../shaders/basic_vertex_byte.bin"

std::span<const unsigned char> shaders::get_basic_vertex_shader_byte() {
    return {reinterpret_cast<const unsigned char*>(__base_vert_spv), __base_vert_spv_len};
}

#include "../../shaders/basic_fragment_byte.h"

std::span<const unsigned char> shaders::get_basic_fragment_shader_byte() {
    return {reinterpret_cast<const unsigned char*>(__base_farg_spv), __base_farg_spv_len};
}

#include "../../shaders/pbr_vertex_byte.bin"

std::span<const unsigned char> shaders::get_pbr_vertex_shader_byte() {
    return {reinterpret_cast<const unsigned char*>(pbr_vert_spv), pbr_vert_spv_len};
}

#include "../../shaders/pbr_fragment_byte.bin"

std::span<const unsigned char> shaders::get_pbr_fragment_shader_byte() {
    return {reinterpret_cast<const unsigned char*>(pbr_frag_spv), pbr_frag_spv_len};
}
