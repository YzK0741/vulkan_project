//
// Created by 小叶 on 2026/1/17.
//

#ifndef VULKAN_PROJECT_VULKAN_CORE_H
#define VULKAN_PROJECT_VULKAN_CORE_H

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <span>
#include <fstream>
#include <vector>

#include "create_info.h"
#include "vulkan_utility.h"
#include "vma/vma.h"



namespace vulkan_core {

    VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data
    );

    struct uniform_buffer_object {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    struct queue_family_indices {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> present_family;
        std::optional<uint32_t> compute_family;
        std::optional<uint32_t> transfer_family;

        [[nodiscard]] bool is_complete() const {
            return graphics_family.has_value() && present_family.has_value();
        }
    };

    struct logical_device {
        VkDevice device = VK_NULL_HANDLE;
        uint32_t graphics_family_index = 0;
        uint32_t present_family_index = 0;
        VkQueue graphics_queue = VK_NULL_HANDLE;
        VkQueue present_queue = VK_NULL_HANDLE;
        uint32_t queue_family_index = 0;
        // 可以添加更多队列：compute_queue, transfer_queue等
    };

    struct device_creation_info {
        queue_family_indices queue_families;
        std::vector<const char*> extensions;
        std::vector<const char*> validation_layers;
        VkPhysicalDeviceFeatures device_features{};
        const void* pNext = nullptr;  // 用于Vulkan 1.1+的特性链
    };

    struct swap_chain_support_details {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

    VkPhysicalDevice pick_suitable_device(const VkInstance& instance, VkSurfaceKHR surface);
    bool check_device_extension_support(const VkPhysicalDevice& physical_device,
                                    const std::vector<const char*>& required_extensions);
    logical_device create_logical_device(VkPhysicalDevice physical_device,
                                     const device_creation_info& create_info);
    queue_family_indices find_queue_families(const VkPhysicalDevice& device, const VkSurfaceKHR& surface);
    bool check_validation_layer_support(const std::vector<const char*>& validation_layers);
    // 获取GLFW需要的扩展
    std::vector<const char*> get_required_extensions();
    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);
    VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes);
    swap_chain_support_details query_swap_chain_support(const VkPhysicalDevice& device, const VkSurfaceKHR& surface);
    VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);

    std::vector<unsigned char> read_file(const std::string &filename);
    VkShaderModule create_shader_module(std::span<const unsigned char> code, const VkDevice& device);
    VkFormat find_depth_format(const VkPhysicalDevice &physical_device);
    VkImageView create_image_view(const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectFlags, const VkDevice& device);
    VkSampleCountFlagBits get_max_usable_sample_count(const VkPhysicalDevice& physical_device);


    constexpr uint32_t WIDTH = 1080;
    constexpr uint32_t HEIGHT = 960;


    struct core: enable_destruct_stack<core>{

        VkInstance instance = {};
        VkDevice device = {};
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        uint32_t graphics_family_index = 0;
        uint32_t present_family_index = 0;
#ifdef _DEBUG
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
#endif

        void init_instance();

        VkQueue graphics_queue = VK_NULL_HANDLE;
        VkQueue present_queue = VK_NULL_HANDLE;
        uint32_t graphics_queue_family = VK_QUEUE_FAMILY_IGNORED;

        void init_device_and_queue();


        GLFWwindow* window = nullptr;

        void init_window(int width = WIDTH, int height = HEIGHT, std::string_view window_name = "") noexcept;

        VMA vma;

        void init_vma() noexcept;

        VkSurfaceKHR surface = {};

        void init_surface();

        VkSwapchainKHR swap_chain = {};
        std::vector<VkImage> swap_chain_images;
        VkFormat swap_chain_image_format = {};
        VkExtent2D swap_chain_extent = {};
        dtor_type* swap_chain_cleanup = nullptr;

        void create_swap_chain();

        std::vector<VkImageView> swap_chain_image_views;
        dtor_type* image_view_cleanup = nullptr;

        void create_image_views();

        VkRenderPass renderpass = {};

        // 深度缓冲相关资源
        VkFormat depth_format = {};
        std::vector<VkImage> depth_images = {};
        std::vector<VkDeviceMemory> depth_image_memories = {};
        std::vector<VkImageView> depth_image_views = {};

        // 简化的深度图像创建函数
        void create_depth_image(VkImage& image, VkDeviceMemory& imageMemory, VkImageView& image_view) const;

        dtor_type* depth_resource_cleanup = nullptr;
        void create_depth_resources();


        dtor_type* renderpass_cleanup = nullptr;
        void create_renderpass();

        std::vector<VkFramebuffer> swap_chain_framebuffers;
        dtor_type* framebuffer_cleanup = nullptr;
        void create_framebuffers();

        VkCommandPool command_pool = {};
        std::vector<VkCommandBuffer> command_buffers = {};

        void create_command_pool();

        VkPipelineLayout pipeline_layout = {};
        dtor_type* pipeline_cleanup_cleanup = nullptr;
        void create_graphics_pipeline_layout();

        VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;

        void create_descriptor_set_layout();

        std::array<VkDescriptorSetLayout, 2> descriptor_set_layout_pbr = {VK_NULL_HANDLE, VK_NULL_HANDLE};

        void create_pbr_descriptor_set_layout();

        VkPipelineLayout pbr_pipeline_layout = {};
        dtor_type* pbr_pipeline_cleanup_cleanup = nullptr;

        void create_pbr_pipeline_layout();

        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

        void create_descriptor_pool();

        void create_command_buffers();

        // 图像可用信号量（当交换链图像准备好渲染时触发）
        std::vector<VkSemaphore> image_available_semaphores;

        // 渲染完成信号量（当渲染完成可以呈现时触发）
        std::vector<VkSemaphore> render_finished_semaphores;

        // 每帧的栅栏（确保同一帧的命令缓冲区不会同时执行）
        std::vector<VkFence> in_flight_fences;

        // 跟踪哪些帧正在使用中
        std::vector<VkFence> images_in_flight;

        // 当前帧索引
        size_t current_frame = 0;

        // 最大并发帧数（通常是交换链图像数量）
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

        void create_sync_objects();

        bool framebuffer_resized = false;

        void cleanup_swap_chain();

        void recreate_swap_chain();


        // MSAA相关
        VkSampleCountFlagBits msaa_samples = VK_SAMPLE_COUNT_1_BIT;  // 默认为无MSAA
        std::vector<VkImage> color_images;       // MSAA颜色缓冲图像
        std::vector<VkDeviceMemory> color_image_memories;
        std::vector<VkImageView> color_image_views;  // MSAA图像视图
        VkFormat color_format = VK_FORMAT_UNDEFINED;

        void create_msaa_image(
            const uint32_t& width,
            const uint32_t &height,
            const VkFormat &format,
            const VkSampleCountFlagBits &num_samples,
            const VkImageTiling &tiling,
            const VkImageUsageFlags &usage,
            const VkMemoryPropertyFlags &properties,
            VkImage& image,
            VkDeviceMemory& imageMemory
            ) const;

        dtor_type* color_resources_cleanup = nullptr;

        void create_color_resources();

        core();

        explicit core(const create_info &info);

        ~core();

        static void framebuffer_resize_callback(GLFWwindow* window, int width, int height);

        void wait_for_fences() const noexcept;

        void get_image_index(uint32_t& image_index, VkResult& result) const;

        void wait_usable_image(uint32_t image_index);

        void reset_fences() const;

        void submit_cmd_buffer() const;

        VkResult present_image(const VkSemaphore* signal_semaphores, uint32_t image_index) const;

        void go_to_next_frame();

        void copy_buffer(
            const VkBuffer& source,
            const VkBuffer& destination,
            VkDeviceSize size) const;
        // 禁用拷贝
        core(const core&) = delete;
        core& operator=(const core&) = delete;
    };
}

#endif //VULKAN_PROJECT_VULKAN_CORE_H