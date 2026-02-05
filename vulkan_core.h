//
// Created by 小叶 on 2026/1/17.
//

#ifndef VULKAN_PROJECT_VULKAN_CORE_H
#define VULKAN_PROJECT_VULKAN_CORE_H

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <fstream>
#include <vector>

#include "vulkan_utility.h"

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
    std::vector<VkPresentModeKHR> presentModes;
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

std::vector<char> read_file(const std::string& filename);
VkShaderModule create_shader_module(const std::vector<char>& code, const VkDevice& device);
VkFormat find_depth_format(const VkPhysicalDevice &physical_device);
VkImageView create_image_view(const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectFlags, const VkDevice& device);
VkSampleCountFlagBits get_max_usable_sample_count(const VkPhysicalDevice& physical_device);

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;


struct vulkan_core {
    VkInstance instance = {};
    VkDevice device = {};
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    uint32_t graphics_family_index = 0;
    uint32_t present_family_index = 0;

    void init_instance() {
        VkApplicationInfo app_info {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Hello Triangle";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "No Engine";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;

        // 获取并设置GLFW所需的扩展
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

        // 在调试模式下添加调试扩展
    #ifdef _DEBUG
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        std::cout << "添加调试扩展: " << VK_EXT_DEBUG_UTILS_EXTENSION_NAME << std::endl;
    #endif

        // 输出所有扩展
        std::cout << "请求的实例扩展 (" << extensions.size() << "):" << std::endl;
        for (const auto& ext : extensions) {
            std::cout << "  - " << ext << std::endl;
        }

        create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        create_info.ppEnabledExtensionNames = extensions.data();

        // 启用验证层
        std::vector<const char*> validation_layers;
    #ifdef _DEBUG
        validation_layers = {
            "VK_LAYER_KHRONOS_validation"
        };

        // 检查验证层支持
        if (check_validation_layer_support(validation_layers)) {
            create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
            create_info.ppEnabledLayerNames = validation_layers.data();
            std::cout << "验证层已启用 (" << validation_layers.size() << "):" << std::endl;
            for (const auto& layer : validation_layers) {
                std::cout << "  - " << layer << std::endl;
            }
        } else {
            std::cout << "警告：验证层不可用，继续运行" << std::endl;
            create_info.enabledLayerCount = 0;
        }
    #else
        std::cout << "发布模式：验证层已禁用" << std::endl;
        create_info.enabledLayerCount = 0;
    #endif

        // 创建Vulkan实例
        std::cout << "正在创建Vulkan实例..." << std::endl;

        if (VkResult result = vkCreateInstance(&create_info, nullptr, &this->instance); result != VK_SUCCESS) {
            // 提供更详细的错误信息
            std::string error_msg = "无法创建Vulkan实例，错误码: ";
            switch (result) {
                case VK_ERROR_OUT_OF_HOST_MEMORY: error_msg += "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
                case VK_ERROR_OUT_OF_DEVICE_MEMORY: error_msg += "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
                case VK_ERROR_INITIALIZATION_FAILED: error_msg += "VK_ERROR_INITIALIZATION_FAILED"; break;
                case VK_ERROR_LAYER_NOT_PRESENT: error_msg += "VK_ERROR_LAYER_NOT_PRESENT"; break;
                case VK_ERROR_EXTENSION_NOT_PRESENT: error_msg += "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
                case VK_ERROR_INCOMPATIBLE_DRIVER: error_msg += "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
                default: error_msg += std::to_string(static_cast<int>(result)); break;
            }
            throw std::runtime_error(error_msg);
        }

        std::cout << "Vulkan实例创建成功" << std::endl;
    }

    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;

    void init_device_and_queue() {
        // 创建设备
        device_creation_info info;

        // 需要在创建表面后获取队列族信息
        info.queue_families = find_queue_families(physical_device, surface);  // 添加surface参数

        // 启用必要扩展
        info.extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        // 检查扩展支持
        if (!check_device_extension_support(physical_device, info.extensions)) {
            throw std::runtime_error("Required device extensions not supported");
        }

        // 可选：启用Vulkan 1.1+特性
        VkPhysicalDeviceVulkan11Features vulkan11_features{};
        vulkan11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        vulkan11_features.storageBuffer16BitAccess = VK_TRUE;
        info.pNext = &vulkan11_features;

        const logical_device logical_device_instance = create_logical_device(physical_device, info);

        this->device = logical_device_instance.device;
        this->graphics_queue = logical_device_instance.graphics_queue;
        this->present_queue = logical_device_instance.present_queue;
        this->graphics_family_index = logical_device_instance.graphics_family_index;
        this->present_family_index = logical_device_instance.present_family_index;
    }


    GLFWwindow* window = nullptr;

    void init_window() noexcept {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    VkSurfaceKHR surface = {};

    void init_surface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("无法创建窗口表面");
        }
    }

    VkSwapchainKHR swap_chain = {};
    std::vector<VkImage> swap_chain_images;
    VkFormat swap_chain_image_format = {};
    VkExtent2D swap_chain_extent = {};

    void create_swap_chain() {
        const swap_chain_support_details swap_chain_support = query_swap_chain_support(this->physical_device, this->surface);

        // 添加检查：
        if (swap_chain_support.formats.empty() || swap_chain_support.presentModes.empty()) {
            throw std::runtime_error("Swap chain not adequately supported");
        }

        const VkSurfaceFormatKHR surface_format = choose_swap_surface_format(swap_chain_support.formats);
        const VkPresentModeKHR present_mode = choose_swap_present_mode(swap_chain_support.presentModes);
        const VkExtent2D extent = choose_swap_extent(swap_chain_support.capabilities, this->window);

        uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;

        if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount) {
            image_count = swap_chain_support.capabilities.maxImageCount;
        }

        image_count = std::max(image_count, swap_chain_support.capabilities.minImageCount);

        VkSwapchainCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = surface;

        create_info.minImageCount = image_count;
        create_info.imageFormat = surface_format.format;
        create_info.imageColorSpace = surface_format.colorSpace;
        create_info.imageExtent = extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        queue_family_indices indices = find_queue_families(this->physical_device, this->surface);
        uint32_t queueFamilyIndices[] = {indices.graphics_family.value(), indices.present_family.value()};

        if (indices.graphics_family != indices.present_family) {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            create_info.queueFamilyIndexCount = 0; // Optional
            create_info.pQueueFamilyIndices = nullptr; // Optional
        }

        create_info.preTransform = swap_chain_support.capabilities.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.presentMode = present_mode;
        create_info.clipped = VK_TRUE;
        create_info.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &create_info, nullptr, &this->swap_chain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }


        vkGetSwapchainImagesKHR(device, this->swap_chain, &image_count, nullptr);
        this->swap_chain_images.resize(image_count);
        vkGetSwapchainImagesKHR(device, this->swap_chain, &image_count, this->swap_chain_images.data());

        this->swap_chain_image_format = surface_format.format;
        this->swap_chain_extent = extent;
    }

    std::vector<VkImageView> swap_chain_image_views;

    void create_image_views() {
        this->swap_chain_image_views.resize(this->swap_chain_images.size());
        for (size_t i = 0; i < this->swap_chain_images.size(); i++) {
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = this->swap_chain_images[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swap_chain_image_format;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            if (vkCreateImageView(device, &createInfo, nullptr, &this->swap_chain_image_views[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    VkRenderPass renderpass = {};

    // 深度缓冲相关资源
    VkFormat depth_format = {};
    std::vector<VkImage> depth_images = {};
    std::vector<VkDeviceMemory> depth_image_memories = {};
    std::vector<VkImageView> depth_image_views = {};

    // 简化的深度图像创建函数
    void create_depth_image(VkImage& image, VkDeviceMemory& imageMemory, VkImageView& imageView) const {
        // 使用类内的交换链尺寸
        const VkExtent2D& extent = this->swap_chain_extent;  // 假设这是类成员

        // 1. 创建图像
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = { extent.width, extent.height, 1 };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = this->depth_format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = this->msaa_samples;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            vkDestroyImage(device, image, nullptr);
            throw std::runtime_error("failed to create depth image!");
        }

        // 2. 分配内存
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits,
                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, this->physical_device);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate depth image memory!");
        }

        vkBindImageMemory(device, image, imageMemory, 0);

        // 3. 创建图像视图
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = this->depth_format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create depth image view!");
        }
    }

    void create_depth_resources() {
        depth_format = find_depth_format(this->physical_device);

        // 先测试深度格式是否有效
        if (depth_format == VK_FORMAT_UNDEFINED) {
            throw std::runtime_error("无法找到支持的深度格式");
        }

        depth_images.resize(swap_chain_image_views.size());
        depth_image_views.resize(swap_chain_image_views.size());
        depth_image_memories.resize(swap_chain_image_views.size());  // 确保分配内存

        for (size_t i = 0; i < swap_chain_image_views.size(); i++) {
            // 直接调用 create_depth_image，但确保参数正确
            create_depth_image(depth_images[i], depth_image_memories[i], depth_image_views[i]);
        }
    }


    void create_renderpass() {
        // 1. 颜色附件（现在是MSAA的）
        VkAttachmentDescription color_attachment = {};
        color_attachment.format = swap_chain_image_format;
        color_attachment.samples = msaa_samples;  // 使用MSAA采样
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // 关键：如果有MSAA，最终布局需要解析到呈现图像
        if (msaa_samples > VK_SAMPLE_COUNT_1_BIT) {
            color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        } else {
            color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

        // 2. 深度附件（也需要MSAA）
        VkAttachmentDescription depth_attachment = {};
        depth_attachment.format = depth_format;
        depth_attachment.samples = msaa_samples;  // 深度也要MSAA
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // 3. 颜色解析附件（只在有MSAA时需要）
        VkAttachmentDescription color_resolve_attachment = {};
        if (msaa_samples > VK_SAMPLE_COUNT_1_BIT) {
            color_resolve_attachment.format = swap_chain_image_format;
            color_resolve_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            color_resolve_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color_resolve_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color_resolve_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color_resolve_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            color_resolve_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            color_resolve_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

        VkAttachmentReference color_attachment_ref{};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_attachment_ref{};
        depth_attachment_ref.attachment = 1;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_resolve_ref{};
        if (msaa_samples > VK_SAMPLE_COUNT_1_BIT) {
            color_resolve_ref.attachment = 2;
            color_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;
        subpass.pDepthStencilAttachment = &depth_attachment_ref;

        // 关键：设置解析附件
        if (msaa_samples > VK_SAMPLE_COUNT_1_BIT) {
            subpass.pResolveAttachments = &color_resolve_ref;
        } else {
            subpass.pResolveAttachments = nullptr;
        }

        // 构建附件数组
        std::vector<VkAttachmentDescription> attachments;
        attachments.push_back(color_attachment);
        attachments.push_back(depth_attachment);

        if (msaa_samples > VK_SAMPLE_COUNT_1_BIT) {
            attachments.push_back(color_resolve_attachment);
        }

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        // 添加子流程依赖
        std::array<VkSubpassDependency, 2> dependencies = {};
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderpass) != VK_SUCCESS) {
            throw std::runtime_error("无法创建渲染通道!");
        }
    }

    std::vector<VkFramebuffer> swap_chain_framebuffers;


    void create_framebuffers() {
        swap_chain_framebuffers.resize(swap_chain_image_views.size());

        for (size_t i = 0; i < swap_chain_image_views.size(); i++) {
            std::vector<VkImageView> attachments;

            // 如果有MSAA，第一个是MSAA颜色附件
            if (msaa_samples > VK_SAMPLE_COUNT_1_BIT) {
                attachments.push_back(color_image_views[i]);  // MSAA颜色附件
            } else {
                attachments.push_back(swap_chain_image_views[i]);  // 普通颜色附件
            }

            attachments.push_back(depth_image_views[i]);  // 深度附件

            // 如果有MSAA，添加解析附件
            if (msaa_samples > VK_SAMPLE_COUNT_1_BIT) {
                attachments.push_back(swap_chain_image_views[i]);  // 解析到交换链图像
            }

            VkFramebufferCreateInfo framebuffer_create_info{};
            framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_create_info.renderPass = renderpass;
            framebuffer_create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebuffer_create_info.pAttachments = attachments.data();
            framebuffer_create_info.width = swap_chain_extent.width;
            framebuffer_create_info.height = swap_chain_extent.height;
            framebuffer_create_info.layers = 1;

            if (vkCreateFramebuffer(this->device, &framebuffer_create_info, nullptr, &this->swap_chain_framebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("无法创建帧缓冲区!");
            }
        }
    }

    VkCommandPool command_pool = {};
    std::vector<VkCommandBuffer> command_buffers = {};

    void create_command_pool() {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphics_family_index;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &command_pool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }


    VkShaderModule vert_shader_module = {};
    VkShaderModule frag_shader_module = {};
    VkPipelineLayout pipeline_layout = {};
    VkPipeline graphics_pipeline = {};

    void create_graphics_pipeline() {
        // 1. 读取并创建着色器模块
        this->vert_shader_module = VK_NULL_HANDLE;
        this->frag_shader_module = VK_NULL_HANDLE;

        try {
            auto vert_shader_code = read_file("vert.spv");  // 需要先编译着色器
            auto frag_shader_code = read_file("frag.spv");

            vert_shader_module = create_shader_module(vert_shader_code, this->device);
            frag_shader_module = create_shader_module(frag_shader_code, this->device);
        } catch (const std::exception& e) {
            std::cout << "着色器文件加载失败: " << e.what() << std::endl;
            std::cout << "请确保 shader.vert.spv 和 shader.frag.spv 文件存在于可执行文件目录中" << std::endl;

            // 创建一个简单的内联着色器或返回错误
            throw std::runtime_error("着色器文件缺失，请先编译着色器");
        };

        // 2. 着色器阶段配置
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

        // 3. 顶点输入状态（使用新的Vertex结构）
        auto binding_description = vertex::get_binding_descriptions();
        auto attribute_descriptions = vertex::get_attribute_descriptions();

        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(binding_description.size());
        vertex_input_info.pVertexBindingDescriptions = binding_description.data();
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
        vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

        // 4. 输入装配状态
        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        // 5. 视口和裁剪
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swap_chain_extent.width);
        viewport.height = static_cast<float>(swap_chain_extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swap_chain_extent;

        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        // 6. 光栅化状态
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // 改为逆时针，通常更适合标准模型
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

        // 7. 多重采样状态
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;  // 可以启用样本着色
        multisampling.rasterizationSamples = msaa_samples;  // 使用MSAA采样数
        multisampling.minSampleShading = 1.0f;  // 可以调整为小于1.0以启用样本着色
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        // 8. 颜色混合状态（每个帧缓冲区）
        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo color_blending{};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.logicOp = VK_LOGIC_OP_COPY;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;
        color_blending.blendConstants[0] = 0.0f;
        color_blending.blendConstants[1] = 0.0f;
        color_blending.blendConstants[2] = 0.0f;
        color_blending.blendConstants[3] = 0.0f;

        // 9. 动态状态（可动态修改的部分）
        std::vector<VkDynamicState> dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic_state = {};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dynamic_state.pDynamicStates = dynamic_states.data();

        // 10. 管线布局（着色器 uniform 和 push 常量）
        VkPipelineLayoutCreateInfo pipeline_layout_info = {};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &this->descriptor_set_layout;
        pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(this->device, &pipeline_layout_info, nullptr, &this->pipeline_layout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        //深度测试
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.pNext = nullptr;
        depthStencil.flags = 0;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;      // 可选的深度边界测试
        depthStencil.maxDepthBounds = 1.0f;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};  // 模板测试前端状态
        depthStencil.back = {};   // 模板测试后端状态

        // 11. 创建图形管线
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
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = pipeline_layout;
        pipeline_info.renderPass = this->renderpass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.basePipelineIndex = -1;
        pipeline_info.pDepthStencilState = &depthStencil;

        if (vkCreateGraphicsPipelines(this->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &this->graphics_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
    }



    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;

    void create_descriptor_set_layout() {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {};

        // Uniform Buffer绑定（binding 0）
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        bindings[0].pImmutableSamplers = nullptr;

        // 纹理采样器绑定（binding 1）
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].pImmutableSamplers = nullptr;

        // 创建描述符集布局
        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = bindings.size();
        layout_info.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &descriptor_set_layout) != VK_SUCCESS) {
            throw std::runtime_error("无法创建描述符集布局!");
        }
    }

    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

    void create_descriptor_pool() {
        std::vector<VkDescriptorPoolSize> pool_sizes;

        pool_sizes.push_back({
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100
        });

        pool_sizes.push_back({
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100
        });

        VkDescriptorPoolCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        info.maxSets = 100;
        info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        info.pPoolSizes = pool_sizes.data();
        if (vkCreateDescriptorPool(this->device, &info, nullptr, &this->descriptor_pool)) {
            throw std::runtime_error("failed in creating descriptor pool");
        }
    }

    void create_command_buffers() {
        // 如果已有命令缓冲区，先释放
        if (!command_buffers.empty()) {
            vkFreeCommandBuffers(device, command_pool, static_cast<uint32_t>(command_buffers.size()), command_buffers.data());
            command_buffers.clear();
        }

        command_buffers.resize(swap_chain_framebuffers.size());

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = command_pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(command_buffers.size());

        if (vkAllocateCommandBuffers(device, &allocInfo, command_buffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("无法分配命令缓冲区!");
        }
    }

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

    void create_sync_objects() {
        image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
        images_in_flight.resize(swap_chain_images.size(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 初始为已触发状态

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available_semaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished_semaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
                }
        }
    }

    void cleanup() const noexcept {
        // 清理命令池和命令缓冲区
        if (!command_buffers.empty()) {
            vkFreeCommandBuffers(this->device, this->command_pool,
                               static_cast<uint32_t>(command_buffers.size()), command_buffers.data());
        }
        if (command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(this->device, this->command_pool, nullptr);
        }

        // 清理帧缓冲区
        for (const auto& framebuffer : swap_chain_framebuffers) {
            vkDestroyFramebuffer(this->device, framebuffer, nullptr);
        }

        // 清理同步对象
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(this->device, image_available_semaphores[i], nullptr);
            vkDestroySemaphore(this->device, render_finished_semaphores[i], nullptr);
            vkDestroyFence(this->device, in_flight_fences[i], nullptr);
        }

        // 清理MSAA颜色资源
        for (size_t i = 0; i < color_image_views.size(); i++) {
            vkDestroyImageView(this->device, color_image_views[i], nullptr);
            vkDestroyImage(this->device, color_images[i], nullptr);
            vkFreeMemory(this->device, color_image_memories[i], nullptr);
        }

        // 清理深度资源
        for (size_t i = 0; i < depth_image_views.size(); i++) {
            vkDestroyImageView(this->device, depth_image_views[i], nullptr);
            vkDestroyImage(this->device, depth_images[i], nullptr);
            vkFreeMemory(this->device, depth_image_memories[i], nullptr);
        }

        // 清理交换链资源
        for (const auto& image_view : this->swap_chain_image_views) {
            vkDestroyImageView(this->device, image_view, nullptr);
        }

        if (swap_chain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(this->device, this->swap_chain, nullptr);
        }

        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(this->device, nullptr);
        }

        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(this->instance, this->surface, nullptr);
        }

        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(this->instance, nullptr);
        }

        if (window != nullptr) {
            glfwDestroyWindow(this->window);
        }

        glfwTerminate();
    }

    bool framebuffer_resized = false;

    /*
    void record_command_buffer(const VkCommandBuffer& command_buffer, const uint32_t image_index) const {
        throw std::runtime_error("vulkan_core::record_command_buffer should not be called directly!");
        // 开始记录命令缓冲区
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;  // 可选
        begin_info.pInheritanceInfo = nullptr;  // 可选

        if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
            throw std::runtime_error("无法开始记录命令缓冲区!");
        }

        // ✅ 正确设置清除值：颜色和深度
        std::array<VkClearValue, 2> clear_values{};
        clear_values[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clear_values[1].depthStencil = {1.0f, 0};

        // 开始渲染通道
        VkRenderPassBeginInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = renderpass;
        render_pass_info.framebuffer = swap_chain_framebuffers[image_index];
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = swap_chain_extent;

        // ✅ 正确设置清除值 - 一次性完成
        render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
        render_pass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        // 绑定图形管线
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

        // 设置动态视口
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swap_chain_extent.width);
        viewport.height = static_cast<float>(swap_chain_extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        // 设置动态裁剪
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swap_chain_extent;
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        // 绘制命令（这里绘制一个三角形）
        vkCmdDraw(command_buffer, 3, 1, 0, 0);

        // 结束渲染通道
        vkCmdEndRenderPass(command_buffer);

        // 结束记录命令缓冲区
        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("无法结束记录命令缓冲区!");
        }
    }*/

    void cleanup_swap_chain() {
        // 清理帧缓冲区
        for (const auto& framebuffer : swap_chain_framebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        swap_chain_framebuffers.clear();

        // 清理颜色资源（MSAA）
        for (size_t i = 0; i < color_image_views.size(); i++) {
            vkDestroyImageView(device, color_image_views[i], nullptr);
            vkDestroyImage(device, color_images[i], nullptr);
            vkFreeMemory(device, color_image_memories[i], nullptr);
        }
        color_image_views.clear();
        color_images.clear();
        color_image_memories.clear();

        // 清理深度资源
        for (size_t i = 0; i < depth_image_views.size(); i++) {
            vkDestroyImageView(device, depth_image_views[i], nullptr);
            vkDestroyImage(device, depth_images[i], nullptr);
            vkFreeMemory(device, depth_image_memories[i], nullptr);
        }
        depth_image_views.clear();
        depth_images.clear();
        depth_image_memories.clear();

        // ... 其余清理代码 ...
    }

    void recreate_swap_chain() {

        this->create_swap_chain();
        this->create_image_views();
        this->create_depth_resources();

        if (msaa_samples > VK_SAMPLE_COUNT_1_BIT) {
            create_color_resources();
        }

        this->create_renderpass();
        this->create_graphics_pipeline();
        this->create_framebuffers();
        this->create_command_buffers();

        images_in_flight.resize(swap_chain_images.size(), VK_NULL_HANDLE);
    }


    // MSAA相关
    VkSampleCountFlagBits msaa_samples = VK_SAMPLE_COUNT_1_BIT;  // 默认为无MSAA
    std::vector<VkImage> color_images;       // MSAA颜色缓冲图像
    std::vector<VkDeviceMemory> color_image_memories;
    std::vector<VkImageView> color_image_views;  // MSAA图像视图
    VkFormat color_format = VK_FORMAT_UNDEFINED;

    void create_msaa_image(const uint32_t& width, const uint32_t &height, const VkFormat &format,
                           const VkSampleCountFlagBits &num_samples,
                           const VkImageTiling &tiling, const VkImageUsageFlags &usage,
                           const VkMemoryPropertyFlags &properties,
                           VkImage& image, VkDeviceMemory& imageMemory) const {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = num_samples;  // 关键：设置采样数
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageInfo, nullptr,  &image) != VK_SUCCESS) {
            throw std::runtime_error("无法创建MSAA图像!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = find_memory_type(
            memRequirements.memoryTypeBits,
            properties,
            physical_device
        );

        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("无法分配MSAA图像内存!");
        }

        vkBindImageMemory(device, image, imageMemory, 0);
    }

    void create_color_resources() {
        color_images.resize(swap_chain_image_views.size());
        color_image_memories.resize(swap_chain_image_views.size());
        color_image_views.resize(swap_chain_image_views.size());

        for (size_t i = 0; i < swap_chain_image_views.size(); i++) {
            create_msaa_image(
                swap_chain_extent.width,
                swap_chain_extent.height,
                color_format,
                msaa_samples,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                color_images[i],
                color_image_memories[i]
            );

            color_image_views[i] = create_image_view(
                color_images[i],
                color_format,
                VK_IMAGE_ASPECT_COLOR_BIT,
                device
            );
        }
    }

    vulkan_core() {
        try {
            std::cout << "开始Vulkan初始化..." << std::endl;

            this->init_window();
            std::cout << "窗口初始化完成" << std::endl;

            this->init_instance();
            std::cout << "Vulkan实例创建完成" << std::endl;

            this->init_surface();
            std::cout << "表面创建完成" << std::endl;

            this->physical_device = pick_suitable_device(this->instance, this->surface);
            std::cout << "物理设备选择完成" << std::endl;

            this->init_device_and_queue();
            std::cout << "逻辑设备和队列创建完成" << std::endl;

            this->create_swap_chain();
            std::cout << "交换链创建完成" << std::endl;

            this->create_image_views();
            std::cout << "图像视图创建完成" << std::endl;

            // 在创建深度格式之后，检查MSAA支持
            depth_format = find_depth_format(this->physical_device);

            // 获取最大可用的采样数
            msaa_samples = get_max_usable_sample_count(this->physical_device);
            std::cout << "使用MSAA采样数: " << msaa_samples << std::endl;

            this->create_depth_resources();
            std::cout << "深度资源创建完成" << std::endl;

            // 创建MSAA颜色资源（如果有MSAA）
            if (msaa_samples > VK_SAMPLE_COUNT_1_BIT) {
                color_format = swap_chain_image_format;  // 使用交换链的格式
                create_color_resources();
                std::cout << "MSAA颜色资源创建完成" << std::endl;
            }

            this->create_descriptor_set_layout();
            std::cout << "描述符集布局创建完成" << std::endl;

            this->create_descriptor_pool();
            std::cout << "描述符池创建完成" << std::endl;

            this->create_renderpass();       // ✅ 先创建渲染通道
            std::cout << "渲染通道创建完成" << std::endl;

            this->create_graphics_pipeline(); // 创建图形管线
            std::cout << "图形管线创建完成" << std::endl;

            this->create_framebuffers();      // ✅ 现在可以创建帧缓冲区了
            std::cout << "帧缓冲区创建完成" << std::endl;

            this->create_command_pool();
            std::cout << "命令池创建完成" << std::endl;

            this->create_command_buffers();
            std::cout << "命令缓冲区创建完成" << std::endl;

            // ✅ 新增：创建同步对象
            this->create_sync_objects();
            std::cout << "同步对象创建完成" << std::endl;

            std::cout << "Vulkan初始化成功!" << std::endl;
        } catch (...) {
            std::cerr << "Vulkan初始化失败" << std::endl;
            cleanup();
            throw;
        }
    }

    ~vulkan_core() {
        this->cleanup();
    }

    static void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
        const auto app = static_cast<vulkan_core*>(glfwGetWindowUserPointer(window));
        app->framebuffer_resized = true;
    }

    void wait_for_fences() const noexcept {
        vkWaitForFences(this->device, 1, &this->in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);
    }

   void get_image_index(uint32_t& image_index, VkResult& result) const {
        result = vkAcquireNextImageKHR(
            device,
            swap_chain,
            UINT64_MAX,
            image_available_semaphores[current_frame],  // 等待的信号量
            VK_NULL_HANDLE,
            &image_index
        );
    }

    void wait_usable_image(const uint32_t image_index) {
        if (images_in_flight[image_index] != VK_NULL_HANDLE) {
            vkWaitForFences(device, 1, &images_in_flight[image_index], VK_TRUE, UINT64_MAX);
        }
        images_in_flight[image_index] = in_flight_fences[current_frame];
    }

    void reset_fences() const {
        vkResetFences(device, 1, &in_flight_fences[current_frame]);
    }

    void submit_cmd_buffer() const {
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        const VkSemaphore wait_semaphores[] = {image_available_semaphores[current_frame]};
        constexpr VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffers[current_frame];

        const VkSemaphore signal_semaphores[] = {render_finished_semaphores[current_frame]};
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        if (vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fences[current_frame]) != VK_SUCCESS) {
            throw std::runtime_error("无法提交绘制命令缓冲区!");
        }
    }

    VkResult present_image(const VkSemaphore* signal_semaphores, const uint32_t image_index) const {
        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;

        const VkSwapchainKHR swap_chains[] = {swap_chain};
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swap_chains;
        present_info.pImageIndices = &image_index;
        present_info.pResults = nullptr;

        const VkResult result = vkQueuePresentKHR(present_queue, &present_info);
        return result;
    }

    void go_to_next_frame() {
        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }


    void create_buffer(
        const VkDeviceSize size,
        const VkBufferUsageFlags usage,
        const VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& buffer_memory
    ) const {
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("无法创建缓冲区!");
        }

        VkMemoryRequirements mem_requirements;
        vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_requirements.size;
        alloc_info.memoryTypeIndex = find_memory_type(
            mem_requirements.memoryTypeBits,
            properties,
            physical_device
        );

        if (vkAllocateMemory(device, &alloc_info, nullptr, &buffer_memory) != VK_SUCCESS) {
            throw std::runtime_error("无法分配缓冲区内存!");
        }

        vkBindBufferMemory(device, buffer, buffer_memory, 0);
    }

    void copy_buffer(
        const VkBuffer& source,
        const VkBuffer& destination,
        const VkDeviceSize size) const {
        VkCommandBufferAllocateInfo allocate_info;
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandPool = this->command_pool;
        allocate_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer;
        vkAllocateCommandBuffers(this->device, &allocate_info, &command_buffer);

        VkCommandBufferBeginInfo begin_info;
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(command_buffer, &begin_info);

        VkBufferCopy buffer_copy;
        buffer_copy.dstOffset = 0;
        buffer_copy.srcOffset = 0;
        buffer_copy.size = size;
        vkCmdCopyBuffer(command_buffer, source, destination, 1, &buffer_copy);

        vkEndCommandBuffer(command_buffer);

        VkSubmitInfo submit_info;
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;
        vkQueueSubmit(this->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

        vkQueueWaitIdle(this->graphics_queue);
        vkFreeCommandBuffers(this->device, this->command_pool, 1, &command_buffer);
    }
    /* void draw_frame() {
        // 1. 等待前一帧完成（使用栅栏）
        this->wait_for_fences();

        // 2. 从交换链获取下一张图像
        uint32_t image_index;
        VkResult result;
        get_image_index(image_index, result);

        // 3. 检查是否需要重建交换链
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreate_swap_chain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("无法获取交换链图像!");
        }

        // 4. 检查当前图像是否正在使用
        this->wait_usable_image(image_index);

        // 5. 重置栅栏（准备新的一帧）
        vkResetFences(device, 1, &in_flight_fences[current_frame]);

        // 6. 记录命令缓冲区（这里需要实际实现）
        record_command_buffer(command_buffers[current_frame], image_index);

        // 7. 提交命令缓冲区
        submit_cmd_buffer();

        const VkSemaphore signal_semaphores[] = {render_finished_semaphores[current_frame]};

        // 8. 呈现图像
        result = present_image(signal_semaphores, image_index);

        // 9. 检查交换链是否需要重建
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized) {
            framebuffer_resized = false;
            recreate_swap_chain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("无法呈现交换链图像!");
        }

        // 10. 前进到下一帧
        go_to_next_frame();
    } */
    // 禁用拷贝
    vulkan_core(const vulkan_core&) = delete;
    vulkan_core& operator=(const vulkan_core&) = delete;
};



#endif //VULKAN_PROJECT_VULKAN_CORE_H