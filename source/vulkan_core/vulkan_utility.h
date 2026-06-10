//
// Created by 小叶 on 2026/1/24.
//

#ifndef VULKAN_PROJECT_VULKAN_UTILITY_H
#define VULKAN_PROJECT_VULKAN_UTILITY_H

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <stb/stb_image.h>
#include "../utility.h"

uint32_t find_memory_type(const uint32_t& type_filter, const VkMemoryPropertyFlags& properties, const VkPhysicalDevice& physical_device);

struct vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 tex_coord;

    // 需要实现相等运算符用于哈希映射
    bool operator==(const vertex& other) const {
        constexpr float EPSILON = 0.0001f;

        // 比较位置
        if (glm::length(position - other.position) > EPSILON)
            return false;

        // 比较法线
        if (glm::length(normal - other.normal) > EPSILON)
            return false;

        // 比较纹理坐标
        if (glm::length(tex_coord - other.tex_coord) > EPSILON)
            return false;

        return true;
    }

    // 获取绑定描述（告诉Vulkan顶点数据如何组织）
    static std::array<VkVertexInputBindingDescription, 1> get_binding_descriptions() {
        std::array<VkVertexInputBindingDescription, 1> binding_descriptions{};

        // 只有一个绑定，所有顶点属性都打包在一个数组中
        binding_descriptions[0].binding = 0;                          // 绑定索引
        binding_descriptions[0].stride = sizeof(vertex);              // 每个顶点的大小
        binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // 每顶点数据

        return binding_descriptions;
    }

    // 获取属性描述（告诉Vulkan每个属性的位置、格式和偏移）
    static std::array<VkVertexInputAttributeDescription, 3> get_attribute_descriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions{};

        // 位置属性 - location 0
        attribute_descriptions[0].binding = 0;
        attribute_descriptions[0].location = 0; // 着色器中的 location
        attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        attribute_descriptions[0].offset = offsetof(vertex, position); // 偏移量

        // 法线属性 - location 1
        attribute_descriptions[1].binding = 0;
        attribute_descriptions[1].location = 1;
        attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        attribute_descriptions[1].offset = offsetof(vertex, normal);

        // 纹理坐标属性 - location 2
        attribute_descriptions[2].binding = 0;
        attribute_descriptions[2].location = 2;
        attribute_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT; // vec2
        attribute_descriptions[2].offset = offsetof(vertex, tex_coord);

        return attribute_descriptions;
    }
};


struct pbr_gltf_data {
    std::vector<vertex> vertices;
    std::vector<uint32_t> indices;
    int materialIndex = -1;
};

template <typename T>
void hash_combine(std::size_t& seed, const T& val) {
    seed ^= std::hash<T>{}(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// 自定义哈希函数
template<>
struct std::hash<vertex> {
    std::size_t operator()(const vertex& v) const noexcept {
        const auto h1 = std::hash<float>{}(v.position.x);
        const auto h2 = std::hash<float>{}(v.position.y);
        const auto h3 = std::hash<float>{}(v.position.z);
        const auto h4 = std::hash<float>{}(v.normal.x);
        const auto h5 = std::hash<float>{}(v.normal.y);
        const auto h6 = std::hash<float>{}(v.normal.z);
        const auto h7 = std::hash<float>{}(v.tex_coord.x);
        const auto h8 = std::hash<float>{}(v.tex_coord.y);

        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5) ^ (h7 << 6) ^ (h8 << 7);
    }
};

std::pair<std::vector<vertex>, std::vector<uint32_t>>
get_gltf_module_from_file(std::string_view path);

#include <vector>

// 通用顶点缓冲区创建器
class vertex_buffer_creator {
    VkDevice device;
    VkPhysicalDevice physical_device;
    VkCommandPool command_pool;
    VkQueue graphics_queue;

public:
    struct VertexBuffer {
        VkBuffer buffer;
        VkDeviceMemory memory;
        uint32_t vertexCount;
    };

    vertex_buffer_creator(
        VkDevice device,
        VkPhysicalDevice physical_device,
        VkCommandPool command_pool,
        VkQueue graphics_queue
    ) : device(device),
        physical_device(physical_device),
        command_pool(command_pool),
        graphics_queue(graphics_queue) {}

    // 通用函数：从任意类型的vector创建顶点缓冲区
    template<typename T>
    VertexBuffer create_vertex_buffer(const std::vector<T>& vertices, const VkBufferUsageFlags additional_usage = 0) {
        VertexBuffer vertex_buffer = {};

        // 计算缓冲区大小
        const VkDeviceSize bufferSize = sizeof(T) * vertices.size();
        vertex_buffer.vertexCount = static_cast<uint32_t>(vertices.size());

        if (vertices.empty()) {
            std::println("顶点数据为空!");
            print_stacktrace_and_terminate();
        }

        // 创建暂存缓冲区
        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        create_buffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            staging_buffer_memory
        );

        // 复制数据到暂存缓冲区
        void* data;
        vkMapMemory(device, staging_buffer_memory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(device, staging_buffer_memory);

        // 创建最终的顶点缓冲区
        create_buffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | additional_usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            vertex_buffer.buffer,
            vertex_buffer.memory
        );

        // 从暂存缓冲区复制到设备本地缓冲区
        copy_buffer(staging_buffer, vertex_buffer.buffer, bufferSize);

        // 清理暂存缓冲区
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_buffer_memory, nullptr);

        return vertex_buffer;
    }

    // 创建索引缓冲区
    VertexBuffer create_index_buffer(const std::vector<uint16_t>& indices) {
        return create_vertex_buffer(indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    }

    VertexBuffer create_index_buffer(const std::vector<uint32_t>& indices) {
        return create_vertex_buffer(indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    }

    // 销毁缓冲区
    void destroy_vertex_buffer(const VertexBuffer& vertexBuffer) const noexcept {
        vkDestroyBuffer(device, vertexBuffer.buffer, nullptr);
        vkFreeMemory(device, vertexBuffer.memory, nullptr);
    }

private:
    void create_buffer(
        const VkDeviceSize size,
        const VkBufferUsageFlags usage,
        const VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory
    ) const {
        // 创建缓冲区
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            std::println("无法创建缓冲区!");
            print_stacktrace_and_terminate();
        }

        // 获取内存需求
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        // 分配内存
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            std::println("无法分配缓冲区内存!");
            print_stacktrace_and_terminate();
        }

        // 绑定内存
        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        std::println("找不到合适的内存类型!");
        print_stacktrace_and_terminate();

    }

    void copy_buffer(const VkBuffer& srcBuffer, const VkBuffer& dstBuffer, VkDeviceSize size) const {
        // 创建命令缓冲区
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = command_pool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        // 开始命令缓冲区
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        // 复制命令
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        // 结束命令缓冲区
        vkEndCommandBuffer(commandBuffer);

        // 提交并执行
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphics_queue);

        // 清理命令缓冲区
        vkFreeCommandBuffers(device, command_pool, 1, &commandBuffer);
    }
};


struct stb_texture {
    int width;
    int height;
    stbi_uc* pixels;
    VkDeviceSize image_size;
    ~stb_texture() {
        stbi_image_free(pixels);
    }
};

stb_texture create_texture_image_from_file();

struct buffer_resource {
    VkDevice device = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    size_t size = 0;
    bool is_owned = false;  // 标记是否拥有资源

    buffer_resource() = default;

    explicit buffer_resource(const VkDevice& device_) : device(device_) {}

    // 移动构造函数
    buffer_resource(buffer_resource&& other) noexcept
        : device(other.device),
          buffer(other.buffer),
          memory(other.memory),
          size(other.size),
          is_owned(other.is_owned) {
        other.device = VK_NULL_HANDLE;
        other.buffer = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.size = 0;
        other.is_owned = false;
    }

    // 移动赋值运算符
    buffer_resource& operator=(buffer_resource&& other) noexcept {
        if (this != &other) {
            cleanup();
            device = other.device;
            buffer = other.buffer;
            memory = other.memory;
            size = other.size;
            is_owned = other.is_owned;

            other.device = VK_NULL_HANDLE;
            other.buffer = VK_NULL_HANDLE;
            other.memory = VK_NULL_HANDLE;
            other.size = 0;
            other.is_owned = false;
        }
        return *this;
    }

    void set(const VkBuffer& buf, const VkDeviceMemory& mem, size_t sz, VkDevice dev) {
        cleanup();
        device = dev;
        buffer = buf;
        memory = mem;
        size = sz;
        is_owned = true;
    }

    void cleanup() {
        if (is_owned) {
            if (buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, buffer, nullptr);
                buffer = VK_NULL_HANDLE;
            }
            if (memory != VK_NULL_HANDLE) {
                vkFreeMemory(device, memory, nullptr);
                memory = VK_NULL_HANDLE;
            }
            size = 0;
            is_owned = false;
        }
    }

    VkBuffer operator*() const {
        return buffer;
    }

    explicit operator bool() const {
        return buffer != VK_NULL_HANDLE;
    }

    ~buffer_resource() {
        cleanup();
    }

    // 禁用拷贝
    buffer_resource(const buffer_resource&) = delete;
    buffer_resource& operator=(const buffer_resource&) = delete;
};

buffer_resource create_buffer_from_stb(VkDevice device,
                                      VkPhysicalDevice physical_device,
                                      const stb_texture& texture);

#endif //VULKAN_PROJECT_VULKAN_UTILITY_H