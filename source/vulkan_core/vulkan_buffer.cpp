//
// Created by 小叶 on 2026/2/6.
//

#include "./vulkan_buffer.h"
#include "../utility.h"

#include <ostream>

uint32_t find_memory_type(
        const VkPhysicalDevice& physical_device,
        const uint32_t type_filter,
        const VkMemoryPropertyFlags properties
    ) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
            }
    }

    std::println("无法找到合适的内存类型!");
    print_stacktrace_and_terminate();
}


void create_buffer(
        const VkDevice& device,
        const VkPhysicalDevice& physical_device,
        const VkDeviceSize size,
        const VkBufferUsageFlags usage,
        const VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& buffer_memory
    ) {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
        std::println("无法创建缓冲区!");
        print_stacktrace_and_terminate();
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(
        physical_device,
        mem_requirements.memoryTypeBits,
        properties
    );

    if (vkAllocateMemory(device, &alloc_info, nullptr, &buffer_memory) != VK_SUCCESS) {
        std::println("无法分配缓冲区内存!");
        print_stacktrace_and_terminate();
    }

    vkBindBufferMemory(device, buffer, buffer_memory, 0);
}