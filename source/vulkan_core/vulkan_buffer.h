//
// Created by 小叶 on 2026/2/6.
//

#ifndef VULKAN_PROJECT_VULKAN_BUFFER_H
#define VULKAN_PROJECT_VULKAN_BUFFER_H

#include <vulkan/vulkan.h>

uint32_t find_memory_type(
        const VkPhysicalDevice& physical_device,
        uint32_t type_filter,
        VkMemoryPropertyFlags properties
    );

void create_buffer(
        const VkDevice& device,
        const VkPhysicalDevice& physical_device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& buffer_memory
        );

#endif //VULKAN_PROJECT_VULKAN_BUFFER_H