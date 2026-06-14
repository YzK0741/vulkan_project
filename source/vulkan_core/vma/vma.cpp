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
    vma_allocator_create_info.flags = 0;

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
    std::unique_lock lock(this->mutex);

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

bool VMA::direct_upload(VkBuffer buffer, const VmaAllocation allocation, const void *data, const VkDeviceSize size) const {
    void* mapped_data = nullptr;
    if (const VkResult result = vmaMapMemory(this->allocator, allocation, &mapped_data); result != VK_SUCCESS) {
        std::println("Failed to map memory: {}", static_cast<int>(result));
        return false;
    }

    memcpy(mapped_data, data, size);

    VmaAllocationInfo alloc_info;
    vmaGetAllocationInfo(this->allocator, allocation, &alloc_info);
    if ((alloc_info.memoryType & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
        vmaFlushAllocation(this->allocator, allocation, 0, size);
    }

    vmaUnmapMemory(this->allocator, allocation);
    return true;
}


bool VMA::staging_upload(const VkBuffer dst_buffer, const void *data, const VkDeviceSize size) {
        // 创建 staging buffer
        VkBufferCreateInfo staging_create_info = {};
        staging_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        staging_create_info.size = size;
        staging_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo staging_alloc_info = {};
        staging_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        staging_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VkBuffer staging_buffer = VK_NULL_HANDLE;
        VmaAllocation staging_allocation = VK_NULL_HANDLE;
        VmaAllocationInfo staging_info = {};

        const VkResult result = vmaCreateBuffer(
            this->allocator,
            &staging_create_info,
            &staging_alloc_info,
            &staging_buffer,
            &staging_allocation,
            &staging_info
        );

        if (result != VK_SUCCESS) {
            std::println(stderr, "Failed to create staging buffer: {}", static_cast<int>(result));
            return false;
        }

        // 拷贝数据
        if (staging_info.pMappedData) {
            memcpy(staging_info.pMappedData, data, size);
            if ((staging_info.memoryType & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
                vmaFlushAllocation(this->allocator, staging_allocation, 0, size);
            }
        } else {
            vmaDestroyBuffer(this->allocator, staging_buffer, staging_allocation);
            return false;
        }

        // 执行拷贝命令
        const VkCommandBuffer command_buffer = create_command_buffer(this->device, this->command_pool);
        VkFence fence = this->create_fence();

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(command_buffer, &begin_info);

        VkBufferCopy copy_region = {};
        copy_region.size = size;
        vkCmdCopyBuffer(command_buffer, staging_buffer, dst_buffer, 1, &copy_region);

        vkEndCommandBuffer(command_buffer);

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        {
            std::lock_guard<std::mutex> lock(this->mutex);
            vkQueueSubmit(this->queue, 1, &submit_info, fence);
        }

        vkWaitForFences(this->device, 1, &fence, VK_TRUE, UINT64_MAX);

        // 清理
        vkDestroyFence(this->device, fence, nullptr);
        vkFreeCommandBuffers(this->device, this->command_pool, 1, &command_buffer);
        vmaDestroyBuffer(this->allocator, staging_buffer, staging_allocation);

        return true;
}

enable_handler_distribute<VMA>::handler VMA::create_empty_image(const image_info &info, const image_type type) {
    if (auto result = this->distribute_handler(); result.has_value()) {
        const auto handler_value = result.value();

        const auto alloc_info = get_image_allocation_info_from_type(type);
        auto image_create_info = get_image_create_info_from_type(type, info);

        VmaAllocation allocation = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;

        VkResult vk_result = vmaCreateImage(
            this->allocator,
            &image_create_info,
            &alloc_info,
            &image,
            &allocation,
            nullptr
        );

        if (vk_result != VK_SUCCESS) {
            std::println("Failed to create image: {}", static_cast<int>(vk_result));
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

bool VMA::direct_image_upload(VkImage image, VmaAllocation allocation, const void *data, const VkDeviceSize size) const {
    void* mapped_data = nullptr;
    VkResult result = vmaMapMemory(this->allocator, allocation, &mapped_data);
    if (result != VK_SUCCESS) {
        std::println("Failed to map image memory: {}", static_cast<int>(result));
        return false;
    }

    memcpy(mapped_data, data, size);

    VmaAllocationInfo alloc_info;
    vmaGetAllocationInfo(this->allocator, allocation, &alloc_info);
    if ((alloc_info.memoryType & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
        vmaFlushAllocation(this->allocator, allocation, 0, size);
    }

    vmaUnmapMemory(this->allocator, allocation);
    return true;
}

bool VMA::staging_image_upload(VkImage dst_image, const void *data, VkDeviceSize size, const image_info &info) {
    // 创建 staging buffer（保持不变）
    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = size;
    buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VmaAllocation staging_allocation = VK_NULL_HANDLE;
    VmaAllocationInfo staging_info = {};

    VkResult result = vmaCreateBuffer(
        this->allocator,
        &buffer_create_info,
        &alloc_info,
        &staging_buffer,
        &staging_allocation,
        &staging_info
    );

    if (result != VK_SUCCESS) {
        std::println("Failed to create staging buffer for image: {}", static_cast<int>(result));
        return false;
    }

    // 拷贝数据到 staging buffer
    if (staging_info.pMappedData) {
        memcpy(staging_info.pMappedData, data, size);
        if ((staging_info.memoryType & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
            vmaFlushAllocation(this->allocator, staging_allocation, 0, size);
        }
    } else {
        vmaDestroyBuffer(this->allocator, staging_buffer, staging_allocation);
        return false;
    }

    // 执行拷贝命令
    VkCommandBuffer command_buffer = create_command_buffer(this->device, this->command_pool);
    VkFence fence = this->create_fence();

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(command_buffer, &begin_info);

    // ========== 修复点 1：正确的布局转换顺序 ==========
    // 第一步：UNDEFINED -> TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = dst_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = info.mip_levels;
    barrier.subresourceRange.layerCount = info.array_layers;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    // 第二步：拷贝数据
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;  // 0 表示紧密排列
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = info.array_layers;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {info.width, info.height, 1};

    vkCmdCopyBufferToImage(
        command_buffer,
        staging_buffer,
        dst_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    // 第三步：TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    vkEndCommandBuffer(command_buffer);

    // 提交并等待完成
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    {
        std::lock_guard<std::mutex> lock(this->mutex);
        vkQueueSubmit(this->queue, 1, &submit_info, fence);
    }

    vkWaitForFences(this->device, 1, &fence, VK_TRUE, UINT64_MAX);

    // 清理
    vkDestroyFence(this->device, fence, nullptr);
    vkFreeCommandBuffers(this->device, this->command_pool, 1, &command_buffer);
    vmaDestroyBuffer(this->allocator, staging_buffer, staging_allocation);

    return true;
}
