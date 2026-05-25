//
// Created by 小叶 on 2026/5/18.
//
#ifdef _DEBUG
    #define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
    #define VMA_DEBUG_MARGIN 16
    #define VMA_DEBUG_DETECT_CORRUPTION 1
    #define VMA_DEBUG_DONT_EXCEED_HEAP_SIZE_WITH_ALLOCATION_SIZE 1
#endif
#define VMA_IMPLEMENTATION
#include <print>
#include <vma/vk_mem_alloc.h>
#include "../../utility.h"
#include "vma.h"

#include <ranges>


void VMA::init(
    const VkInstance &instance,
    const VkDevice &vulkan_device,
    const VkPhysicalDevice &physical_device,
    const VkQueue& vulkan_queue,
    const uint32_t queue_family_index
    ) {
    VmaAllocatorCreateInfo vma_allocator_create_info = {};
    vma_allocator_create_info.instance = instance;
    vma_allocator_create_info.device = vulkan_device;
    vma_allocator_create_info.physicalDevice = physical_device;

    vmaCreateAllocator(&vma_allocator_create_info, &this->allocator);

    VkCommandPoolCreateInfo command_pool_create_info = {};

    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_create_info.queueFamilyIndex = queue_family_index;

    if (vkCreateCommandPool(vulkan_device, &command_pool_create_info, nullptr, &this->command_pool) != VK_SUCCESS) {
        std::println("Failed to create command pool!");
        print_stacktrace_and_terminate();
    }

    this->device = vulkan_device;
    this->queue = vulkan_queue;
}

void VMA::destroy() const {
    for (const auto &[buffer, allocation]: this->buffers | std::views::values) { // NOLINT(*-misplaced-const)
        vmaDestroyBuffer(this->allocator, buffer, allocation);
    }
    for (const auto &[image, allocation]: this->images | std::views::values) { // NOLINT(*-misplaced-const)
        vmaDestroyImage(this->allocator, image, allocation);
    }
    if (this->command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(this->device, this->command_pool, nullptr);
    }
    if (this->allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(this->allocator);
    }
}

enable_handler_distribute<VMA>::handler VMA::create_buffer(const VkBufferCreateInfo &buffer_create_info) {

    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation;

    vmaCreateBuffer(this->allocator, &buffer_create_info, &allocation_create_info, &buffer, &allocation, nullptr);

    if (auto expected_handler = this->distribute_handler()) {
        const auto handler = expected_handler.value();
        this->buffers[handler] = {buffer, allocation};
        return handler;
    } else {
        std::println("failed to distribute handler: {}", expected_handler.value());
        print_stacktrace_and_terminate();
    }
}

void VMA::destroy_buffer(const handler buffer_handler) {
    if (this->buffers.contains(buffer_handler)) {
        if (is_valid_handler(buffer_handler)) {
            const auto&[buffer, allocation] = buffers[buffer_handler]; // NOLINT(*-misplaced-const)
            vmaDestroyBuffer(this->allocator, buffer, allocation);
            this->buffers.erase(buffer_handler);
            if (const auto result = this->recycle_handler(buffer_handler); !result) {
                std::println(stderr, "can not recycle buffer handler [{}], {}", buffer_handler, result.error());
            }
        } else {
            std::println("invalid buffer handler [{}]", buffer_handler);
        }
    } else {
        std::println("buffer does not exit");
    }

}

enable_handler_distribute<VMA>::handler VMA::create_image(const VkImageCreateInfo &image_create_info) {
    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation;

    vmaCreateImage(this->allocator, &image_create_info, &allocation_create_info, &image, &allocation, nullptr);

    if (auto expected_handler = this->distribute_handler()) {
        const auto handler = expected_handler.value();
        this->images[handler] = {image, allocation};
        return handler;
    } else {
        std::println("failed to distribute handler: {}", expected_handler.value());
        print_stacktrace_and_terminate();
    }
}

void VMA::destroy_image(const handler &image_handler) {
    if (this->images.contains(image_handler)) {
        const auto&[image, allocation] = this->images[image_handler]; // NOLINT(*-misplaced-const)
        vmaDestroyImage(this->allocator, image, allocation);
        this->images.erase(image_handler);
        if (auto result = this->recycle_handler(image_handler); !result) {
            std::println("failed to recycle handler: {}", result.error());
        }
    }

}

std::pair<VkBuffer, VmaAllocation> make_staging_buffer(const VmaAllocator& allocator, const VkDeviceSize size) {
    VkBufferCreateInfo staging_buffer_create_info = {};
    staging_buffer_create_info.size = size;
    staging_buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;


    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VmaAllocation staging_buffer_allocation;
    vmaCreateBuffer(allocator, &staging_buffer_create_info,
        &allocation_create_info,
        &staging_buffer,
        &staging_buffer_allocation,
        nullptr
        );

    return {staging_buffer, staging_buffer_allocation};
}

VkCommandBuffer create_command_buffer(VkDevice device,const VkCommandPool& command_pool) {
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.commandBufferCount = 1;
    command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.commandPool = command_pool;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    vkAllocateCommandBuffers(device, &command_buffer_allocate_info, &command_buffer);

    return command_buffer;
}

vma_waiter VMA::update_to_buffer(
    const void *source,
    const handler buffer_handler,
    const VkDeviceSize size,
    const VkDeviceSize src_offset
) {

    const auto& [staging_buffer, staging_buffer_allocation] = make_staging_buffer(this->allocator, size);

    const auto& [des, allocation] = this->buffers[buffer_handler]; // NOLINT(*-misplaced-const)

    VmaAllocationInfo des_info = {};
    vmaGetAllocationInfo(this->allocator, allocation, &des_info);

    const auto dst_offset = des_info.offset;

    this->mutex.lock();
    const VkCommandBuffer& command_buffer = create_command_buffer(this->device, this->command_pool);
    mutex.unlock();

    const VkFence& fence = this->create_fence();

    std::future<void> future = std::async(std::launch::async,
        [=,this] {
            this->do_upload_to_buffer(staging_buffer,
                staging_buffer_allocation,source, des, command_buffer, fence, size, src_offset, dst_offset); // NOLINT(*-misplaced-const)
        });

    return {
        std::move(future),
        [=, this] {
            vkWaitForFences(this->device, 1, &fence, VK_TRUE, UINT64_MAX); // NOLINT(*-misplaced-const)
            vkDestroyFence(this->device, fence, nullptr);
            vkResetCommandBuffer(command_buffer,0); // NOLINT(*-misplaced-const)
            vkFreeCommandBuffers(this->device, this->command_pool, 1, &command_buffer);
            vmaDestroyBuffer(this->allocator, staging_buffer, staging_buffer_allocation);}
    };
}

const vma_buffer &VMA::get_buffer(const handler buffer_handle) {
    return this->buffers[buffer_handle];
}

const vma_image &VMA::get_image(const handler &image_handle) {
    if (this->images.contains(image_handle)) {
        return this->images[image_handle];
    } else {
        std::println(stderr, "handle does not exit");
        print_stacktrace_and_terminate();
    }
}


VkDeviceMemory VMA::get_device_memory(const VmaAllocation &allocation) const {
    VmaAllocationInfo allocation_info = {};

    vmaGetAllocationInfo(this->allocator, allocation, &allocation_info);

    return allocation_info.deviceMemory;
}

void VMA::do_upload_to_buffer(
    const VkBuffer &staging_buffer,
    const VmaAllocation &staging_buffer_allocation,
    const void *source,
    const VkBuffer &destination,
    const VkCommandBuffer &command_buffer,
    const VkFence& fence,
    const VkDeviceSize size,
    const VkDeviceSize src_offset,
    const VkDeviceSize dst_offset
) {


    void* mapped_data = nullptr;
    vmaMapMemory(this->allocator, staging_buffer_allocation, &mapped_data);
    memcpy(mapped_data, source, size);
    vmaUnmapMemory(this->allocator, staging_buffer_allocation);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(command_buffer, &begin_info);

    VkBufferCopy copy = {};
    copy.size = size;
    copy.dstOffset = dst_offset;
    copy.srcOffset = src_offset;
    vkCmdCopyBuffer(command_buffer, staging_buffer, destination, 1, &copy);

    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    this->mutex.lock();
    vkQueueSubmit(this->queue, 1, &submit_info, fence);
    this->mutex.unlock();
}

VkFence VMA::create_fence() const {

    VkFence fence = VK_NULL_HANDLE;

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.flags = 0;
    vkCreateFence(this->device, &fence_create_info, nullptr, &fence);

    return fence;
}
