//
// Created by 小叶 on 2026/6/5.
//

#ifndef VULKAN_PROJECT_BASE_PBR_H
#define VULKAN_PROJECT_BASE_PBR_H

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <map>
#include <string>

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
        float padding[2];           // 8 bytes to align to 32?
    };

    VkDescriptorSetLayout create_set0_layout(VkDevice device);
    VkDescriptorSetLayout create_set1_layout(VkDevice device);

    struct texture {
        long image_handler;
        VkImageView image_view;
        VkSampler sampler;
    };

    struct renderable_object {
        VkDescriptorSet set0;
        VkDescriptorSet set1;
        std::map<std::string, texture> textures;
        VkBuffer vertex_buffer;
        VkBuffer index_buffer;
        uint32_t index_count;
        MaterialPC material_pc;
        UBO ubo;
        VkBuffer ubo_buffer;
        LightUBO light_ubo;
        VkBuffer light_ubo_buffer;
        void draw(const VkCommandBuffer cmd,
          const VkPipelineLayout pipeline_layout,
          const VkShaderStageFlags push_constant_stages = VK_SHADER_STAGE_FRAGMENT_BIT) const {
            // 验证
            if (index_count == 0 || vertex_buffer == VK_NULL_HANDLE) return;

            const std::array<VkDescriptorSet, 2> sets = {set0, set1};
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline_layout, 0, 2, sets.data(), 0, nullptr);
            vkCmdPushConstants(cmd, pipeline_layout,
                              push_constant_stages,
                              0, sizeof(material_pc), &material_pc);

            constexpr VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, offsets);
            vkCmdBindIndexBuffer(cmd, index_buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, index_count, 1, 0, 0, 0);
        }

        void bind_set0(const VkDevice device, const VkDescriptorPool descriptor_pool,
                       const VkDescriptorSetLayout set0_layout) {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = descriptor_pool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &set0_layout;

            vkAllocateDescriptorSets(device, &allocInfo, &this->set0);

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

            // UBO buffer (binding 0)
            VkDescriptorBufferInfo uboBufferInfo{};
            uboBufferInfo.buffer = ubo_buffer;
            uboBufferInfo.range = VK_WHOLE_SIZE;
            uboBufferInfo.offset = 0;

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = this->set0;
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &uboBufferInfo;

            // Light UBO buffer (binding 1)
            VkDescriptorBufferInfo lightBufferInfo{};
            lightBufferInfo.buffer = light_ubo_buffer;
            lightBufferInfo.range = VK_WHOLE_SIZE;
            lightBufferInfo.offset = 0;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = this->set0;
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo = &lightBufferInfo;

            vkUpdateDescriptorSets(device, descriptorWrites.size(),
                                   descriptorWrites.data(), 0, nullptr);
        }

        void bind_set1(const VkDevice device, const VkDescriptorPool descriptor_pool,
                       const VkDescriptorSetLayout set1_layout) {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = descriptor_pool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &set1_layout;

            vkAllocateDescriptorSets(device, &allocInfo, &set1);
            // 获取纹理
            const auto& albedo_tex = textures.at("ALBEDO");
            const auto& normal_tex = textures.at("NORMAL");
            const auto& roughness_tex = textures.at("ROUGHNESS");
            const auto& occlusion_tex = textures.at("OCCLUSION");

            std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

            // Albedo Map (binding 0)
            VkDescriptorImageInfo albedoImageInfo{};
            albedoImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            albedoImageInfo.imageView = albedo_tex.image_view;
            albedoImageInfo.sampler = albedo_tex.sampler;

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = set1;
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &albedoImageInfo;

            // Normal Map (binding 1)
            VkDescriptorImageInfo normalImageInfo{};
            normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            normalImageInfo.imageView = normal_tex.image_view;
            normalImageInfo.sampler = normal_tex.sampler;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = set1;
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &normalImageInfo;

            // MetallicRoughness Map (binding 2)
            VkDescriptorImageInfo roughnessImageInfo{};
            roughnessImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            roughnessImageInfo.imageView = roughness_tex.image_view;
            roughnessImageInfo.sampler = roughness_tex.sampler;

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = set1;
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pImageInfo = &roughnessImageInfo;

            // AO Map (binding 3)
            VkDescriptorImageInfo aoImageInfo{};
            aoImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            aoImageInfo.imageView = occlusion_tex.image_view;
            aoImageInfo.sampler = occlusion_tex.sampler;

            descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[3].dstSet = set1;
            descriptorWrites[3].dstBinding = 3;
            descriptorWrites[3].dstArrayElement = 0;
            descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[3].descriptorCount = 1;
            descriptorWrites[3].pImageInfo = &aoImageInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()),
                                   descriptorWrites.data(), 0, nullptr);
        }

        // 便捷函数：同时绑定 set0 和 set1
        void bind_descriptor_sets(const VkDevice device, const VkDescriptorPool descriptor_pool,
                                  const VkDescriptorSetLayout set0_layout,
                                  const VkDescriptorSetLayout set1_layout) {
            bind_set0(device, descriptor_pool, set0_layout);
            bind_set1(device, descriptor_pool, set1_layout);
        }

    };

} // base_pbr

#endif //VULKAN_PROJECT_BASE_PBR_H
