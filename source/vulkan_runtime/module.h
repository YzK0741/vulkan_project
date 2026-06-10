//
// Created by 小叶 on 2026/2/22.
//

#ifndef VULKAN_PROJECT_MOUDLE_H
#define VULKAN_PROJECT_MOUDLE_H
#include <vector>
#include "../vulkan_core/vulkan_utility.h"
#include "../vulkan_core/basic_pbr.h"

struct model_data {
    std::vector<vertex> vertices;
    std::vector<uint32_t> indices;

    model_data() = default;

    void clear() {
        vertices.clear();
        indices.clear();
    }

    [[nodiscard]] size_t vertex_count() const { return vertices.size(); }
    [[nodiscard]] size_t index_count() const { return indices.size(); }
};

struct pbr_model_data {
    std::vector<basic_pbr::vertex> vertices;
    std::vector<uint32_t> indices;

    pbr_model_data() = default;

    void clear() {
        vertices.clear();
        indices.clear();
    }

    [[nodiscard]] size_t vertex_count() const { return vertices.size(); }
    [[nodiscard]] size_t index_count() const { return indices.size(); }
};

#endif //VULKAN_PROJECT_MOUDLE_H