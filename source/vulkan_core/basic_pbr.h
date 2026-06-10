//
// Created by 小叶 on 2026/6/5.
//

#ifndef VULKAN_PROJECT_BASE_PBR_H
#define VULKAN_PROJECT_BASE_PBR_H

#include <glm/glm.hpp>
#include <map>
#include <string_view>
#include <vulkan/vulkan.h>

namespace basic_pbr {
    struct vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec3 tangent;
    };
    constexpr std::array<VkVertexInputBindingDescription, 1> get_binding_descriptions() {
        std::array<VkVertexInputBindingDescription, 1> binding_descriptions{};

        // 只有一个绑定，所有顶点属性都打包在一个数组中
        binding_descriptions[0].binding = 0;                          // 绑定索引
        binding_descriptions[0].stride = sizeof(vertex);              // 每个顶点的大小
        binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // 每顶点数据

        return binding_descriptions;
    }

    constexpr std::array<VkVertexInputAttributeDescription, 4> get_attribute_descriptions(){
        std::array<VkVertexInputAttributeDescription, 4> attribute_descriptions{};

        // 位置属性 - location 0
        attribute_descriptions[0].binding = 0;
        attribute_descriptions[0].location = 0;                       // 着色器中的 location
        attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        attribute_descriptions[0].offset = offsetof(vertex, position); // 偏移量

        // 法线属性 - location 1
        attribute_descriptions[1].binding = 0;
        attribute_descriptions[1].location = 1;
        attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        attribute_descriptions[1].offset = offsetof(vertex, normal);

        // 纹理坐标属性 - location 2
        attribute_descriptions[2].binding = 0;
        attribute_descriptions[2].location = 2;
        attribute_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;    // vec2
        attribute_descriptions[2].offset = offsetof(vertex, uv);

        attribute_descriptions[3].binding = 0;
        attribute_descriptions[3].location = 3;
        attribute_descriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[3].offset = offsetof(vertex, tangent);

        return attribute_descriptions;
    }

    struct UBO {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 model;
        glm::vec3 camPos;
        uint8_t padding[2];
    };

    struct Light {
        glm::vec3 position;
        glm::vec3 color;
        float intensity;
    };

    // LightUBO (对应 set=0, binding=1)
    struct LightUBO {
        Light lights[4];
        int numLights;
        float padding[3];  // std140 对齐，int 需要补充到 16 字节
    };

    // Push Constant (对应 frag 中的 MaterialPC)
    struct MaterialPC {
        glm::vec4 baseColorFactor;  // 16 bytes
        float metallicFactor;       // 4 bytes
        float roughnessFactor;      // 4 bytes
        float padding[2];           // 8 bytes to align to 32
    };

    VkDescriptorSetLayout create_set0_layout(VkDevice device);
    VkDescriptorSetLayout create_set1_layout(VkDevice device);

    struct renderable_texture {
        long image_handler;
        VkBuffer image_buffer;
        VkImage image;
        VkDeviceSize offset;
        VkSampler sampler;
        VkImageView image_view;
        VkDeviceMemory device_memory;
        VkDeviceSize size;
        uint32_t width, height;
    };

    struct pbr_object {
        VkBuffer vertex_buffer = VK_NULL_HANDLE;
        VkDeviceSize vertex_buffer_offset;
        VkBuffer index_buffer = VK_NULL_HANDLE;
        VkDeviceSize index_buffer_offset;
        uint32_t index_count;
        std::map<std::string_view, renderable_texture> textures;
        UBO ubo;
        VkBuffer ubo_buffer = VK_NULL_HANDLE;
        uint32_t ubo_offset;
        LightUBO light_ubo;
        VkBuffer light_ubo_buffer = VK_NULL_HANDLE;
        uint32_t light_ubo_offset;
        MaterialPC material_pc;
        VkDescriptorSet set0;
        VkDescriptorSet set1;

        void draw(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout) const;
        void bind_set0(VkDevice device) const;
        void bind_set1(VkDevice device);
    };



} // base_pbr

struct texture {
    std::vector<unsigned char> data;
    int width, height;
    VkFormat format;
};

#endif //VULKAN_PROJECT_BASE_PBR_H
