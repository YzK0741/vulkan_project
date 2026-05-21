//
// Created by 小叶 on 2026/5/18.
//

#ifndef VULKAN_PROJECT_VMA_H
#define VULKAN_PROJECT_VMA_H

#include <vma/vk_mem_alloc.h>
#include "vma_waiter.h"

struct vma_buffer {
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct vma_image {
    VkImage image;
    VmaAllocation allocation;
};

class VMA {
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    std::mutex mutex;

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

    void init(
        const VkInstance &instance,
        const VkDevice &vulkan_device,
        const VkPhysicalDevice &physical_device,
        const VkQueue& vulkan_queue,
        uint32_t queue_family_index
        );

    void destroy() const;

    [[nodiscard]] vma_buffer create_buffer(const VkBufferCreateInfo &buffer_create_info) const;

    void destroy_buffer(const vma_buffer &buffer) const;

    [[nodiscard]] vma_image create_image(const VkImageCreateInfo &image_create_info) const;

    void destroy_image(const vma_image &image) const;

    vma_waiter update_to_buffer(
        const void *source,
        const VkBuffer &destination,
        VkDeviceSize size,
        VkDeviceSize src_offset = 0,
        VkDeviceSize dst_offset = 0
    );

    [[nodiscard]] VkDeviceMemory get_device_memory(const VmaAllocation &allocation) const;
};


#endif //VULKAN_PROJECT_VMA_H
