//
// Created by 小叶 on 2026/5/18.
//

#ifndef VULKAN_PROJECT_VMA_H
#define VULKAN_PROJECT_VMA_H

#include <vma/vk_mem_alloc.h>
#include <span>
#include "../vk_format.h"
#include "vma_waiter.h"
#include "../../utility.h"

struct vma_buffer {
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct vma_image {
    VkImage image;
    VmaAllocation allocation;
};

class VMA : enable_handler_distribute<VMA> {
public:
    enum class buffer_type {
    vertex,              // GPU_ONLY, 需要 staging buffer
    index,               // GPU_ONLY, 需要 staging buffer
    uniform_gpu_only,    // GPU_ONLY, 适合不需要频繁更新的 uniform
    uniform_coherent,    // HOST_VISIBLE | HOST_COHERENT, 适合每帧更新的 uniform
    uniform_cached       // HOST_VISIBLE | HOST_CACHED, 适合 read-back
    };

    enum class image_type {
        texture_2d,           // GPU_ONLY, 普通纹理，需要 staging
        texture_2d_color,     // GPU_ONLY, 带颜色格式的纹理
        texture_2d_depth,     // GPU_ONLY, 深度纹理
        texture_2d_staging,   // HOST_VISIBLE, 用于动态更新的纹理
        texture_cubemap,      // GPU_ONLY, 立方体贴图
        render_target         // GPU_ONLY, 渲染目标 (可读写)
    };

    // Image 的辅助结构
    struct image_info {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mip_levels = 1;
        uint32_t array_layers = 1;
        VkFormat format = {};
        VkImageUsageFlags extra_usage = 0;
    };
    
private:

    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    std::mutex mutex;
    std::unordered_map<handler,vma_buffer> buffers;
    std::unordered_map<handler,vma_image> images;


    bool direct_upload(VkBuffer buffer, VmaAllocation allocation, const void* data, VkDeviceSize size) const;

    bool staging_upload(VkBuffer dst_buffer, const void* data, VkDeviceSize size);

    bool direct_image_upload(VkImage image, VmaAllocation allocation, const void* data, VkDeviceSize size) const;

    bool staging_image_upload(VkImage dst_image, const void* data, VkDeviceSize size, const image_info& info);


    void do_upload_to_buffer(const VkBuffer &staging_buffer, const VmaAllocation &staging_buffer_allocation,
                             const void *source,
                             const VkBuffer &destination,
                             const VkCommandBuffer &command_buffer,
                             const VkFence& fence,
                             VkDeviceSize size,
                             VkDeviceSize src_offset,
                             VkDeviceSize dst_offset
                             );

    [[nodiscard]] VkFence create_fence() const;




public:
    static constexpr handler invalid_handler = 0;

    static constexpr VmaAllocationCreateInfo get_allocation_info_from_type(const buffer_type type) {
        VmaAllocationCreateInfo info = {};
        switch (type) {
            case buffer_type::vertex:
                [[fallthrough]];
            case buffer_type::index:
                [[fallthrough]];
            case buffer_type::uniform_gpu_only: {
                info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
                break;
            }
            case buffer_type::uniform_coherent: {
                info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
                info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                             VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
                break;
            }
            case buffer_type::uniform_cached: {
                info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
                info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                             VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
                break;
            }
        }
        return info;
    }

    static constexpr VkBufferCreateInfo get_create_info_from_type(const buffer_type type) {
        VkBufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

        switch (type) {
            case buffer_type::vertex: {
                info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                break;
            }
            case buffer_type::index: {
                info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                break;
            }
            case buffer_type::uniform_gpu_only: {
                info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                break;
            }
            case buffer_type::uniform_coherent:
            case buffer_type::uniform_cached: {
                info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                break;
            }
        }
        return info;
    }

    static constexpr VmaAllocationCreateInfo get_image_allocation_info_from_type(const image_type type) {
        VmaAllocationCreateInfo info = {};

        switch (type) {
            case image_type::texture_2d:
            case image_type::texture_2d_color:
            case image_type::texture_2d_depth:
            case image_type::texture_cubemap:
            case image_type::render_target: {
                info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
                break;
            }
            case image_type::texture_2d_staging: {
                info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
                info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                             VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
                break;
            }
        }
        return info;
    }

    static constexpr VkImageCreateInfo get_image_create_info_from_type(
        const image_type type,
        const image_info& info
    ) {
        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.extent.width = info.width;
        image_info.extent.height = info.height;
        image_info.extent.depth = 1;
        image_info.mipLevels = info.mip_levels;
        image_info.arrayLayers = info.array_layers;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.format = info.format;

        switch (type) {
            case image_type::texture_2d:
            case image_type::texture_2d_staging:
                image_info.imageType = VK_IMAGE_TYPE_2D;
                image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                   VK_IMAGE_USAGE_SAMPLED_BIT |
                                   info.extra_usage;
                break;

            case image_type::texture_2d_color:
                image_info.imageType = VK_IMAGE_TYPE_2D;
                image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                   VK_IMAGE_USAGE_SAMPLED_BIT |
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   info.extra_usage;
                break;

            case image_type::texture_2d_depth:
                image_info.imageType = VK_IMAGE_TYPE_2D;
                image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                   info.extra_usage;
                break;

            case image_type::texture_cubemap:
                image_info.imageType = VK_IMAGE_TYPE_2D;
                image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
                image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                   VK_IMAGE_USAGE_SAMPLED_BIT |
                                   info.extra_usage;
                break;

            case image_type::render_target:
                image_info.imageType = VK_IMAGE_TYPE_2D;
                image_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                   VK_IMAGE_USAGE_SAMPLED_BIT |
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   info.extra_usage;
                break;
        }

        return image_info;
    }


    void init(
        const VkInstance &instance,
        const VkDevice &vulkan_device,
        const VkPhysicalDevice &physical_device,
        const VkQueue& vulkan_queue,
        uint32_t queue_family_index
        );

    void destroy() const;

    [[nodiscard]] handler create_buffer(const VkBufferCreateInfo &buffer_create_info);

    [[nodiscard]] const vma_buffer& get_buffer(handler buffer_handle);

    void destroy_buffer(long buffer_handler);

    [[nodiscard]] handler create_image(const VkImageCreateInfo &image_create_info);
    [[nodiscard]] const vma_image &get_image(const handler &image_handle);

    void destroy_image(const long &image_handler);

    // 创建并填充数据（自动选择最佳上传方式）
    template <typename data_element_type>
    handler create_buffer_with_data(std::span<data_element_type> datas, const buffer_type type) {
        if (datas.empty()) {
            std::println("Cannot create buffer with empty data");
            return invalid_handler;
        }

        if (auto result = this->distribute_handler(); result.has_value()) {
            const auto handler_value = result.value();
            const auto buffer_size = datas.size() * sizeof(data_element_type);

            const auto allocation_create_info = get_allocation_info_from_type(type);
            auto buffer_create_info = get_create_info_from_type(type);
            buffer_create_info.size = buffer_size;

            VmaAllocation allocation = VK_NULL_HANDLE;
            VkBuffer buffer = VK_NULL_HANDLE;
            VmaAllocationInfo alloc_info = {};

            const VkResult vk_result = vmaCreateBuffer(
                this->allocator,
                &buffer_create_info,
                &allocation_create_info,
                &buffer,
                &allocation,
                &alloc_info
            );

            if (vk_result != VK_SUCCESS) {
                std::println("Failed to create buffer: {}", static_cast<int>(vk_result));
                if (auto expected = this->recycle_handler(handler_value); !expected) {
                    std::println(stderr, "vma handler distribute error: {}", expected.error());
                }
                return invalid_handler;
            }

            // 根据 buffer 类型选择上传方式
            bool upload_success = false;
            switch (type) {
                case buffer_type::uniform_coherent:
                case buffer_type::uniform_cached:
                    // 直接映射上传
                    upload_success = direct_upload(buffer, allocation, datas.data(), buffer_size);
                    break;

                case buffer_type::vertex:
                case buffer_type::index:
                case buffer_type::uniform_gpu_only:
                    // 使用 staging buffer
                    upload_success = staging_upload(buffer, datas.data(), buffer_size);
                    break;
            }

            if (!upload_success) {
                vmaDestroyBuffer(this->allocator, buffer, allocation);
                if (auto expected = this->recycle_handler(handler_value)) {
                    std::println("Failed to upload buffer: {}",expected.error());
                }
                return invalid_handler;
            }

            this->buffers.emplace(handler_value, vma_buffer{buffer, allocation});
            return handler_value;

        } else {
            std::println("vma handler distribute error: {}", result.error());
            return invalid_handler;
        }
    }

    // 创建带数据的纹理
    template <typename pixel_type>
    handler create_image_with_data(
        std::span<pixel_type> pixels,
        const image_info& info,
        const image_type type
    ) {
        if (pixels.empty()) {
            std::println("Cannot create image with empty data");
            return invalid_handler;
        }

        const VkDeviceSize image_size = pixels.size_bytes();

        if (const VkDeviceSize expected_size = info.height * info.width  * sizeof_vk_format(info.format); expected_size != image_size) {
            std::println(stderr, "incorrect image size [{}], expected [{}]", image_size, expected_size);
        }

        if (auto result = this->distribute_handler(); result.has_value()) {
            const auto handler_value = result.value();

            const auto alloc_info = get_image_allocation_info_from_type(type);
            auto image_create_info = get_image_create_info_from_type(type, info);
            image_create_info.mipLevels = info.mip_levels;
            image_create_info.extent.width = info.width;
            image_create_info.extent.height = info.height;
            image_create_info.extent.depth = 1;
            image_create_info.arrayLayers = info.array_layers;


            VmaAllocation allocation = VK_NULL_HANDLE;
            VkImage image = VK_NULL_HANDLE;
            VmaAllocationInfo alloc_detail = {};

            const VkResult vk_result = vmaCreateImage(
                this->allocator,
                &image_create_info,
                &alloc_info,
                &image,
                &allocation,
                &alloc_detail
            );

            if (vk_result != VK_SUCCESS) {
                std::println("Failed to create image: {}", static_cast<int>(vk_result));
                if (auto expected = this->recycle_handler(handler_value); !expected) {
                    std::println(stderr, "vma handler distribute error: {}", expected.error());
                }
                return invalid_handler;
            }

            // 根据类型选择上传方式
            bool upload_success = false;
            switch (type) {
                case image_type::texture_2d_staging:
                    // 直接映射上传
                    upload_success = direct_image_upload(image, allocation, pixels.data(), image_size);
                    break;

                case image_type::texture_2d:
                case image_type::texture_2d_color:
                case image_type::texture_cubemap:
                case image_type::render_target:
                    // 使用 staging buffer 上传
                    upload_success = staging_image_upload(image, pixels.data(), image_size, info);
                    break;

                case image_type::texture_2d_depth:
                    // 深度纹理通常不需要初始数据
                    upload_success = true;
                    break;
            }

            if (!upload_success) {
                vmaDestroyImage(this->allocator, image, allocation);
                if (auto expected = this->recycle_handler(handler_value); !expected) {
                    std::println(stderr, "vma handler distribute error: {}", expected.error());
                }
                return invalid_handler;
            }

            this->images.emplace(handler_value, vma_image{image, allocation});
            return handler_value;

        } else {
            std::println("vma handler distribute error: {}", result.error());
            return invalid_handler;
        }
    }

    // 创建空图像（用于渲染目标等）
    handler create_empty_image(const image_info& info, image_type type);


    vma_waiter update_to_buffer(
        const void *source,
        handler buffer_handler,
        VkDeviceSize size,
        VkDeviceSize src_offset
    );

    [[nodiscard]] VkDeviceMemory get_device_memory(const VmaAllocation &allocation) const;
};


#endif //VULKAN_PROJECT_VMA_H
