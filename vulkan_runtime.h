//
// Created by 小叶 on 2026/1/24.
//

#ifndef VULKAN_PROJECT_VULKAN_RUNTIME_H
#define VULKAN_PROJECT_VULKAN_RUNTIME_H

//#include <stb/stb_image_write.h>
#include <thread>
#include "vulkan_core.h"
#include "vulkan_utility.h"
#include "png_loader.h"
#include "vulkan_buffer.h"

struct vulkan_mesh_buffer {
private:
    std::reference_wrapper<const vulkan_core> core_ref;
    size_t vertex_count = 0;
    size_t index_count = 0;

    // 交错顶点结构，与vertex结构保持一致
    using interleaved_vertex = vertex;

    template <typename T>
    static void create_buffer_from_vector(
        vulkan_core& core,
        const std::vector<T>& data,
        const VkBufferUsageFlags usage_flags,
        VkBuffer& buffer,
        VkDeviceMemory& device_memory
        ) {
        if (data.empty()) {
            buffer = VK_NULL_HANDLE;
            device_memory = VK_NULL_HANDLE;
            return;
        }

        const VkDeviceSize size = sizeof(T) * data.size();
        VkBuffer staging_buffer = VK_NULL_HANDLE;
        VkDeviceMemory staging_memory = VK_NULL_HANDLE;
            create_buffer(
                core.device,
                core.physical_device,
                size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                staging_buffer,
                staging_memory);

            void *mapped_data = nullptr;
            VkResult result = vkMapMemory(core.device, staging_memory, 0, size, 0, &mapped_data);
            if (result != VK_SUCCESS) {
                std::println("Failed to map memory for staging buffer");
                print_stacktrace_and_terminate();
            }
            memcpy(mapped_data, data.data(), size);
            vkUnmapMemory(core.device, staging_memory);

            create_buffer(
                core.device,
                core.physical_device,
                size,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage_flags,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                buffer,
                device_memory);

            core.copy_buffer(staging_buffer, buffer, size);

            vkDestroyBuffer(core.device, staging_buffer, nullptr);
            vkFreeMemory(core.device, staging_memory, nullptr);

    }

    void cleanup() {
        auto& core = core_ref.get();
        auto destroy_buffer = [&core](VkBuffer& buffer, VkDeviceMemory& memory) {
            if (buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(core.device, buffer, nullptr);
                buffer = VK_NULL_HANDLE;
            }
            if (memory != VK_NULL_HANDLE) {
                vkFreeMemory(core.device, memory, nullptr);
                memory = VK_NULL_HANDLE;
            }
        };

        destroy_buffer(interleaved_vertex_buffer, interleaved_vertex_memory);
        destroy_buffer(index_buffer, index_memory);
    }

public:
    // 交错顶点缓冲区（包含所有顶点属性）
    VkBuffer interleaved_vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory interleaved_vertex_memory = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory index_memory = VK_NULL_HANDLE;
    VkIndexType index_type = VK_INDEX_TYPE_UINT32;

    // 禁止复制
    vulkan_mesh_buffer(const vulkan_mesh_buffer&) = delete;
    vulkan_mesh_buffer& operator=(const vulkan_mesh_buffer&) = delete;

    // 移动构造函数
    vulkan_mesh_buffer(vulkan_mesh_buffer&& other) noexcept
        : core_ref(other.core_ref)
        , vertex_count(other.vertex_count)
        , index_count(other.index_count)
        , interleaved_vertex_buffer(std::exchange(other.interleaved_vertex_buffer, VK_NULL_HANDLE))
        , interleaved_vertex_memory(std::exchange(other.interleaved_vertex_memory, VK_NULL_HANDLE))
        , index_buffer(std::exchange(other.index_buffer, VK_NULL_HANDLE))
        , index_memory(std::exchange(other.index_memory, VK_NULL_HANDLE))
        , index_type(other.index_type) {
    }

    // 移动赋值运算符
    vulkan_mesh_buffer& operator=(vulkan_mesh_buffer&& other) noexcept {
        if (this != &other) {
            cleanup();

            core_ref = other.core_ref;
            vertex_count = other.vertex_count;
            index_count = other.index_count;

            interleaved_vertex_buffer = std::exchange(other.interleaved_vertex_buffer, VK_NULL_HANDLE);
            interleaved_vertex_memory = std::exchange(other.interleaved_vertex_memory, VK_NULL_HANDLE);
            index_buffer = std::exchange(other.index_buffer, VK_NULL_HANDLE);
            index_memory = std::exchange(other.index_memory, VK_NULL_HANDLE);
            index_type = other.index_type;
        }
        return *this;
    }

    // 构造函数：使用vertex结构的数组
    template <typename IndexT>
    explicit vulkan_mesh_buffer(vulkan_core& core,
                                const std::vector<vertex>& vertices,
                                const std::vector<IndexT>& indices)
        : core_ref(core) {

        if (vertices.empty()) {
            std::println("Vertices cannot be empty");
            print_stacktrace_and_terminate();
        }

        vertex_count = vertices.size();

        // 设置索引类型
        if constexpr (sizeof(IndexT) == 2) {
            index_type = VK_INDEX_TYPE_UINT16;
        } else if constexpr (sizeof(IndexT) == 4) {
            index_type = VK_INDEX_TYPE_UINT32;
        } else {
            std::println("Unsupported index type");
            print_stacktrace_and_terminate();
        }

        // 直接使用vertex结构创建交错顶点缓冲区
        create_buffer_from_vector(core,
            vertices,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            this->interleaved_vertex_buffer,
            this->interleaved_vertex_memory);

        // 创建索引缓冲区
        if (!indices.empty()) {
            index_count = indices.size();
            create_buffer_from_vector(core,
                indices,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                this->index_buffer,
                this->index_memory);
        }
    }

    // 构造函数：使用分离的顶点属性数组，转换为交错布局
    template <typename PositionT, typename NormalT, typename TexCoordT, typename IndexT>
    explicit vulkan_mesh_buffer(vulkan_core& core,
                                const std::vector<PositionT>& positions,
                                const std::vector<NormalT>& normals,
                                const std::vector<TexCoordT>& tex_coords,
                                const std::vector<IndexT>& indices)
        : core_ref(core) {

        if (positions.empty()) {
            std::println("Vertex positions cannot be empty");
            print_stacktrace_and_terminate();
        }

        vertex_count = positions.size();

        // 验证数据一致性
        if (!normals.empty() && normals.size() != vertex_count) {
            std::println("Normals count must match positions count");
            print_stacktrace_and_terminate();
        }
        if (!tex_coords.empty() && tex_coords.size() != vertex_count) {
            std::println("Texture coordinates count must match positions count");
            print_stacktrace_and_terminate();
        }

        // 设置索引类型
        if constexpr (sizeof(IndexT) == 2) {
            index_type = VK_INDEX_TYPE_UINT16;
        } else if constexpr (sizeof(IndexT) == 4) {
            index_type = VK_INDEX_TYPE_UINT32;
        } else {
            std::println("Unsupported index type");
            print_stacktrace_and_terminate();
        }

        // 将分离的顶点属性转换为交错vertex结构
        std::vector<vertex> interleaved_vertices;
        interleaved_vertices.reserve(vertex_count);

        for (size_t i = 0; i < vertex_count; ++i) {
            vertex v = {};
            v.position = positions[i];  // 位置

            if (!normals.empty()) {
                v.normal = normals[i];  // 使用法线作为颜色
            } else {
                v.normal = glm::vec3(1.0f, 1.0f, 1.0f); // 默认颜色
            }

            if (!tex_coords.empty()) {
                v.tex_coord = tex_coords[i];  // 纹理坐标
            } else {
                v.tex_coord = glm::vec2(0.0f, 0.0f); // 默认纹理坐标
            }

            interleaved_vertices.push_back(v);
        }

        // 创建交错顶点缓冲区
        create_buffer_from_vector(core,
            interleaved_vertices,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            this->interleaved_vertex_buffer,
            this->interleaved_vertex_memory);

        // 创建索引缓冲区
        if (!indices.empty()) {
            index_count = indices.size();
            create_buffer_from_vector(core,
                indices,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                this->index_buffer,
                this->index_memory);
        }
    }

    // 构造函数：无索引版本
    template <typename PositionT, typename NormalT, typename TexCoordT>
    explicit vulkan_mesh_buffer(vulkan_core& core,
                                const std::vector<PositionT>& positions,
                                const std::vector<NormalT>& normals,
                                const std::vector<TexCoordT>& tex_coords)
        : vulkan_mesh_buffer(core, positions, normals, tex_coords, std::vector<uint32_t>{}) {
    }

    ~vulkan_mesh_buffer() {
        cleanup();
    }

    // 获取信息
    [[nodiscard]] size_t get_vertex_count() const { return vertex_count; }
    [[nodiscard]] size_t get_index_count() const { return index_count; }
    [[nodiscard]] bool has_indices() const { return index_buffer != VK_NULL_HANDLE; }

    // 绑定交错顶点缓冲区
    void bind_vertex_buffers(VkCommandBuffer command_buffer, uint32_t first_binding = 0) const {
        if (interleaved_vertex_buffer != VK_NULL_HANDLE) {
            VkBuffer vertex_buffers[] = {interleaved_vertex_buffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(command_buffer, first_binding, 1, vertex_buffers, offsets);
        }
    }

    // 绑定索引缓冲区
    void bind_index_buffer(VkCommandBuffer command_buffer) const {
        if (index_buffer != VK_NULL_HANDLE) {
            vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, index_type);
        }
    }

    // 统一绑定方法
    void bind(VkCommandBuffer command_buffer, uint32_t first_vertex_binding = 0) const {
        bind_vertex_buffers(command_buffer, first_vertex_binding);
        if (has_indices()) {
            bind_index_buffer(command_buffer);
        }
    }

    // 绘制方法
    void draw(VkCommandBuffer command_buffer,
              uint32_t instance_count = 1,
              uint32_t first_instance = 0,
              int32_t vertex_offset = 0) const {

        bind(command_buffer);

        if (has_indices()) {
            vkCmdDrawIndexed(command_buffer,
                            static_cast<uint32_t>(index_count),
                            instance_count,
                            0,  // 第一个索引
                            vertex_offset,
                            first_instance);
        } else {
            vkCmdDraw(command_buffer,
                     static_cast<uint32_t>(vertex_count),
                     instance_count,
                     vertex_offset,
                     first_instance);
        }
    }

    // 获取顶点绑定描述（用于创建管线）- 使用与vertex结构匹配的描述
    struct vertex_binding_info {
        std::vector<VkVertexInputBindingDescription> bindings;
        std::vector<VkVertexInputAttributeDescription> attributes;
    };


};


struct vulkan_texture {
    VkImage texture_image = VK_NULL_HANDLE;
    VkDeviceMemory texture_image_memory = VK_NULL_HANDLE;
    VkImageView texture_image_view = VK_NULL_HANDLE;
    VkSampler texture_sampler = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;

    [[nodiscard]] bool is_valid() const {
        return texture_image != VK_NULL_HANDLE &&
               texture_image_view != VK_NULL_HANDLE &&
               texture_sampler != VK_NULL_HANDLE;
    }

    // 将纹理更新到描述符集
    void update_to_descriptor_set(VkDevice device,
                                 VkDescriptorSet descriptor_set,
                                 uint32_t binding = 1,  // 通常纹理绑定在binding 1
                                 VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 uint32_t array_element = 0) const {
        if (!is_valid()) {
            std::println("纹理无效，无法更新描述符集");
            print_stacktrace_and_terminate();
        }

        // 创建描述符图像信息
        VkDescriptorImageInfo image_info{};
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = texture_image_view;
        image_info.sampler = texture_sampler;

        // 创建描述符写入
        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = descriptor_set;
        descriptor_write.dstBinding = binding;
        descriptor_write.dstArrayElement = array_element;
        descriptor_write.descriptorType = descriptor_type;
        descriptor_write.descriptorCount = 1;  // 单个纹理
        descriptor_write.pImageInfo = &image_info;

        // 更新描述符集
        vkUpdateDescriptorSets(device, 1, &descriptor_write, 0, nullptr);
    }

    // 批量更新多个纹理到描述符集（用于数组纹理）
    static void update_array_to_descriptor_set(VkDevice device,
                                              VkDescriptorSet descriptor_set,
                                              const std::vector<vulkan_texture>& textures,
                                              uint32_t binding = 1,
                                              uint32_t start_array_element = 0) {
        if (textures.empty()) return;

        std::vector<VkDescriptorImageInfo> image_infos;
        image_infos.reserve(textures.size());

        for (const auto& texture : textures) {
            if (!texture.is_valid()) {
                std::println("纹理无效，无法更新描述符集");
                print_stacktrace_and_terminate();
            }

            VkDescriptorImageInfo image_info{};
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = texture.texture_image_view;
            image_info.sampler = texture.texture_sampler;
            image_infos.push_back(image_info);
        }

        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = descriptor_set;
        descriptor_write.dstBinding = binding;
        descriptor_write.dstArrayElement = start_array_element;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_write.descriptorCount = static_cast<uint32_t>(textures.size());
        descriptor_write.pImageInfo = image_infos.data();

        vkUpdateDescriptorSets(device, 1, &descriptor_write, 0, nullptr);
    }

    // 更新到独立的采样器和图像描述符（如果需要分离）
    void update_separate_to_descriptor_set(VkDevice device,
                                          VkDescriptorSet descriptor_set,
                                          uint32_t sampler_binding,
                                          uint32_t image_binding) const {
        if (!is_valid()) {
            std::println("纹理无效，无法更新描述符集");
            print_stacktrace_and_terminate();
        }

        // 更新采样器
        VkDescriptorImageInfo sampler_info{};
        sampler_info.sampler = texture_sampler;

        VkWriteDescriptorSet sampler_write{};
        sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sampler_write.dstSet = descriptor_set;
        sampler_write.dstBinding = sampler_binding;
        sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        sampler_write.descriptorCount = 1;
        sampler_write.pImageInfo = &sampler_info;

        // 更新图像视图
        VkDescriptorImageInfo image_info{};
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = texture_image_view;

        VkWriteDescriptorSet image_write{};
        image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        image_write.dstSet = descriptor_set;
        image_write.dstBinding = image_binding;
        image_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        image_write.descriptorCount = 1;
        image_write.pImageInfo = &image_info;

        VkWriteDescriptorSet writes[] = {sampler_write, image_write};
        vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
    }

    // 检查并更新描述符集（如果纹理有效）
    bool try_update_to_descriptor_set(VkDevice device,
                                     VkDescriptorSet descriptor_set,
                                     uint32_t binding = 1,
                                     VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) const {
        if (!is_valid()) {
            return false;
        }
        update_to_descriptor_set(device, descriptor_set, binding, descriptor_type);
        return true;
        }
};

struct vulkan_uniform_buffers {
    std::vector<VkBuffer> uniform_buffers;
    std::vector<VkDeviceMemory> uniform_buffers_memory;
    std::vector<void*> uniform_buffers_mapped;
    std::function<void(uniform_buffer_object&, const vulkan_core&)> update_mvp = [](uniform_buffer_object& ubo, const vulkan_core& core) {
        // 1. 模型矩阵：根据您的模型大小调整
        static float angle = 0.0f;
        angle += glm::radians(1.0f);  // 每秒旋转

        ubo.model = glm::mat4(1.0f);
        ubo.model = glm::scale(ubo.model, glm::vec3(2.0f,  2.0f, 2.0f));  // 缩放

        // 修正旋转：将模型的Z轴向上转为Y轴向上
        ubo.model = glm::rotate(ubo.model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        // 2. 向下平移（注意坐标系方向！）
        //ubo.model = glm::translate(ubo.model, glm::vec3(0.0f, -1.0f, 0.0f));  // 向下移动1个单位

        ubo.model = glm::rotate(ubo.model, angle, glm::vec3(0.0f, 0.0f, 1.0f));  // 持续旋转

        // 2. 视图矩阵：调整相机位置
        ubo.view = glm::lookAt(
            glm::vec3(3.0f, 3.0f, 3.0f),  // 相机位置
            glm::vec3(0.0f, 0.0f, 0.0f),  // 观察目标
            glm::vec3(0.0f, 0.0f, 1.0f)   // 上方向（Z轴向上）
        );

        // 3. 投影矩阵
        ubo.proj = glm::perspective(
            glm::radians(60.0f),  // 视野角度
            static_cast<float>(core.swap_chain_extent.width) /
                        static_cast<float>(core.swap_chain_extent.height),
            0.1f,    // 近平面
            100.0f   // 远平面
        );

        // Vulkan的Y轴是向下的，需要翻转
        ubo.proj[1][1] *= -1;
    };
    void update_uniform_buffer(const uint32_t current_image, const vulkan_core& core) const {
        uniform_buffer_object ubo{};
        update_mvp(ubo, core);
        // 复制数据
        if (current_image < uniform_buffers_mapped.size()) {
            memcpy(uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
        }
    }
    static std::function<void(uniform_buffer_object&, const vulkan_core&)> get_default_mvp_method() {
        return [](uniform_buffer_object& ubo, const vulkan_core& core) {
            // 1. 模型矩阵：根据您的模型大小调整
            static float angle = 0.0f;
            angle += glm::radians(1.0f);  // 每秒旋转

            ubo.model = glm::mat4(1.0f);
            ubo.model = glm::scale(ubo.model, glm::vec3(2.0f,  2.0f, 2.0f));  // 缩放

            // 修正旋转：将模型的Z轴向上转为Y轴向上
            ubo.model = glm::rotate(ubo.model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 0.1f));

            // 2. 向下平移（注意坐标系方向！）
            //ubo.model = glm::translate(ubo.model, glm::vec3(0.0f, -1.0f, 0.0f));  // 向下移动1个单位

            ubo.model = glm::rotate(ubo.model, angle, glm::vec3(0.0f, 0.0f, 1.0f));  // 持续旋转

            // 2. 视图矩阵：调整相机位置
            ubo.view = glm::lookAt(
                glm::vec3(3.0f, 3.0f, 3.0f),  // 相机位置
                glm::vec3(0.0f, 0.0f, 0.0f),  // 观察目标
                glm::vec3(0.0f, 0.0f, 1.0f)   // 上方向（Z轴向上）
            );

            // 3. 投影矩阵
            ubo.proj = glm::perspective(
                glm::radians(60.0f),  // 视野角度
                static_cast<float>(core.swap_chain_extent.width) /
                            static_cast<float>(core.swap_chain_extent.height),
                0.1f,    // 近平面
                100.0f   // 远平面
            );

            // Vulkan的Y轴是向下的，需要翻转
            ubo.proj[1][1] *= -1;
        };
    }
};

struct vulkan_renderable_object {
    vulkan_mesh_buffer mesh;
    vulkan_texture texture;
    vulkan_uniform_buffers uniform_buffers;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    void change_mvp_method(std::function<void(uniform_buffer_object&, const vulkan_core&)> method) {
        this->uniform_buffers.update_mvp = std::move(method);
    }
};

class vulkan_runtime {
    vulkan_core core;
    std::vector<vulkan_renderable_object> objects;
    vertex_buffer_creator creator;
    buffer_resource vertex_buffer;
    buffer_resource index_buffer;
    uint32_t index_count = 0;

    // 纹理资源
    VkImage texture_image = VK_NULL_HANDLE;
    VkDeviceMemory texture_image_memory = VK_NULL_HANDLE;
    VkImageView texture_image_view = VK_NULL_HANDLE;
    VkSampler texture_sampler = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    // 管线相关
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline = VK_NULL_HANDLE;

    // Uniform Buffer相关
    std::vector<VkBuffer> uniform_buffers;
    std::vector<VkDeviceMemory> uniform_buffers_memory;
    std::vector<void*> uniform_buffers_mapped;

    // 相机参数
    float camera_distance = 10.0f;  // 相机距离
    float rotation_angle = 0.0f;    // 旋转角度

    void create_uniform_buffers() {
        uniform_buffers.resize(core.swap_chain_images.size());
        uniform_buffers_memory.resize(core.swap_chain_images.size());
        uniform_buffers_mapped.resize(core.swap_chain_images.size());

        for (size_t i = 0; i < core.swap_chain_images.size(); i++) {
            VkDeviceSize buffer_size = sizeof(uniform_buffer_object);
            create_buffer(
                core.device,
                core.physical_device,
                buffer_size,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniform_buffers[i],
                uniform_buffers_memory[i]
            );

            vkMapMemory(core.device, uniform_buffers_memory[i], 0, buffer_size, 0, &uniform_buffers_mapped[i]);
        }
    }

    void update_descriptor_sets() const {
        if (descriptor_set == VK_NULL_HANDLE) {
            return;
        }

        std::array<VkWriteDescriptorSet, 1> descriptor_writes = {};

        // Uniform Buffer描述符
        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = uniform_buffers[0];  // 使用第一个缓冲区
        buffer_info.offset = 0;
        buffer_info.range = sizeof(uniform_buffer_object);

        descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet = descriptor_set;
        descriptor_writes[0].dstBinding = 0;
        descriptor_writes[0].dstArrayElement = 0;
        descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].pBufferInfo = &buffer_info;


        vkUpdateDescriptorSets(core.device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }

    // 创建管线布局
    void create_pipeline_layout() {
        VkPipelineLayoutCreateInfo pipeline_layout_info = {};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &core.descriptor_set_layout;
        pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(core.device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
            std::println("failed to create pipeline layout!");
            print_stacktrace_and_terminate();
        }
    }

    // 为特定网格创建管线
    [[nodiscard]] VkPipeline create_mesh_pipeline() {
        // 1. 验证核心对象
        if (core.device == VK_NULL_HANDLE) {
            std::println("Vulkan device is not initialized");
            print_stacktrace_and_terminate();
        }

        if (core.swap_chain_extent.width == 0 || core.swap_chain_extent.height == 0) {
            std::println("Swap chain extent is not initialized");
            print_stacktrace_and_terminate();
        }

        if (core.renderpass == VK_NULL_HANDLE) {
            std::println("Render pass is not initialized");
            print_stacktrace_and_terminate();
        }

        this->pipeline_layout = this->core.pipeline_layout;
        if (pipeline_layout == VK_NULL_HANDLE) {
            std::println("Pipeline layout is not initialized");
            print_stacktrace_and_terminate();
        }

        // 使用全局的顶点输入描述（与vertex结构匹配）
        auto binding_description = vertex::get_binding_descriptions();
        auto attribute_descriptions = vertex::get_attribute_descriptions();

        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(binding_description.size());
        vertex_input_info.pVertexBindingDescriptions = binding_description.data();
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
        vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

        // 读取并创建着色器模块
        VkShaderModule vert_shader_module = VK_NULL_HANDLE;
        VkShaderModule frag_shader_module = VK_NULL_HANDLE;
        auto vert_shader_code = read_file("vert.spv");
        auto frag_shader_code = read_file("frag.spv");

        if (vert_shader_code.empty() || frag_shader_code.empty()) {
            std::println("着色器文件为空");
            print_stacktrace_and_terminate();
        }

        vert_shader_module = create_shader_module(vert_shader_code, core.device);
        frag_shader_module = create_shader_module(frag_shader_code, core.device);

        VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
        vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vert_shader_stage_info.module = vert_shader_module;
        vert_shader_stage_info.pName = "main";

        VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
        frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_shader_stage_info.module = frag_shader_module;
        frag_shader_stage_info.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vert_shader_stage_info, frag_shader_stage_info};

        // 输入装配状态
        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        // 视口和裁剪
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(core.swap_chain_extent.width);
        viewport.height = static_cast<float>(core.swap_chain_extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = core.swap_chain_extent;

        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        // 光栅化状态
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        // 多重采样状态
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = core.msaa_samples;
        multisampling.minSampleShading = 1.0f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        // 颜色混合状态
        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo color_blending{};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;

        // 深度测试
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        // 创建管线
        VkGraphicsPipelineCreateInfo pipeline_info = {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = shaderStages;
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDepthStencilState = &depthStencil;
        pipeline_info.layout = pipeline_layout;
        pipeline_info.renderPass = core.renderpass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

        VkPipeline pipeline;
        if (vkCreateGraphicsPipelines(core.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS) {
            std::println("failed to create graphics pipeline!");
            print_stacktrace_and_terminate();
        }

        // 清理着色器模块
        vkDestroyShaderModule(core.device, vert_shader_module, nullptr);
        vkDestroyShaderModule(core.device, frag_shader_module, nullptr);

        return pipeline;
    }

public:
    vulkan_runtime()
        : core(),
          creator(core.device, core.physical_device, core.command_pool, core.graphics_queue),
          vertex_buffer(core.device),
          index_buffer(core.device) {
        // 创建单一的全局管线
        graphics_pipeline = create_mesh_pipeline();
        create_pipeline_layout();
    }

    vulkan_renderable_object& add_object(const std::vector<glm::vec3>& positions,
                    const std::vector<glm::vec3>& normals,
                    const std::vector<glm::vec2>& tex_coords,  // 纹理坐标
                    const std::vector<uint32_t>& indexes,
                    const std::string_view path = "") {
        vulkan_renderable_object object = {vulkan_mesh_buffer(this->core, positions, normals, tex_coords, indexes)};

        // 创建纹理资源
        vulkan_texture texture;
        if (path.empty()) {
            const stb_texture stb_texture1 = create_default_texture();
            create_texture_from_stb(stb_texture1);
            texture.texture_image = texture_image;
            texture.texture_image_memory = texture_image_memory;
            texture.texture_image_view = texture_image_view;
            texture.texture_sampler = texture_sampler;
            texture_image = VK_NULL_HANDLE;
            texture_image_memory = VK_NULL_HANDLE;
            texture_image_view = VK_NULL_HANDLE;
            texture_sampler = VK_NULL_HANDLE;
            std::cout << "纹理资源创建成功，尺寸: " << stb_texture1.width << "x" << stb_texture1.height << std::endl;
        } else {
            load_png_texture(path.data());
            texture.texture_image = texture_image;
            texture.texture_image_memory = texture_image_memory;
            texture.texture_image_view = texture_image_view;
            texture.texture_sampler = texture_sampler;
            texture_image = VK_NULL_HANDLE;
            texture_image_memory = VK_NULL_HANDLE;
            texture_image_view = VK_NULL_HANDLE;
            texture_sampler = VK_NULL_HANDLE;
        }

        create_uniform_buffers();
        object.uniform_buffers.uniform_buffers = std::move(this->uniform_buffers);
        object.uniform_buffers.uniform_buffers_mapped = std::move(this->uniform_buffers_mapped);
        object.uniform_buffers.uniform_buffers_memory = std::move(this->uniform_buffers_memory);

        object.texture = texture;
        object.descriptor_set = make_descriptor_sets(object.uniform_buffers.uniform_buffers[0]);
        object.texture.update_to_descriptor_set(core.device,object.descriptor_set);

        this->objects.push_back(std::move(object));
        return *(this->objects.end() - 1);
    }

    vulkan_renderable_object& add_object(const std::vector<vertex>& vertices, const std::vector<uint32_t>& indexes, const std::string_view path = "") {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> tex_coords;

        for (const auto&[position, normal, tex_coord] : vertices) {
            positions.push_back(position);
            normals.push_back(normal);
            tex_coords.push_back(tex_coord);
        }


        return this->add_object(positions, normals, tex_coords, indexes, "");
    }

    vulkan_renderable_object& add_object(const std::vector<vertex>& vertices,
        const std::vector<uint32_t>& indexes,
        const std::vector<unsigned char>& texture_data = {},
        int width = 0,
        int height = 0,
        VkFormat format = VK_FORMAT_UNDEFINED
        ) {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> tex_coords;

        for (const auto&[position, normal, tex_coord] : vertices) {
            positions.push_back(position);
            normals.push_back(normal);
            tex_coords.push_back(tex_coord);
        }

        vulkan_renderable_object object = {vulkan_mesh_buffer(this->core, positions, normals, tex_coords, indexes)};

        vulkan_texture texture;
        if (texture_data.empty()) {
            const stb_texture stb_texture1 = create_default_texture();
            create_texture_from_stb(stb_texture1);
            texture.texture_image = texture_image;
            texture.texture_image_memory = texture_image_memory;
            texture.texture_image_view = texture_image_view;
            texture.texture_sampler = texture_sampler;
            texture_image = VK_NULL_HANDLE;
            texture_image_memory = VK_NULL_HANDLE;
            texture_image_view = VK_NULL_HANDLE;
            texture_sampler = VK_NULL_HANDLE;
        } else {
            create_vulkan_texture(texture_data, width, height, format);
            texture.texture_image = texture_image;
            texture.texture_image_memory = texture_image_memory;
            texture.texture_image_view = texture_image_view;
            texture.texture_sampler = texture_sampler;
            texture_image = VK_NULL_HANDLE;
            texture_image_memory = VK_NULL_HANDLE;
            texture_image_view = VK_NULL_HANDLE;
            texture_sampler = VK_NULL_HANDLE;
        }

        create_uniform_buffers();
        object.uniform_buffers.uniform_buffers = std::move(this->uniform_buffers);
        object.uniform_buffers.uniform_buffers_mapped = std::move(this->uniform_buffers_mapped);
        object.uniform_buffers.uniform_buffers_memory = std::move(this->uniform_buffers_memory);

        object.texture = texture;
        object.descriptor_set = make_descriptor_sets(object.uniform_buffers.uniform_buffers[0]);
        object.texture.update_to_descriptor_set(core.device,object.descriptor_set);

        this->objects.push_back(std::move(object));

        return *(objects.end() - 1);
    }

    void update_uniform_buffer(const uint32_t current_image) const {
        uniform_buffer_object ubo{};

        // 1. 模型矩阵：根据您的模型大小调整
        static float angle = 0.0f;
        angle += glm::radians(1.0f);  // 每秒旋转

        ubo.model = glm::mat4(1.0f);
        ubo.model = glm::scale(ubo.model, glm::vec3(2.0f,  2.0f, 2.0f));  // 缩放

        // 修正旋转：将模型的Z轴向上转为Y轴向上
        ubo.model = glm::rotate(ubo.model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 0.1f));

        // 2. 向下平移（注意坐标系方向！）
        //ubo.model = glm::translate(ubo.model, glm::vec3(0.0f, -1.0f, 0.0f));  // 向下移动1个单位

        ubo.model = glm::rotate(ubo.model, angle, glm::vec3(0.0f, 0.0f, 1.0f));  // 持续旋转

        // 2. 视图矩阵：调整相机位置
        ubo.view = glm::lookAt(
            glm::vec3(3.0f, 3.0f, 3.0f),  // 相机位置
            glm::vec3(0.0f, 0.0f, 0.0f),  // 观察目标
            glm::vec3(0.0f, 0.0f, 1.0f)   // 上方向（Z轴向上）
        );

        // 3. 投影矩阵
        ubo.proj = glm::perspective(
            glm::radians(60.0f),  // 视野角度
            static_cast<float>(core.swap_chain_extent.width) /
                        static_cast<float>(core.swap_chain_extent.height),
            0.1f,    // 近平面
            100.0f   // 远平面
        );

        // Vulkan的Y轴是向下的，需要翻转
        ubo.proj[1][1] *= -1;

        // 复制数据
        if (current_image < uniform_buffers_mapped.size()) {
            memcpy(uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
        }
    }

    // 从PNG数据加载纹理
    void load_png_texture(const std::string& filepath) {
            png_texture_data texture_data = png_texture_loader::load_png_texture(filepath);

            // 创建Vulkan纹理资源
            png_texture_loader::create_vulkan_texture(
                core.device,
                core.physical_device,
                core.command_pool,
                core.graphics_queue,
                texture_data,
                this->texture_image,
                this->texture_image_memory,
                this->texture_image_view,
                this->texture_sampler
            );

            update_descriptor_sets();  // 更新描述符集
            std::cout << "PNG纹理加载成功: " << filepath << std::endl;
    }

    // 从PNG纹理数据加载
    void load_png_texture_from_data(const png_texture_data& texture_data) {
            // 创建Vulkan纹理资源
            png_texture_loader::create_vulkan_texture(
                core.device,
                core.physical_device,
                core.command_pool,
                core.graphics_queue,
                texture_data,
                this->texture_image,
                this->texture_image_memory,
                this->texture_image_view,
                this->texture_sampler
            );

            update_descriptor_sets();  // 更新描述符集
            std::cout << "PNG纹理从数据加载成功" << std::endl;
    }

    void render_frame_with_objects() {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(core.window, &width, &height);

        if (width == 0 || height == 0) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(16ms);
            return;
        }


        // 1. 等待前一帧
        core.wait_for_fences();

        // 2. 获取图像
        uint32_t image_index;
        VkResult result;
        core.get_image_index(image_index, result);

        // 处理交换链重建
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            core.recreate_swap_chain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            std::println(stderr,"无法获取交换链图像!");
            print_stacktrace_and_terminate();

        }

        // 3. 等待图像可用
        core.wait_usable_image(image_index);
        core.reset_fences();

        // 4. 更新Uniform Buffer
        //update_uniform_buffer(image_index);

        // 5. 记录命令缓冲区 - 使用mesh渲染
        record_command_buffer_with_objects(image_index);


        // 6. 提交并呈现
        core.submit_cmd_buffer();

        const VkSemaphore signal_semaphores[] = {
            core.render_finished_semaphores[core.current_frame]
        };

        result = core.present_image(signal_semaphores, image_index);

        // 7. 处理交换链重建
        if (result == VK_ERROR_OUT_OF_DATE_KHR ||
            result == VK_SUBOPTIMAL_KHR ||
            core.framebuffer_resized) {
            core.framebuffer_resized = false;
            core.recreate_swap_chain();
            }

        // 8. 前进到下一帧
        core.go_to_next_frame();
    }

    // 获取窗口
    [[nodiscard]] GLFWwindow* get_window() const { return core.window; }

    // 检查是否有有效的几何数据
    [[nodiscard]] bool has_geometry() const {
        return static_cast<bool>(vertex_buffer) && index_count > 0;
    }

private:
    // 创建描述符集
    void create_descriptor_sets() {
        if (!core.descriptor_set_layout) {
            return; // 如果没有描述符集布局，跳过
        }

        // 分配描述符集
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = core.descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &core.descriptor_set_layout;

        if (vkAllocateDescriptorSets(core.device, &alloc_info, &descriptor_set) != VK_SUCCESS) {
            std::println(stderr,"无法分配描述符集!");
        }
    }

    [[nodiscard]] VkDescriptorSet make_descriptor_sets(const VkBuffer& uniform_buffer) const {
        if (!core.descriptor_set_layout) {
            return VK_NULL_HANDLE;
        }

        // 分配描述符集
        VkDescriptorSet descriptor_set_;
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = core.descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &core.descriptor_set_layout;

        if (vkAllocateDescriptorSets(core.device, &alloc_info, &descriptor_set_) != VK_SUCCESS) {
            std::println("无法分配描述符集!");
            print_stacktrace_and_terminate();
        }

        // 关键：为这个描述符集配置Uniform Buffer！
        std::array<VkWriteDescriptorSet, 2> descriptor_writes{};

        // 1. Uniform Buffer描述符
        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = uniform_buffer;  // 使用第一个uniform buffer
        buffer_info.offset = 0;
        buffer_info.range = sizeof(uniform_buffer_object);

        descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet = descriptor_set_;
        descriptor_writes[0].dstBinding = 0;  // binding 0: UBO
        descriptor_writes[0].dstArrayElement = 0;
        descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].pBufferInfo = &buffer_info;

        // 2. 纹理描述符（暂时为空，稍后由纹理自己填充）
        descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].dstSet = descriptor_set_;
        descriptor_writes[1].dstBinding = 1;  // binding 1: 纹理
        descriptor_writes[1].dstArrayElement = 0;
        descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[1].descriptorCount = 1;
        descriptor_writes[1].pImageInfo = nullptr;  // 稍后由纹理填充

        // 更新描述符集（先只更新UBO，纹理稍后）
        vkUpdateDescriptorSets(core.device, 1, &descriptor_writes[0], 0, nullptr);

        return descriptor_set_;
    }


    // 从stb_texture创建Vulkan纹理
    void create_texture_from_stb(const stb_texture& texture) {
        // 创建暂存缓冲区
        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;

        create_buffer(
            core.device,
            core.physical_device,
            texture.image_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            staging_buffer_memory
        );

        // 复制数据到暂存缓冲区
        void* data;
        vkMapMemory(core.device, staging_buffer_memory, 0, texture.image_size, 0, &data);
        memcpy(data, texture.pixels, static_cast<size_t>(texture.image_size));
        vkUnmapMemory(core.device, staging_buffer_memory);

        // 创建纹理图像
        create_texture_image(
            core.device,
            core.physical_device,
            texture.width,
            texture.height,
            VK_FORMAT_R8G8B8A8_UNORM,  // 假设RGBA格式
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            texture_image,
            texture_image_memory
        );

        // 转换图像布局并复制数据
        transition_image_layout(
            core.device,
            core.command_pool,
            core.graphics_queue,
            texture_image,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );

        copy_buffer_to_image(
            core.device,
            core.command_pool,
            core.graphics_queue,
            staging_buffer,
            texture_image,
            static_cast<uint32_t>(texture.width),
            static_cast<uint32_t>(texture.height)
        );

        transition_image_layout(
            core.device,
            core.command_pool,
            core.graphics_queue,
            texture_image,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        // 清理暂存缓冲区
        vkDestroyBuffer(core.device, staging_buffer, nullptr);
        vkFreeMemory(core.device, staging_buffer_memory, nullptr);

        // 创建图像视图
        texture_image_view = create_image_view(
            core.device,
            texture_image,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        // 创建纹理采样器
        create_texture_sampler();
    }

    // 创建默认纹理（用于测试）
    static stb_texture create_default_texture() {
        // 创建一个简单的2x2测试纹理（红、绿、蓝、白）
        int width = 2;
        int height = 2;
        VkDeviceSize image_size = width * height * 4; // RGBA

        auto* pixels = new stbi_uc[16];

        // 红色像素 (255, 0, 0, 255)
        pixels[0] = 255; pixels[1] = 0; pixels[2] = 0; pixels[3] = 255;
        // 绿色像素 (0, 255, 0, 255)
        pixels[4] = 0; pixels[5] = 255; pixels[6] = 0; pixels[7] = 255;
        // 蓝色像素 (0, 0, 255, 255)
        pixels[8] = 0; pixels[9] = 0; pixels[10] = 255; pixels[11] = 255;
        // 白色像素 (255, 255, 255, 255)
        pixels[12] = 255; pixels[13] = 255; pixels[14] = 255; pixels[15] = 255;

        stb_texture texture{};
        texture.width = width;
        texture.height = height;
        texture.image_size = image_size;
        texture.pixels = pixels;

        return texture;
    }

    // 清理纹理资源
    void cleanup_texture_resources() {
        if (texture_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(core.device, texture_sampler, nullptr);
            texture_sampler = VK_NULL_HANDLE;
        }

        if (texture_image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(core.device, texture_image_view, nullptr);
            texture_image_view = VK_NULL_HANDLE;
        }

        if (texture_image_memory != VK_NULL_HANDLE) {
            vkFreeMemory(core.device, texture_image_memory, nullptr);
            texture_image_memory = VK_NULL_HANDLE;
        }

        if (texture_image != VK_NULL_HANDLE) {
            vkDestroyImage(core.device, texture_image, nullptr);
            texture_image = VK_NULL_HANDLE;
        }
    }

    // 通用缓冲区创建函数
    static void create_buffer(
        VkDevice device,
        VkPhysicalDevice physical_device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
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

    // 创建纹理图像
    static void create_texture_image(
        VkDevice device,
        VkPhysicalDevice physical_device,
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
        image_info.extent = { width, height, 1 };
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = format;
        image_info.tiling = tiling;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = usage;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &image_info, nullptr, &image) != VK_SUCCESS) {
            vkDestroyImage(device, image, nullptr);
            std::println("failed to create texture image!");
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
            std::println("failed to allocate texture image memory!");
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
    void create_texture_sampler() {
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

        if (vkCreateSampler(core.device, &sampler_info, nullptr, &texture_sampler) != VK_SUCCESS) {
            std::println("无法创建纹理采样器!");
            print_stacktrace_and_terminate();
        }
    }



    void record_command_buffer(const uint32_t image_index, const vulkan_texture& vk_texture) const {
        VkCommandBuffer cmd_buffer = core.command_buffers[core.current_frame];

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;

        if (vkBeginCommandBuffer(cmd_buffer, &begin_info) != VK_SUCCESS) {
            std::println("无法开始记录命令缓冲区!");
            print_stacktrace_and_terminate();
        }

        // 1. 开始渲染通道
        VkRenderPassBeginInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = core.renderpass;
        render_pass_info.framebuffer = core.swap_chain_framebuffers[image_index];
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = core.swap_chain_extent;

        std::array<VkClearValue, 2> clear_values{};
        clear_values[0].color = {{0.0f, 0.2f, 0.4f, 1.0f}};  // 蓝色背景
        clear_values[1].depthStencil = {1.0f, 0};

        render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
        render_pass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(cmd_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        // 2. 绑定管线
        vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);


        vk_texture.update_to_descriptor_set(core.device, descriptor_set);
        // 3. 绑定描述符集
        if (descriptor_set != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
        }

        // 4. 绑定顶点缓冲区 - 确保使用正确的偏移量
        if (vertex_buffer) {
            const VkBuffer vertex_buffers[] = {*vertex_buffer};
            constexpr VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd_buffer, 0, 1, vertex_buffers, offsets);
        } else {
            std::cout << "警告: 顶点缓冲区为空!" << std::endl;
        }

        // 5. 设置视口和裁剪
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(core.swap_chain_extent.width);
        viewport.height = static_cast<float>(core.swap_chain_extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = core.swap_chain_extent;
        vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

        // 6. 绘制命令 - 关键：使用索引绘制
        if (index_buffer && index_count > 0) {
            vkCmdBindIndexBuffer(cmd_buffer, *index_buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd_buffer, index_count, 1, 0, 0, 0);
        } else if (vertex_buffer) {
            std::cout << "无索引，直接绘制顶点" << std::endl;
            const auto vertex_count = static_cast<uint32_t>(vertex_buffer.size / sizeof(vertex));
            vkCmdDraw(cmd_buffer, vertex_count, 1, 0, 0);
        } else {
            std::cout << "无数据，绘制默认三角形" << std::endl;
            vkCmdDraw(cmd_buffer, 3, 1, 0, 0);
        }

        vkCmdEndRenderPass(cmd_buffer);

        if (vkEndCommandBuffer(cmd_buffer) != VK_SUCCESS) {
            std::println("无法结束记录命令缓冲区!");
            print_stacktrace_and_terminate();
        }
    }

    void record_command_buffer_with_objects(const uint32_t image_index) const {
        VkCommandBuffer cmd_buffer = core.command_buffers[core.current_frame];

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;

        if (vkBeginCommandBuffer(cmd_buffer, &begin_info) != VK_SUCCESS) {
            std::println("无法开始记录命令缓冲区!");
            print_stacktrace_and_terminate();
        }

        // 1. 开始渲染通道
        VkRenderPassBeginInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = core.renderpass;
        render_pass_info.framebuffer = core.swap_chain_framebuffers[image_index];
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = core.swap_chain_extent;

        std::array<VkClearValue, 2> clear_values{};
        clear_values[0].color = {{0.0f, 0.2f, 0.4f, 1.0f}};
        clear_values[1].depthStencil = {1.0f, 0};

        render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
        render_pass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(cmd_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        // 2. 绑定管线 - 使用全局管线
        vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

        // 3. 设置视口和裁剪
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(core.swap_chain_extent.width);
        viewport.height = static_cast<float>(core.swap_chain_extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = core.swap_chain_extent;
        vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);


        // 5. 遍历并绘制所有对象
        for (auto& object : objects) {
            object.uniform_buffers.update_uniform_buffer(image_index, this->core);
            // 绑定描述符集 - 每个对象有自己的描述符集
            if (object.descriptor_set != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      pipeline_layout, 0, 1, &object.descriptor_set, 0, nullptr);
            }

            // 纹理已经在创建对象时更新到描述符集，不需要每次渲染都更新

            const auto& mesh = object.mesh;
            mesh.bind(cmd_buffer);

            if (mesh.has_indices()) {
                vkCmdDrawIndexed(cmd_buffer,
                                static_cast<uint32_t>(mesh.get_index_count()),
                                1, 0, 0, 0);
            } else {
                vkCmdDraw(cmd_buffer,
                         static_cast<uint32_t>(mesh.get_vertex_count()),
                         1, 0, 0);
            }
        }


        vkCmdEndRenderPass(cmd_buffer);

        if (vkEndCommandBuffer(cmd_buffer) != VK_SUCCESS) {
            std::println("无法结束记录命令缓冲区!");
            print_stacktrace_and_terminate();
        }
    }


    void create_vulkan_texture(
        const std::vector<unsigned char>& texture,
        const int width,
        const int height,
        const VkFormat format
    ) {
        if (texture.empty()) {
            std::println("纹理数据无效，无法创建Vulkan纹理");
            print_stacktrace_and_terminate();
        }

        // 1. 创建临时缓冲区来传输纹理数据
        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;

        create_buffer(
            core.device,
            core.physical_device,
            texture.size(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            staging_buffer_memory
        );

        // 2. 复制纹理数据到暂存缓冲区
        void* data;
        vkMapMemory(core.device, staging_buffer_memory, 0, texture.size(), 0, &data);
        memcpy(data, texture.data(), texture.size());
        vkUnmapMemory(core.device, staging_buffer_memory);

        // 3. 创建纹理图像
        create_texture_image(
            core.device,
            core.physical_device,
            width,
            height,
            format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            texture_image,
            texture_image_memory
        );

        // 4. 从暂存缓冲区复制到纹理图像
        transition_image_layout(
            core.device,
            core.command_pool,
            core.graphics_queue,
            texture_image,
            format,  // 根据实际格式调整
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );

        copy_buffer_to_image(
            core.device,
            core.command_pool,
            core.graphics_queue,
            staging_buffer,
            texture_image,
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        );

        // 5. 转换纹理图像到着色器可读状态
        transition_image_layout(
            core.device,
            core.command_pool,
            core.graphics_queue,
            texture_image,
            format,  // 根据实际格式调整
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        // 6. 清理暂存缓冲区
        vkDestroyBuffer(core.device, staging_buffer, nullptr);
        vkFreeMemory(core.device, staging_buffer_memory, nullptr);

        // 7. 创建图像视图
        texture_image_view = create_image_view(
            core.device,
            texture_image,
            format,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        // 8. 创建纹理采样器
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
        if (vkCreateSampler(core.device, &sampler_info, nullptr, &texture_sampler) != VK_SUCCESS) {
            std::println("无法创建纹理采样器!");
            print_stacktrace_and_terminate();
        }
    }
public:
    // 禁用拷贝
    vulkan_runtime(const vulkan_runtime&) = delete;
    vulkan_runtime& operator=(const vulkan_runtime&) = delete;

    // 析构函数清理资源
    ~vulkan_runtime() {
        // 清理管线
        if (graphics_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(core.device, graphics_pipeline, nullptr);
        }
        if (pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(core.device, pipeline_layout, nullptr);
        }

        cleanup_texture_resources();
    }
};

#endif // VULKAN_PROJECT_VULKAN_RUNTIME_H