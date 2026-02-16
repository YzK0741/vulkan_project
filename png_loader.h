//
// Created by 小叶 on 2026/1/25.
//

#ifndef VULKAN_PROJECT_PNG_LOADER_H
#define VULKAN_PROJECT_PNG_LOADER_H

//
// PNG纹理加载器实现
//

#include <string>
#include <stdexcept>
#include <iostream>
#include <vulkan/vulkan.h>
#include <stb/stb_image.h>
#include "vulkan_buffer.h"
// PNG纹理数据结构
struct png_texture_data {
    int width;
    int height;
    int channels;
    VkDeviceSize image_size;
    stbi_uc* pixel_data;

    png_texture_data() : width(0), height(0), channels(0), image_size(0), pixel_data(nullptr) {}

    ~png_texture_data() {
        if (pixel_data) {
            stbi_image_free(pixel_data);
            pixel_data = nullptr;
        }
    }

    [[nodiscard]] bool is_valid() const {
        return pixel_data != nullptr && width > 0 && height > 0;
    }

    // 获取Vulkan格式
    [[nodiscard]] VkFormat get_vk_format() const {
        switch (channels) {
            case 1: return VK_FORMAT_R8_UNORM;           // 单通道
            case 2: return VK_FORMAT_R8G8_UNORM;         // RG双通道
            case 3: return VK_FORMAT_R8G8B8_UNORM;       // RGB三通道（需要扩展）
            case 4: return VK_FORMAT_R8G8B8A8_UNORM;     // RGBA四通道
            default: return VK_FORMAT_UNDEFINED;
        }
    }
};

// PNG纹理加载器类
class png_texture_loader {
public:
    // 从PNG文件加载纹理数据
    static png_texture_data load_png_texture(const std::string& filepath) {
        png_texture_data texture;

        // 设置STB图像加载选项
        stbi_set_flip_vertically_on_load(1); // 翻转Y轴，使纹理坐标符合Vulkan约定

        // 加载PNG图像
        texture.pixel_data = stbi_load(
            filepath.c_str(),
            &texture.width,
            &texture.height,
            &texture.channels,
            4  // 0表示使用原始通道数
        );

        texture.channels = 4;

        if (!texture.pixel_data) {
            std::println("无法加载PNG纹理文件: {}", filepath);
            print_stacktrace_and_terminate();
        }

        if (texture.width <= 0 || texture.height <= 0) {
            std::println("PNG纹理尺寸无效: {}x{}", std::to_string(texture.width),std::to_string(texture.height));
            print_stacktrace_and_terminate();
        }

        // 计算图像大小
        texture.image_size = texture.width * texture.height * texture.channels;

        std::cout << "PNG纹理加载成功:" << std::endl;
        std::cout << "  路径: " << filepath << std::endl;
        std::cout << "  尺寸: " << texture.width << "x" << texture.height << std::endl;
        std::cout << "  通道数: " << texture.channels << std::endl;
        std::cout << "  数据大小: " << texture.image_size << " bytes" << std::endl;
        std::cout << "  Vulkan格式: " << get_format_string(texture.get_vk_format()) << std::endl;

        return texture;
    }

    // 从内存加载PNG纹理
    static png_texture_data load_png_texture_from_memory(const void* data, size_t size) {
        png_texture_data texture;

        stbi_set_flip_vertically_on_load(1); // 翻转Y轴

        // 从内存加载
        texture.pixel_data = stbi_load_from_memory(
            static_cast<const stbi_uc*>(data),
            static_cast<int>(size),
            &texture.width,
            &texture.height,
            &texture.channels,
            0
        );

        if (!texture.pixel_data) {
            std::println("无法从内存加载PNG纹理");
            print_stacktrace_and_terminate();
        }

        if (texture.width <= 0 || texture.height <= 0) {
            std::println("PNG纹理尺寸无效: {}x{}", std::to_string(texture.width), std::to_string(texture.height));
            print_stacktrace_and_terminate();
        }

        texture.image_size = texture.width * texture.height * texture.channels;

        std::cout << "从内存加载PNG纹理成功:" << std::endl;
        std::cout << "  尺寸: " << texture.width << "x" << texture.height << std::endl;
        std::cout << "  通道数: " << texture.channels << std::endl;
        std::cout << "  数据大小: " << texture.image_size << " bytes" << std::endl;

        return texture;
    }

    // 创建Vulkan图像资源
    static void create_vulkan_texture(
        VkDevice device,
        VkPhysicalDevice physical_device,
        VkCommandPool command_pool,
        VkQueue graphics_queue,
        const png_texture_data& png_texture,
        VkImage& texture_image,
        VkDeviceMemory& texture_image_memory,
        VkImageView& texture_image_view,
        VkSampler& texture_sampler
    ) {
        if (!png_texture.is_valid()) {
            std::println("PNG纹理数据无效，无法创建Vulkan纹理");
            print_stacktrace_and_terminate();
        }

        // 1. 创建临时缓冲区来传输纹理数据
        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;

        create_buffer(
            device,
            physical_device,
            png_texture.image_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            staging_buffer_memory
        );

        // 2. 复制纹理数据到暂存缓冲区
        void* data;
        vkMapMemory(device, staging_buffer_memory, 0, png_texture.image_size, 0, &data);
        memcpy(data, png_texture.pixel_data, static_cast<size_t>(png_texture.image_size));
        vkUnmapMemory(device, staging_buffer_memory);

        // 3. 创建纹理图像
        create_texture_image(
            device,
            physical_device,
            command_pool,
            graphics_queue,
            png_texture.width,
            png_texture.height,
            png_texture.get_vk_format(),
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            texture_image,
            texture_image_memory
        );

        // 4. 从暂存缓冲区复制到纹理图像
        transition_image_layout(
            device,
            command_pool,
            graphics_queue,
            texture_image,
            png_texture.get_vk_format(),  // 根据实际格式调整
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );

        copy_buffer_to_image(
            device,
            command_pool,
            graphics_queue,
            staging_buffer,
            texture_image,
            static_cast<uint32_t>(png_texture.width),
            static_cast<uint32_t>(png_texture.height)
        );

        // 5. 转换纹理图像到着色器可读状态
        transition_image_layout(
            device,
            command_pool,
            graphics_queue,
            texture_image,
            png_texture.get_vk_format(),  // 根据实际格式调整
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        // 6. 清理暂存缓冲区
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_buffer_memory, nullptr);

        // 7. 创建图像视图
        texture_image_view = create_image_view(
            device,
            texture_image,
            png_texture.get_vk_format(),
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        // 8. 创建纹理采样器
        create_texture_sampler(device, texture_sampler);
    }

private:

    // 创建纹理图像
    static void create_texture_image(
        VkDevice device,
        VkPhysicalDevice physical_device,
        VkCommandPool command_pool,
        VkQueue graphics_queue,
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& image_memory
    ) {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = width;
        image_info.extent.height = height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = format;
        image_info.tiling = tiling;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = usage;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &image_info, nullptr, &image) != VK_SUCCESS) {
            std::println("无法创建纹理图像!");
            print_stacktrace_and_terminate();
        }

        VkMemoryRequirements mem_requirements;
        vkGetImageMemoryRequirements(device, image, &mem_requirements);

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_requirements.size;
        alloc_info.memoryTypeIndex = find_memory_type(
            physical_device,
            mem_requirements.memoryTypeBits,
            properties
        );

        if (vkAllocateMemory(device, &alloc_info, nullptr, &image_memory) != VK_SUCCESS) {
            std::println("无法分配纹理图像内存!");
            print_stacktrace_and_terminate();
        }

        vkBindImageMemory(device, image, image_memory, 0);
    }

    // 转换图像布局
    static void transition_image_layout(
        VkDevice device,
        VkCommandPool command_pool,
        VkQueue graphics_queue,
        VkImage image,
        VkFormat format,
        VkImageLayout old_layout,
        VkImageLayout new_layout
    ) {
        VkCommandBuffer command_buffer = begin_single_time_commands(device, command_pool);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;

        if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        } else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags source_stage;
        VkPipelineStageFlags destination_stage;

        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            std::println("不支持的图像布局转换!");
            print_stacktrace_and_terminate();
        }

        vkCmdPipelineBarrier(
            command_buffer,
            source_stage, destination_stage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        end_single_time_commands(device, command_pool, graphics_queue, command_buffer);
    }

    // 复制缓冲区到图像
    static void copy_buffer_to_image(
        VkDevice device,
        VkCommandPool command_pool,
        VkQueue graphics_queue,
        VkBuffer buffer,
        VkImage image,
        uint32_t width,
        uint32_t height
    ) {
        VkCommandBuffer command_buffer = begin_single_time_commands(device, command_pool);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage(
            command_buffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        end_single_time_commands(device, command_pool, graphics_queue, command_buffer);
    }

    // 开始单次命令缓冲区
    static VkCommandBuffer begin_single_time_commands(VkDevice device, VkCommandPool command_pool) {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = command_pool;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer;
        vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(command_buffer, &begin_info);

        return command_buffer;
    }

    // 结束单次命令缓冲区
    static void end_single_time_commands(
        VkDevice device,
        VkCommandPool command_pool,
        VkQueue graphics_queue,
        VkCommandBuffer command_buffer
    ) {
        vkEndCommandBuffer(command_buffer);

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphics_queue);

        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    }

    // 创建图像视图
    static VkImageView create_image_view(
        VkDevice device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect_flags
    ) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format;
        view_info.subresourceRange.aspectMask = aspect_flags;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VkImageView image_view;
        if (vkCreateImageView(device, &view_info, nullptr, &image_view) != VK_SUCCESS) {
            std::println("无法创建纹理图像视图!");
            print_stacktrace_and_terminate();
        }

        return image_view;
    }

    // 创建纹理采样器
    static void create_texture_sampler(VkDevice device, VkSampler& texture_sampler) {
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

        if (vkCreateSampler(device, &sampler_info, nullptr, &texture_sampler) != VK_SUCCESS) {
            std::println("无法创建纹理采样器!");
            print_stacktrace_and_terminate();
        }
    }

    // 获取格式字符串（用于调试输出）
    static std::string get_format_string(VkFormat format) {
        switch (format) {
            case VK_FORMAT_R8_UNORM: return "VK_FORMAT_R8_UNORM";
            case VK_FORMAT_R8G8_UNORM: return "VK_FORMAT_R8G8_UNORM";
            case VK_FORMAT_R8G8B8_UNORM: return "VK_FORMAT_R8G8B8_UNORM";
            case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
            default: return "VK_FORMAT_UNDEFINED";
        }
    }
};


#endif //VULKAN_PROJECT_PNG_LOADER_H