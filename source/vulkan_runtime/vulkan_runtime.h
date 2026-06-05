//
// Created by 小叶 on 2026/1/24.
//

#ifndef VULKAN_PROJECT_VULKAN_RUNTIME_H
#define VULKAN_PROJECT_VULKAN_RUNTIME_H

//#include <stb/stb_image_write.h>
#include <barrier>
#include <expected>
#include <thread>
#include <span>
#include <chrono>
#include "create_info.h"
#include "../vulkan_core/vulkan_core.h"
#include "../vulkan_core/vulkan_utility.h"
#include "../loader/png_loader.h"
#include "../vulkan_core/vulkan_buffer.h"

namespace vulkan_runtime {

    using namespace vulkan_core;

    struct vulkan_mesh_buffer {
    private:
        std::reference_wrapper<const core> core_ref;
        size_t vertex_count = 0;
        size_t index_count = 0;

        // 交错顶点结构，与vertex结构保持一致
        using interleaved_vertex = vertex;

        template <typename T>
        static void create_buffer_from_vector(
            core& core,
            const std::vector<T>& data,
            VkBufferUsageFlags usage_flags,
            VkBuffer& buffer,
            VkDeviceMemory& device_memory
            );

        void cleanup();

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
        explicit vulkan_mesh_buffer(core& core,
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
        explicit vulkan_mesh_buffer(core& core,
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
        explicit vulkan_mesh_buffer(core& core,
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
        void update_to_descriptor_set(const VkDevice& device,
                                     const VkDescriptorSet& descriptor_set,
                                     const uint32_t binding = 1,  // 通常纹理绑定在binding 1
                                     const VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     const uint32_t array_element = 0) const {
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
        static void update_array_to_descriptor_set(const VkDevice& device,
                                                  const VkDescriptorSet& descriptor_set,
                                                  const std::vector<vulkan_texture>& textures,
                                                  const uint32_t binding = 1,
                                                  const uint32_t start_array_element = 0) {
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
        std::function<void(uniform_buffer_object&, const core&)> update_mvp;
        void update_uniform_buffer(const uint32_t current_image, const core& core) const {
            uniform_buffer_object ubo{};
            update_mvp(ubo, core);
            // 复制数据
            if (current_image < uniform_buffers_mapped.size()) {
                memcpy(uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
            }
        }
        static std::function<void(uniform_buffer_object&, const core&)> get_default_mvp_method() {
            return [](uniform_buffer_object& ubo, const core& core) {
                // 1. 模型矩阵：根据您的模型大小调整
                static float angle = 0.0f;
                angle += glm::radians(0.1f);  // 每秒旋转

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

        void change_mvp_method(std::function<void(uniform_buffer_object&, const core&)> method) {
            this->uniform_buffers.update_mvp = std::move(method);
        }
    };

    class runtime {
        core core;
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
        VkPipelineLayout pbr_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline pbr_graphics_pipeline = VK_NULL_HANDLE;

        // Uniform Buffer相关
        std::vector<VkBuffer> uniform_buffers;
        std::vector<VkDeviceMemory> uniform_buffers_memory;
        std::vector<void*> uniform_buffers_mapped;

        // 相机参数
        float camera_distance = 10.0f;  // 相机距离
        float rotation_angle = 0.0f;    // 旋转角度

        void create_uniform_buffers();

        void update_descriptor_sets() const;

        // 创建管线布局
        void create_pipeline_layout();

        // 为特定网格创建管线
        void create_pipeline();
        void create_pbr_pipeline();

        std::optional<std::jthread> render_thread;
        std::condition_variable cv;
        std::mutex render_lock;
        bool pause_render_thread = false;

        std::atomic_int frames_in_second = {0};
        std::atomic_int current_frames = {0};
        std::chrono::steady_clock::time_point last_whole_second = std::chrono::steady_clock::now();

        std::expected<void, std::string_view> restore_rendering_no_lock();


    public:
        runtime();

        explicit runtime(const create_info &create_info);

        vulkan_renderable_object& add_object(const std::vector<glm::vec3>& positions,
                        const std::vector<glm::vec3>& normals,
                        const std::vector<glm::vec2>& tex_coords,  // 纹理坐标
                        const std::vector<uint32_t>& indexes,
                        std::string_view path = "");

        vulkan_renderable_object& add_object(
            const std::vector<vertex>& vertices,
            const std::vector<uint32_t>& indexes,
            std::string_view path = ""
            );

        vulkan_renderable_object& add_object(const std::vector<vertex>& vertices,
            const std::vector<uint32_t>& indexes,
            const std::vector<unsigned char>& texture_data = {},
            int width = 0,
            int height = 0,
            VkFormat format = VK_FORMAT_UNDEFINED
            );

        void render_frame();

        std::expected<void, std::string_view> start_rendering();

        std::expected<void, std::string_view> stop_rendering();

        std::expected<void, std::string_view> pause_rendering();

        std::expected<void, std::string_view> restore_rendering();

        std::unique_lock<std::mutex> wait_frame();

        int get_frame_speed() const;

        // 获取窗口
        [[nodiscard]] GLFWwindow* get_window() const { return core.window; }

    private:
        void update_uniform_buffer(uint32_t current_image) const;

        void do_render_frame();

        // 从PNG数据加载纹理
        void load_png_texture(const std::string& filepath);

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
            std::println("PNG纹理从数据加载成功");
        }


        // 检查是否有有效的几何数据
        [[nodiscard]] bool has_geometry() const;


        // 创建描述符集
        void create_descriptor_sets();

        [[nodiscard]] VkDescriptorSet make_descriptor_sets(const VkBuffer& uniform_buffer) const;


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

        vulkan_texture create_texture(const stb_texture &texture);

        vulkan_texture create_texture(const std::vector<unsigned char> &texture, int width, int height, VkFormat format);

        // 创建默认纹理（用于测试）
        static stb_texture create_default_texture();

        // 清理纹理资源
        void cleanup_texture_resources();

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
        static VkCommandBuffer begin_single_time_commands(VkDevice device, VkCommandPool command_pool);

        // 结束单次命令缓冲区
        static void end_single_time_commands(
            VkDevice device,
            VkCommandPool command_pool,
            VkQueue graphics_queue,
            VkCommandBuffer command_buffer
        );

        // 创建图像视图
        static VkImageView create_image_view(
            VkDevice device,
            VkImage image,
            VkFormat format,
            VkImageAspectFlags aspect_flags
        );

        // 创建纹理采样器
        void create_texture_sampler();

        void record_command_buffer(uint32_t image_index, const vulkan_texture& vk_texture) const;

        void record_command_buffer_with_objects(uint32_t image_index) const;


        void create_vulkan_texture(
            const std::vector<unsigned char>& texture,
            int width,
            int height,
            VkFormat format
        );
    public:
        // 禁用拷贝
        runtime(const runtime&) = delete;
        runtime& operator=(const runtime&) = delete;

        // 析构函数清理资源
        ~runtime();
    };
}

#endif // VULKAN_PROJECT_VULKAN_RUNTIME_H