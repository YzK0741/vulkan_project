//
// Created by 小叶 on 2026/5/18.
//

#ifndef VULKAN_PROJECT_VMA_H
#define VULKAN_PROJECT_VMA_H

#include <vma/vk_mem_alloc.h>
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
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    std::mutex mutex;
    std::unordered_map<handler,vma_buffer> buffers;
    std::unordered_map<handler,vma_image> images;

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

    [[nodiscard]] handler create_buffer(const VkBufferCreateInfo &buffer_create_info);

    [[nodiscard]] const vma_buffer& get_buffer(handler buffer_handle);

    void destroy_buffer(long buffer_handler);

    [[nodiscard]] handler create_image(const VkImageCreateInfo &image_create_info);
    [[nodiscard]] const vma_image &get_image(const handler &image_handle);

    void destroy_image(const long &image_handler);

    vma_waiter update_to_buffer(
        const void *source,
        handler buffer_handler,
        VkDeviceSize size,
        VkDeviceSize src_offset
    );

    [[nodiscard]] VkDeviceMemory get_device_memory(const VmaAllocation &allocation) const;
};


#endif //VULKAN_PROJECT_VMA_H
