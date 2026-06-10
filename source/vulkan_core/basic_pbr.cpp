//
// Created by 小叶 on 2026/6/5.
//
#include <print>
#include "../utility.h"
#include "basic_pbr.h"

namespace basic_pbr {

    VkDescriptorSetLayout create_set0_layout(const VkDevice device) { // NOLINT(*-misplaced-const)
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

        // binding 0: UBO (projection, view, model, camPos)
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[0].pImmutableSamplers = nullptr;

        // binding 1: LightUBO (灯光数据)
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VkDescriptorSetLayout layout;
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout);
        return layout;
    }

    VkDescriptorSetLayout create_set1_layout(const VkDevice device) {
        std::array<VkDescriptorSetLayoutBinding, 4> bindings{};

        // binding 0: albedoMap
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[0].pImmutableSamplers = nullptr;

        // binding 1: normalMap
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].pImmutableSamplers = nullptr;

        // binding 2: metallicRoughnessMap
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[2].pImmutableSamplers = nullptr;

        // binding 3: emissiveMap
        bindings[3].binding = 3;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[3].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VkDescriptorSetLayout layout;
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout);
        return layout;
    }

    void pbr_object::draw(const VkCommandBuffer cmd, const VkPipelineLayout pipeline_layout) const {
        const std::array<VkDescriptorSet, 2> sets = {this->set0, this->set1};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 2, sets.data(), 0, nullptr);
        vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MaterialPC), &this->material_pc);
        vkCmdBindVertexBuffers(cmd, 0, 1, &this->vertex_buffer, &this->vertex_buffer_offset);
        vkCmdBindIndexBuffer(cmd, this->index_buffer, this->index_buffer_offset, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, index_count, 1, 0, 0, 0);
    }
    void pbr_object::bind_set0(const VkDevice device) const {
        if (this->set0 == VK_NULL_HANDLE) {
            std::println(stderr, "ERROR: set0 is null!");
            print_stacktrace_and_terminate();
        }

        if (this->ubo_buffer == VK_NULL_HANDLE) {
            std::println(stderr, "ERROR: ubo_buffer is null!");
            print_stacktrace_and_terminate();
        }

        if (this->light_ubo_buffer == VK_NULL_HANDLE) {
            std::println(stderr, "ERROR: light_ubo_buffer is null!");
            print_stacktrace_and_terminate();
        }

        const VkDescriptorBufferInfo ubo_info = {this->ubo_buffer, this->ubo_offset, sizeof(UBO)};

        VkWriteDescriptorSet ubo_write = {};
        ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ubo_write.dstSet = this->set0;
        ubo_write.dstBinding = 0;
        ubo_write.dstArrayElement = 0;
        ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_write.descriptorCount = 1;
        ubo_write.pBufferInfo = &ubo_info;
        ubo_write.pNext = nullptr;

        const VkDescriptorBufferInfo light_ubo_info = {this->light_ubo_buffer, this->light_ubo_offset, sizeof(LightUBO)};
        VkWriteDescriptorSet light_ubo_write = {};
        light_ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        light_ubo_write.dstSet = this->set0;
        light_ubo_write.dstBinding = 1;
        light_ubo_write.dstArrayElement = 0;
        light_ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        light_ubo_write.descriptorCount = 1;
        light_ubo_write.pBufferInfo = &light_ubo_info;
        light_ubo_write.pNext = nullptr;

        const std::array<VkWriteDescriptorSet, 2> writes = {ubo_write, light_ubo_write};

        vkUpdateDescriptorSets(device, 2, writes.data(), 0, nullptr);

    }


    void pbr_object::bind_set1(VkDevice device) {

        if (!this->textures.contains("ALBEDO")) {
            std::println(stderr, "ALBEDO does not exist");
            print_stacktrace_and_terminate();
        }

        if (!this->textures.contains("NORMAL")) {
            std::println(stderr, "NORMAL does not exist");
            print_stacktrace_and_terminate();
        }

        if (!this->textures.contains("METALLIC_ROUGHNESS")) {
            std::println(stderr, "METALLIC_ROUGHNESS does not exist");
            print_stacktrace_and_terminate();
        }

        if (!this->textures.contains("EMISSIVE")) {
            std::println(stderr, "EMISSIVE does not exist");
            print_stacktrace_and_terminate();
        }

        std::array<std::string_view, 4> required_textures = {"ALBEDO", "NORMAL", "METALLIC_ROUGHNESS", "EMISSIVE"};

        for (const auto& texture : required_textures) {
            if (!this->textures.contains(texture)) {
                std::println(stderr, "{} does not exist", texture);
                print_stacktrace_and_terminate();
            }
        }

        const auto albedo = this->textures["ALBEDO"];
        VkDescriptorImageInfo albedo_image_info ={};
        albedo_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        albedo_image_info.imageView = albedo.image_view;
        albedo_image_info.sampler = albedo.sampler;

        VkWriteDescriptorSet albedo_write = {};
        albedo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        albedo_write.dstSet = this->set1;
        albedo_write.dstBinding = 0;
        albedo_write.dstArrayElement = 0;
        albedo_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;;
        albedo_write.descriptorCount = 1;
        albedo_write.pImageInfo = &albedo_image_info;
        albedo_write.pNext = nullptr;

        const auto normal = this->textures["NORMAL"];

        VkDescriptorImageInfo normal_image_info ={};
        normal_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        normal_image_info.imageView = normal.image_view;
        normal_image_info.sampler = normal.sampler;

        VkWriteDescriptorSet normal_write = {};
        normal_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        normal_write.dstSet = this->set1;
        normal_write.dstBinding = 1;
        normal_write.dstArrayElement = 0;
        normal_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;;
        normal_write.descriptorCount = 1;
        normal_write.pImageInfo = &normal_image_info;
        normal_write.pNext = nullptr;

        const auto metallic_roughness = this->textures["METALLIC_ROUGHNESS"];

        VkDescriptorImageInfo metallic_roughness_image_info ={};
        metallic_roughness_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        metallic_roughness_image_info.imageView = metallic_roughness.image_view;
        metallic_roughness_image_info.sampler = metallic_roughness.sampler;

        VkWriteDescriptorSet metallic_roughness_write = {};
        metallic_roughness_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        metallic_roughness_write.dstSet = this->set1;
        metallic_roughness_write.dstBinding = 2;
        metallic_roughness_write.dstArrayElement = 0;
        metallic_roughness_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;;
        metallic_roughness_write.descriptorCount = 1;
        metallic_roughness_write.pImageInfo = &metallic_roughness_image_info;
        metallic_roughness_write.pNext = nullptr;


        const auto emissive = this->textures["EMISSIVE"];

        VkDescriptorImageInfo emissive_image_info ={};
        emissive_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        emissive_image_info.imageView = emissive.image_view;
        emissive_image_info.sampler = emissive.sampler;

        VkWriteDescriptorSet emissive_write = {};
        emissive_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        emissive_write.dstSet = this->set1;
        emissive_write.dstBinding = 3;
        emissive_write.dstArrayElement = 0;
        emissive_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;;
        emissive_write.descriptorCount = 1;
        emissive_write.pImageInfo = &emissive_image_info;
        emissive_write.pNext = nullptr;

        std::array<VkWriteDescriptorSet, 4> writes = {albedo_write, normal_write, metallic_roughness_write, emissive_write};

        vkUpdateDescriptorSets(device, 4, writes.data(), 0, nullptr);
    }
} // base_pbr