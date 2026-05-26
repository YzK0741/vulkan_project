//
// Created by 小叶 on 2026/5/19.
//

#include "vulkan_runtime.h"


VkSampler create_sampler(const VkDevice& device) {
    VkSampler sampler;

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;

    if (vkCreateSampler(device, &sampler_info, nullptr, &sampler) != VK_SUCCESS) {
        std::println("无法创建纹理采样器!");
        print_stacktrace_and_terminate();
    }

    return sampler;
}


namespace vulkan_runtime {
    vulkan_texture runtime::create_texture(const stb_texture &texture) {

        VkBufferCreateInfo buffer_create_info{};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size = texture.image_size;
        buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        auto handler = this->core.vma.create_buffer(buffer_create_info); // NOLINT(*-misplaced-const)

        auto waiter = this->core.vma.update_to_buffer(
            texture.pixels,
            handler,
            texture.image_size,
            0
        );

        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent = {
            static_cast<uint32_t>(texture.width),
            static_cast<uint32_t>(texture.height),
            1
        };
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        const auto image_handler = this->core.vma.create_image(image_info);

        const auto&[image, image_allocation] = this->core.vma.get_image(image_handler); // NOLINT(*-misplaced-const)

        waiter.wait();

        transition_image_layout(
            this->core.device,
            this->core.command_pool,
            this->core.graphics_queue,
            image,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );

        VkImageView image_view = create_image_view(
                this->core.device,
                image,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_ASPECT_COLOR_BIT
                );

        const VkSampler sampler = create_sampler(this->core.device); // NOLINT(*-misplaced-const)

        VkDeviceMemory image_memory = this->core.vma.get_device_memory(image_allocation);

        return {
            image,
            image_memory,
            image_view,
            sampler,
            static_cast<uint32_t>(texture.width),
            static_cast<uint32_t>(texture.height)
        };
    }

    vulkan_texture runtime::create_texture(const std::vector<unsigned char> &texture, const int width, const int height, const VkFormat format) {

        VkBufferCreateInfo buffer_create_info{};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size = texture.size();
        buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        const auto handler = this->core.vma.create_buffer(buffer_create_info); // NOLINT(*-misplaced-const)
        const auto& [buffer, allocation] = core.vma.get_buffer(handler); // NOLINT(*-misplaced-const)

        auto waiter = this->core.vma.update_to_buffer(
            texture.data(),
            handler,
            texture.size(),
            0
        );

        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            1
        };
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = format;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        const auto image_handler = this->core.vma.create_image(image_info);

        const auto&[image, image_allocation] = this->core.vma.get_image(image_handler); // NOLINT(*-misplaced-const

        waiter.wait();

        transition_image_layout(
            this->core.device,
            this->core.command_pool,
            this->core.graphics_queue,
            image,
            format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );

        copy_buffer_to_image(
                core.device,
                core.command_pool,
                core.graphics_queue,
                buffer,
                image,
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
                );

        transition_image_layout(
                core.device,
                core.command_pool,
                core.graphics_queue,
                image,
                format,  // 根据实际格式调整
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

        const VkImageView& image_view = create_image_view(
                this->core.device,
                image,
                format,
                VK_IMAGE_ASPECT_COLOR_BIT
                );

        const VkSampler sampler = create_sampler(this->core.device); // NOLINT(*-misplaced-const)

        VkDeviceMemory image_memory = this->core.vma.get_device_memory(image_allocation);

        return {
            image,
            image_memory,
            image_view,
            sampler,
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };
    }
}
