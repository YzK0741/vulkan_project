//
// Created by 小叶 on 2026/1/18.
//
#include <optional>
#include <set>
#include <limits>
#include <algorithm>
#include "vulkan_core.h"

#include <boost/core/demangle.hpp>


namespace vulkan_core {

    VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data
    ) {

        // 根据严重程度添加前缀
        std::string_view prefix;
        if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            prefix = "[ERROR]";
        } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            prefix = "[WARNING]";
        } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
            prefix = "[INFO]";
        } else {
            prefix = "[VERBOSE]";
        }

        std::println("{} {}", prefix, callback_data->pMessage);

        return VK_FALSE;  // 返回 VK_FALSE 表示不中止程序
    }

    bool check_validation_layer_support(const std::vector<const char*>& validation_layers) {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

        for (const char* layer_name : validation_layers) {
            bool layer_found = false;
            for (const auto& layer_properties : available_layers) {
                if (strcmp(layer_name, layer_properties.layerName) == 0) {
                    layer_found = true;
                    break;
                }
            }
            if (!layer_found) {
                return false;
            }
        }
        return true;
    }

    // 获取GLFW需要的扩展
    std::vector<const char*> get_required_extensions() {
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

#ifdef _DEBUG
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

        return extensions;
    }

    queue_family_indices find_queue_families(const VkPhysicalDevice& device, const VkSurfaceKHR& surface) {
        queue_family_indices indices;

        // 获取队列族属性
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        // 查找合适的队列族
        for (uint32_t i = 0; i < queue_family_count; ++i) {
            const auto& queue_family = queue_families[i];

            // 检查图形支持
            if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) && !indices.graphics_family.has_value()) {
                indices.graphics_family = i;
            }

            // 检查计算支持（非图形队列）
            if ((queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                !indices.compute_family.has_value() &&
                !(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.compute_family = i;
                }

            // 检查传输支持（非图形/计算队列）
            if ((queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                !indices.transfer_family.has_value() &&
                !(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                !(queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                indices.transfer_family = i;
                }

            // 检查呈现支持（需要表面）
            if (surface != VK_NULL_HANDLE) {
                VkBool32 present_support = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
                if (present_support && !indices.present_family.has_value()) {
                    indices.present_family = i;
                }
            }

            // 如果只需要图形支持，提前退出
            if (surface == VK_NULL_HANDLE && indices.graphics_family.has_value()) {
                break;
            }

            // 如果所有需要的队列都已找到，提前退出
            if (surface == VK_NULL_HANDLE) {
                if (indices.graphics_family.has_value()) {
                    break;
                }
            } else if (indices.is_complete()) {
                break;
            }
        }

        // 回退方案：如果没有找到专用的计算/传输队列，使用图形队列
        if (!indices.compute_family.has_value() && indices.graphics_family.has_value()) {
            indices.compute_family = indices.graphics_family;
        }

        if (!indices.transfer_family.has_value()) {
            // 优先使用图形队列，如果没有图形队列则使用第一个可用的队列
            indices.transfer_family = indices.graphics_family.has_value()
                ? indices.graphics_family
                : (queue_family_count > 0 ? std::optional<uint32_t>(0) : std::nullopt);
        }

        return indices;
    }

    VkPhysicalDevice pick_suitable_device(const VkInstance& instance, VkSurfaceKHR surface) {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            std::println("Failed to find GPUs with Vulkan support!");
            print_stacktrace_and_terminate();
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            VkPhysicalDeviceProperties deviceProperties;
            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

            // 检查队列族是否完整（需要表面！）
            if (queue_family_indices indices = find_queue_families(device, surface); !indices.is_complete()) {
                continue;
            }

            // 检查扩展支持
            const std::vector<const char*> requiredExtensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };
            if (!check_device_extension_support(device, requiredExtensions)) {
                continue;
            }

            return device; // 找到合适的设备
        }

        std::println("Failed to find a suitable GPU!");
        print_stacktrace_and_terminate();
    }

    logical_device create_logical_device(
        VkPhysicalDevice physical_device,
        const device_creation_info& create_info
        ) {
        if (!create_info.queue_families.is_complete()) {
            std::println("Queue families not complete");
            print_stacktrace_and_terminate();
        }

        // 使用set收集唯一的队列族索引
        std::set<uint32_t> unique_queue_families = {
            create_info.queue_families.graphics_family.value(),
            create_info.queue_families.present_family.value()
        };

        // 可选：添加其他队列族
        if (create_info.queue_families.compute_family) {
            unique_queue_families.insert(create_info.queue_families.compute_family.value());
        }
        if (create_info.queue_families.transfer_family) {
            unique_queue_families.insert(create_info.queue_families.transfer_family.value());
        }

        // 创建队列信息
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        constexpr float queue_priority = 1.0f;

        for (const uint32_t& queue_family : unique_queue_families) {
            VkDeviceQueueCreateInfo queue_info{};
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info.queueFamilyIndex = queue_family;
            queue_info.queueCount = 1;
            queue_info.pQueuePriorities = &queue_priority;
            queue_create_infos.push_back(queue_info);
        }

        // 创建设备
        VkDeviceCreateInfo device_create_info{};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        device_create_info.pQueueCreateInfos = queue_create_infos.data();
        device_create_info.pEnabledFeatures = &create_info.device_features;

        // 扩展 - 必须正确处理空向量
        device_create_info.enabledExtensionCount = static_cast<uint32_t>(create_info.extensions.size());
        device_create_info.ppEnabledExtensionNames =
            create_info.extensions.empty() ? nullptr : create_info.extensions.data();

        // 验证层 - 现代Vulkan通常不在设备级启用
        device_create_info.enabledLayerCount = static_cast<uint32_t>(create_info.validation_layers.size());
        device_create_info.ppEnabledLayerNames =
            create_info.validation_layers.empty() ? nullptr : create_info.validation_layers.data();

        // 复制pNext链 - 注意：这里只是浅拷贝
        device_create_info.pNext = create_info.pNext;

        VkDevice device;
        VkResult result = vkCreateDevice(physical_device, &device_create_info, nullptr, &device);
        if (result != VK_SUCCESS) {
            std::println("Failed to create logical device: {}",
                                    std::to_string(result));
            print_stacktrace_and_terminate();
        }

        // 获取队列
        logical_device logical_device;
        logical_device.device = device;
        logical_device.graphics_family_index = create_info.queue_families.graphics_family.value();
        logical_device.present_family_index = create_info.queue_families.present_family.value();

        vkGetDeviceQueue(device, logical_device.graphics_family_index, 0,
                         &logical_device.graphics_queue);
        vkGetDeviceQueue(device, logical_device.present_family_index, 0,
                         &logical_device.present_queue);

        return logical_device;
    }

    bool check_device_extension_support(
        const VkPhysicalDevice& physical_device,
        const std::vector<const char*>& required_extensions
        ) {
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extension_count);
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count,
                                             availableExtensions.data());

        std::set<std::string> requiredSet(required_extensions.begin(), required_extensions.end());
        for (const auto&[extensionName, specVersion] : availableExtensions) {
            requiredSet.erase(extensionName);
        }

        return requiredSet.empty();
    }

    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            // 确保宽度和高度不为0
            width = std::max(width, 1);
            height = std::max(height, 1);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width,
                                           capabilities.minImageExtent.width,
                                           capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height,
                                            capabilities.minImageExtent.height,
                                            capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    /**
     * @remark hack: when find 0x3BA04C28 in present modes replace it with VK_PRESENT_MODE_MAILBOX_KHR
     */
    swap_chain_support_details query_swap_chain_support(const VkPhysicalDevice& device, const VkSurfaceKHR& surface) {
        swap_chain_support_details details = {};

        // 1. 查询表面能力
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        // 2. 查询表面格式
        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
        if (format_count != 0) {
            details.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
        }

        // 3. 查询呈现模式
        uint32_t present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
        if (present_mode_count != 0) {
            details.present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes.data());
        }

        std::print("present modes count: {} \n", details.present_modes.size());

        std::ranges::for_each(details.present_modes, [](auto& x) {
            if (x == 0x3BA04C28) {
                x = VK_PRESENT_MODE_MAILBOX_KHR;
                std::println("wrong present mode '0x3BA04C28' occurred");
            }
        });
        return details;
    }

    VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes) {

        if (std::ranges::find(available_present_modes, VK_PRESENT_MODE_MAILBOX_KHR) != available_present_modes.end())
            return VK_PRESENT_MODE_MAILBOX_KHR;

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) {
        for (const auto& available_format : available_formats) {
            if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return available_format;
            }
        }

        return available_formats[0];
    }

    // 读取文件函数（用于读取着色器文件）
    std::vector<unsigned char> read_file(const std::string &filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            std::println("failed to open file: {}", filename);
            print_stacktrace_and_terminate();
        }

        const auto fileSize = file.tellg();
        std::vector<unsigned char> buffer(fileSize);

        file.seekg(0);
        file.read(reinterpret_cast<std::istream::char_type *>(buffer.data()), fileSize);
        file.close();

        return buffer;
    }

    VkShaderModule create_shader_module(const std::span<const unsigned char> code, const VkDevice& device)  {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            std::println("failed to create shader module!");
            print_stacktrace_and_terminate();
        }
        return shaderModule;
    }

    VkFormat find_depth_format(const VkPhysicalDevice &physical_device) {
        // 尝试获取支持的深度格式，按偏好顺序
        const std::vector<VkFormat> candidates = {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM
        };

        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

            // 检查格式是否支持作为深度附件
            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                return format;
            }
        }

        std::println("failed to find supported depth format!");
        print_stacktrace_and_terminate();
    }

    VkImageView create_image_view(const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectFlags, const VkDevice& device) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;

        // 组件映射（保持默认）
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // 子资源范围（描述图像的哪部分可访问）
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            std::println("failed to create image view!");
            print_stacktrace_and_terminate();
        }

        return imageView;
    }

    VkSampleCountFlagBits get_max_usable_sample_count(const VkPhysicalDevice& physical_device) {
        VkPhysicalDeviceProperties physical_device_properties;
        vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);

        const VkSampleCountFlags counts = physical_device_properties.limits.framebufferColorSampleCounts &
                                          physical_device_properties.limits.framebufferDepthSampleCounts;

        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT)  { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT)  { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT)  { return VK_SAMPLE_COUNT_2_BIT; }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    core::core() {
            std::println("开始Vulkan初始化...");

            this->init_window();
            std::println("窗口初始化完成");

            this->init_instance();
            std::println("Vulkan实例创建完成");

            this->init_surface();
            std::println("表面创建完成");

            this->physical_device = pick_suitable_device(this->instance, this->surface);
            std::println("物理设备选择完成");

            this->init_device_and_queue();
            std::println("逻辑设备和队列创建完成");

            this->init_vma();
            std::println("vma 创建完成");

            this->create_swap_chain();
            std::println("交换链创建完成");

            this->create_image_views();
            std::println("图像视图创建完成");

            // 在创建深度格式之后，检查MSAA支持
            depth_format = find_depth_format(this->physical_device);

            // 获取最大可用的采样数
            msaa_samples = get_max_usable_sample_count(this->physical_device);
            std::println("使用MSAA采样数: {}", static_cast<long>(this->msaa_samples));

            this->create_depth_resources();
            std::println("深度资源创建完成");

            // 创建MSAA颜色资源（如果有MSAA）
            if (msaa_samples > VK_SAMPLE_COUNT_1_BIT) {
                color_format = swap_chain_image_format;  // 使用交换链的格式
                create_color_resources();
                std::println("MSAA颜色资源创建完成");
            }

            this->create_descriptor_set_layout();
            std::println("描述符集布局创建完成");

            this->create_descriptor_pool();
            std::println("描述符池创建完成");

            this->create_renderpass();
            std::println("渲染通道创建完成");

            this->create_framebuffers();
            std::println("帧缓冲区创建完成");

            this->create_graphics_pipeline_layout();

            this->create_command_pool();
            std::println("命令池创建完成");

            this->create_command_buffers();
            std::println("命令缓冲区创建完成");

            this->create_sync_objects();
            std::println("同步对象创建完成");

            std::println("Vulkan初始化成功!");
        }

    core::core(const create_info &info) {

        std::println("开始Vulkan初始化...");

        this->init_window(info.width, info.height, info.window_name);
        std::println("窗口初始化完成");

        if (info.instance != VK_NULL_HANDLE) {

            this->instance = info.instance;
            std::println("Vulkan实例创建完成");

        } else {

            this->init_instance();
            std::println("Vulkan实例创建完成");

        }

        this->init_surface();
        std::println("表面创建完成");

        this->physical_device = pick_suitable_device(this->instance, this->surface);
        std::println("物理设备选择完成");

        this->init_device_and_queue();
        std::println("逻辑设备和队列创建完成");

        this->init_vma();
        std::println("vma 创建完成");

        this->create_swap_chain();
        std::println("交换链创建完成");

        this->create_image_views();
        std::println("图像视图创建完成");

        // 在创建深度格式之后，检查MSAA支持
        depth_format = find_depth_format(this->physical_device);

        // 获取最大可用的采样数
        msaa_samples = get_max_usable_sample_count(this->physical_device);
        std::println("使用MSAA采样数: {}", static_cast<long>(this->msaa_samples));

        this->create_depth_resources();
        std::println("深度资源创建完成");

        // 创建MSAA颜色资源（如果有MSAA）
        if (msaa_samples > VK_SAMPLE_COUNT_1_BIT) {
            color_format = swap_chain_image_format;  // 使用交换链的格式
            create_color_resources();
            std::println("MSAA颜色资源创建完成");
        }

        this->create_descriptor_set_layout();
        std::println("描述符集布局创建完成");

        this->create_descriptor_pool();
        std::println("描述符池创建完成");

        this->create_renderpass();
        std::println("渲染通道创建完成");

        this->create_framebuffers();
        std::println("帧缓冲区创建完成");

        this->create_graphics_pipeline_layout();

        this->create_command_pool();
        std::println("命令池创建完成");

        this->create_command_buffers();
        std::println("命令缓冲区创建完成");

        this->create_sync_objects();
        std::println("同步对象创建完成");

        std::println("Vulkan初始化成功!");
    }

    core::~core() {
        this->cleanup();
    }

    void core::init_instance()  {
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
            std::println("添加调试扩展: {}", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

            // 输出所有扩展
            std::cout << "请求的实例扩展 (" << extensions.size() << "):" << std::endl;
            for (const auto& ext : extensions) {
                std::println("  - {}", ext);
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
                std::println("验证层已启用 ( {} )", validation_layers.size());
                for (const auto& layer : validation_layers) {
                    std::println("  - {}", layer);
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
            std::println("正在创建Vulkan实例...");

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
                std::println("{}", error_msg);
                print_stacktrace_and_terminate();
            }

            register_cleanup([this] {

                if (instance != VK_NULL_HANDLE) {
                    vkDestroyInstance(this->instance, nullptr);
                }
            });

            std::println("Vulkan实例创建成功");

#ifdef _DEBUG
            // 获取函数指针
            auto vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
                instance, "vkCreateDebugUtilsMessengerEXT"));
            auto vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
                instance, "vkDestroyDebugUtilsMessengerEXT"));

            // 检查函数指针是否获取成功
            if (!vkCreateDebugUtilsMessengerEXT || !vkDestroyDebugUtilsMessengerEXT) {
                std::println(stderr, "Failed to get debug utils function pointers");
                print_stacktrace_and_terminate();
            }
            VkDebugUtilsMessengerCreateInfoEXT debug_info{};
            debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debug_info.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug_info.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug_info.pfnUserCallback = debug_callback;  // 需要实现这个回调函数
            debug_info.pUserData = nullptr;

            if (vkCreateDebugUtilsMessengerEXT(instance, &debug_info, nullptr, &debug_messenger) != VK_SUCCESS) {
                std::println(stderr, "创建调试信使失败");
            } else {
                std::println(stderr, "调试信使创建成功");
                // 记得在 cleanup 中销毁
                register_cleanup([this, vkDestroyDebugUtilsMessengerEXT] {
                    if (debug_messenger != VK_NULL_HANDLE) {
                        vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
                    }
                });
            }

#endif
        }

    void core::init_device_and_queue() {
        // 创建设备
        device_creation_info info;

        // 需要在创建表面后获取队列族信息
        info.queue_families = find_queue_families(physical_device, surface);  // 添加surface参数

        // 启用必要扩展
        info.extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        // 检查扩展支持
        if (!check_device_extension_support(physical_device, info.extensions)) {
            std::println("Required device extensions not supported");
            print_stacktrace_and_terminate();
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

        register_cleanup([this] {
            if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(this->device, nullptr);
            }
        });
    }

    void core::init_window(const int width, const int height, const std::string_view window_name) noexcept {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(width,
            height,
            window_name.data() ? window_name.data() : "Vulkan",
            nullptr,
            nullptr
            );

        register_cleanup([this] {
            if (window) {
                glfwDestroyWindow(window);
            }
        });
    }

    void core::init_vma() noexcept {
        this->vma.init(this->instance, this->device, this->physical_device, this->graphics_queue, this->graphics_family_index);

        register_cleanup([this] {
           vma.destroy();
        });
    }

    void core::init_surface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            std::println("无法创建窗口表面");
            print_stacktrace_and_terminate();
        }

        register_cleanup([this] {
            if (surface != VK_NULL_HANDLE) {
                vkDestroySurfaceKHR(instance, surface, nullptr);
            }
        });
    }

    void core::create_swap_chain() {
            if (swap_chain_cleanup)
                (*swap_chain_cleanup)();


            const auto [capabilities, formats, present_modes] = query_swap_chain_support(this->physical_device, this->surface);

            // 添加检查：
            if (formats.empty() || present_modes.empty()) {
                std::println("Swap chain not adequately supported");
                print_stacktrace_and_terminate();
            }

            const VkSurfaceFormatKHR surface_format = choose_swap_surface_format(formats);
            const VkPresentModeKHR present_mode = choose_swap_present_mode(present_modes);
            const VkExtent2D extent = choose_swap_extent(capabilities, this->window);

            uint32_t image_count = capabilities.minImageCount + 1;

            if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
                image_count = capabilities.maxImageCount;
            }

            image_count = std::max(image_count, capabilities.minImageCount);

            VkSwapchainCreateInfoKHR create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            create_info.surface = surface;

            create_info.minImageCount = image_count;
            create_info.imageFormat = surface_format.format;
            create_info.imageColorSpace = surface_format.colorSpace;
            create_info.imageExtent = extent;
            create_info.imageArrayLayers = 1;
            create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            const queue_family_indices indices = find_queue_families(this->physical_device, this->surface);
            const uint32_t queueFamilyIndices[] = {indices.graphics_family.value(), indices.present_family.value()};

            if (indices.graphics_family != indices.present_family) {
                create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                create_info.queueFamilyIndexCount = 2;
                create_info.pQueueFamilyIndices = queueFamilyIndices;
            } else {
                create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                create_info.queueFamilyIndexCount = 0; // Optional
                create_info.pQueueFamilyIndices = nullptr; // Optional
            }

            create_info.preTransform = capabilities.currentTransform;
            create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            create_info.presentMode = present_mode;
            create_info.clipped = VK_TRUE;
            create_info.oldSwapchain = VK_NULL_HANDLE;

            if (vkCreateSwapchainKHR(device, &create_info, nullptr, &this->swap_chain) != VK_SUCCESS) {
                std::println("failed to create swap chain!");
                print_stacktrace_and_terminate();
            }


            vkGetSwapchainImagesKHR(device, this->swap_chain, &image_count, nullptr);
            this->swap_chain_images.resize(image_count);
            vkGetSwapchainImagesKHR(device, this->swap_chain, &image_count, this->swap_chain_images.data());

            this->swap_chain_image_format = surface_format.format;
            this->swap_chain_extent = extent;

            if (!swap_chain_cleanup) {
                register_cleanup([this] {
                    if (swap_chain != VK_NULL_HANDLE) {
                        vkDestroySwapchainKHR(device, swap_chain, nullptr);
                    }
                });
                swap_chain_cleanup = &dtor_stack.top();
            }
        }

    void core::create_image_views() {
        if (image_view_cleanup)
            (*image_view_cleanup)();

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
                std::println("failed to create image views!");
                print_stacktrace_and_terminate();
            }
        }

        if (!image_view_cleanup) {
            register_cleanup([this] {
                for (const auto& image_view : swap_chain_image_views) {
                    vkDestroyImageView(device, image_view, nullptr);
                }
            });
            image_view_cleanup = &dtor_stack.top();
        }
    }

    void core::create_depth_image(VkImage &image, VkDeviceMemory &imageMemory, VkImageView &image_view) const  {
            // 使用类内的交换链尺寸
            const VkExtent2D& extent = this->swap_chain_extent;

            // 1. 创建图像
            VkImageCreateInfo image_info{};
            image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            image_info.imageType = VK_IMAGE_TYPE_2D;
            image_info.extent = { extent.width, extent.height, 1 };
            image_info.mipLevels = 1;
            image_info.arrayLayers = 1;
            image_info.format = this->depth_format;
            image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            image_info.samples = this->msaa_samples;
            image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateImage(device, &image_info, nullptr, &image) != VK_SUCCESS) {
                vkDestroyImage(device, image, nullptr);
                std::println("failed to create depth image!");
                print_stacktrace_and_terminate();
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
                std::println("failed to allocate depth image memory!");
                print_stacktrace_and_terminate();
            }

            vkBindImageMemory(device, image, imageMemory, 0);

            // 3. 创建图像视图
            VkImageViewCreateInfo view_info{};
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = image;
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = this->depth_format;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            view_info.subresourceRange.baseMipLevel = 0;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.baseArrayLayer = 0;
            view_info.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &view_info, nullptr, &image_view) != VK_SUCCESS) {
                std::println("failed to create depth image view!");
                print_stacktrace_and_terminate();
            }
        }

    void core::create_depth_resources()  {

        if (depth_resource_cleanup)
            (*depth_resource_cleanup)();

        depth_format = find_depth_format(this->physical_device);

        // 先测试深度格式是否有效
        if (depth_format == VK_FORMAT_UNDEFINED) {
            std::println("无法找到支持的深度格式");
            print_stacktrace_and_terminate();
        }

        depth_images.resize(swap_chain_image_views.size());
        depth_image_views.resize(swap_chain_image_views.size());
        depth_image_memories.resize(swap_chain_image_views.size());  // 确保分配内存

        for (size_t i = 0; i < swap_chain_image_views.size(); i++) {
            // 直接调用 create_depth_image，但确保参数正确
            create_depth_image(depth_images[i], depth_image_memories[i], depth_image_views[i]);
        }


        if (!depth_resource_cleanup) {
            register_cleanup([this] {
                for (const auto& view: depth_image_views) {
                    vkDestroyImageView(device, view, nullptr);
                }
                for (const auto& image : depth_images) {
                    vkDestroyImage(device, image, nullptr);
                }
                for (const auto& memory : depth_image_memories) {
                    vkFreeMemory(device, memory, nullptr);
                }
            });
            depth_resource_cleanup = &dtor_stack.top();
        }
    }

    void core::create_renderpass() {
            if (renderpass_cleanup)
                (*renderpass_cleanup)();
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
                std::println("无法创建渲染通道!");
                print_stacktrace_and_terminate();
            }


            if (!renderpass_cleanup) {
                register_cleanup([this] {
                    vkDestroyRenderPass(device, renderpass, nullptr);
                });
                renderpass_cleanup = &dtor_stack.top();
            }
        }

    void core::create_framebuffers() {

            if (framebuffer_cleanup)
                (*framebuffer_cleanup)();

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
                    std::println("无法创建帧缓冲区!");
                    print_stacktrace_and_terminate();
                }
            }


            if (!framebuffer_cleanup) {
                register_cleanup([this] {
                    for (const auto& frame_buffer : swap_chain_framebuffers) {
                        vkDestroyFramebuffer(device, frame_buffer, nullptr);
                    }
                });
                framebuffer_cleanup = &dtor_stack.top();
            }
        }

    void core::create_command_pool() {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphics_family_index;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &command_pool) != VK_SUCCESS) {
            std::println("failed to create command pool!");
            print_stacktrace_and_terminate();
        }

        register_cleanup([this] {
            if (command_pool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device, command_pool, nullptr);
            }
        });
    }

    void core::create_graphics_pipeline_layout() {
            if (pipeline_cleanup_cleanup)
                (*pipeline_cleanup_cleanup)();

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

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = swap_chain_extent;

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
                std::println("failed to create pipeline layout!");
                print_stacktrace_and_terminate();
            }


            if (!pipeline_cleanup_cleanup) {
                register_cleanup([this] {
                    if (pipeline_layout != VK_NULL_HANDLE) {
                        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
                    }
                });
                pipeline_cleanup_cleanup = &dtor_stack.top();
            }
        }

    void core::create_descriptor_set_layout()  {
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
            std::println("无法创建描述符集布局!");
            print_stacktrace_and_terminate();
        }

        register_cleanup([this] {
            if (descriptor_set_layout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
            }
        });
    }

    void core::create_pbr_descriptor_set_layout() {
            std::array<VkDescriptorSetLayoutBinding, 6> bindings = {};  // 增加到6个

            // binding 0: Uniform Buffer
            bindings[0].binding = 0;
            bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            bindings[0].descriptorCount = 1;
            bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            // binding 1: Base Color
            bindings[1].binding = 1;
            bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[1].descriptorCount = 1;
            bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            // binding 2: Metallic/Roughness
            bindings[2].binding = 2;
            bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[2].descriptorCount = 1;
            bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            // binding 3: Normal
            bindings[3].binding = 3;
            bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[3].descriptorCount = 1;
            bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            // binding 4: Occlusion
            bindings[4].binding = 4;
            bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[4].descriptorCount = 1;
            bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            // binding 5: Emissive
            bindings[5].binding = 5;
            bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[5].descriptorCount = 1;
            bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            // 创建描述符集布局
            VkDescriptorSetLayoutCreateInfo layout_info{};
            layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount = bindings.size();
            layout_info.pBindings = bindings.data();

            if (vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &descriptor_set_layout_pbr) != VK_SUCCESS) {
                std::println(stderr, "PBR描述符集布局创建失败");
                print_stacktrace_and_terminate();
            }

            register_cleanup([this] {
                vkDestroyDescriptorSetLayout(device, descriptor_set_layout_pbr, nullptr);
            });
        }

    void core::create_descriptor_pool() {
        std::vector<VkDescriptorPoolSize> pool_sizes;
        constexpr int max_size = 1024;
        pool_sizes.push_back({
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, max_size
        });

        pool_sizes.push_back({
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_size
        });

        VkDescriptorPoolCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        info.maxSets = max_size;
        info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        info.pPoolSizes = pool_sizes.data();
        if (vkCreateDescriptorPool(this->device, &info, nullptr, &this->descriptor_pool)) {
            std::println("failed in creating descriptor pool");
            print_stacktrace_and_terminate();
        }

        register_cleanup([this] {
            if (descriptor_pool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
            }
        });
    }

    void core::create_command_buffers() {
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
            std::println("无法分配命令缓冲区!");
            print_stacktrace_and_terminate();
        }
    }

    void core::create_sync_objects() {
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
                std::println("failed to create synchronization objects for a frame!");
                print_stacktrace_and_terminate();
                }
        }

        register_cleanup([this] {
            for (const auto& semaphore : image_available_semaphores) {
                vkDestroySemaphore(device, semaphore, nullptr);
            }

            for (const auto& semaphore : render_finished_semaphores) {
                vkDestroySemaphore(device, semaphore, nullptr);
            }

            for (const auto& fence : in_flight_fences) {
                vkDestroyFence(device, fence, nullptr);
            }
        });
    }

    void core::cleanup_swap_chain() {
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
    }

    void core::recreate_swap_chain() {

        this->create_swap_chain();
        this->create_image_views();
        this->create_depth_resources();

        if (msaa_samples > VK_SAMPLE_COUNT_1_BIT) {
            create_color_resources();
        }

        this->create_renderpass();
        this->create_graphics_pipeline_layout();
        this->create_framebuffers();
        this->create_command_buffers();

        images_in_flight.resize(swap_chain_images.size(), VK_NULL_HANDLE);
    }

    void core::create_msaa_image(
        const uint32_t &width,
        const uint32_t &height,
        const VkFormat &format,
        const VkSampleCountFlagBits &num_samples,
        const VkImageTiling &tiling,
        const VkImageUsageFlags &usage,
        const VkMemoryPropertyFlags &properties,
        VkImage &image,
        VkDeviceMemory &imageMemory
        ) const {
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
            std::println("无法创建MSAA图像!");
            print_stacktrace_and_terminate();
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
            std::println(stderr, "无法分配MSAA图像内存!");
            print_stacktrace_and_terminate();
        }

        vkBindImageMemory(device, image, imageMemory, 0);
    }

    void core::create_color_resources() {
            if (color_resources_cleanup)
                (*color_resources_cleanup)();

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

            if (!color_resources_cleanup) {
                register_cleanup([this] {
                    for (const auto& view : color_image_views) {
                        vkDestroyImageView(device, view, nullptr);
                    }
                    for (const auto& image : color_images) {
                        vkDestroyImage(device, image, nullptr);
                    }
                    for (const auto& memory : color_image_memories) {
                        vkFreeMemory(device, memory, nullptr);
                    }
                });
                color_resources_cleanup = &dtor_stack.top();
            }
        }

    void core::framebuffer_resize_callback(GLFWwindow *window, int width, int height) {
        const auto app = static_cast<core*>(glfwGetWindowUserPointer(window));
        app->framebuffer_resized = true;
    }

    void core::wait_for_fences() const noexcept {
        vkWaitForFences(this->device, 1, &this->in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);
    }

    void core::get_image_index(uint32_t &image_index, VkResult &result) const {
        result = vkAcquireNextImageKHR(
            device,
            swap_chain,
            UINT64_MAX,
            image_available_semaphores[current_frame],  // 等待的信号量
            VK_NULL_HANDLE,
            &image_index
        );
    }

    void core::wait_usable_image(const uint32_t image_index) {

        if (images_in_flight[image_index] != VK_NULL_HANDLE) {
            vkWaitForFences(device, 1, &images_in_flight[image_index], VK_TRUE, UINT64_MAX);
        }
        images_in_flight[image_index] = in_flight_fences[current_frame];
    }

    void core::reset_fences() const {
        vkResetFences(device, 1, &in_flight_fences[current_frame]);
    }

    void core::submit_cmd_buffer() const {
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
            std::println("无法提交绘制命令缓冲区!");
            print_stacktrace_and_terminate();
        }
    }

    VkResult core::present_image(const VkSemaphore *signal_semaphores, const uint32_t image_index) const {
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

    void core::go_to_next_frame() {
        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void core::copy_buffer(
        const VkBuffer &source,
        const VkBuffer &destination,
        const VkDeviceSize size
        ) const {

            // 1. 验证成员变量有效性
            if (this->device == VK_NULL_HANDLE) {
                std::println("Vulkan device is not initialized");
                print_stacktrace_and_terminate();
            }

            if (this->command_pool == VK_NULL_HANDLE) {
                std::println("Command pool is not initialized");
                print_stacktrace_and_terminate();
            }

            if (this->graphics_queue == VK_NULL_HANDLE) {
                std::println("Graphics queue is not initialized");
                print_stacktrace_and_terminate();
            }

            // 2. 完全初始化的分配信息结构体
            VkCommandBufferAllocateInfo allocate_info{};
            allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocate_info.pNext = nullptr;
            allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocate_info.commandPool = this->command_pool;
            allocate_info.commandBufferCount = 1;

            VkCommandBuffer command_buffer = VK_NULL_HANDLE;
            VkResult result = vkAllocateCommandBuffers(this->device, &allocate_info, &command_buffer);

            if (result != VK_SUCCESS) {
                std::println("Failed to allocate command buffer for buffer copy");
                print_stacktrace_and_terminate();
            }

            // 3. 完全初始化的开始信息结构体
            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.pNext = nullptr;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            begin_info.pInheritanceInfo = nullptr;

            result = vkBeginCommandBuffer(command_buffer, &begin_info);
            if (result != VK_SUCCESS) {
                vkFreeCommandBuffers(this->device, this->command_pool, 1, &command_buffer);
                std::println("Failed to begin command buffer");
                print_stacktrace_and_terminate();
            }

            // 4. 完全初始化的缓冲区拷贝结构体
            VkBufferCopy buffer_copy{};
            buffer_copy.srcOffset = 0;
            buffer_copy.dstOffset = 0;
            buffer_copy.size = size;

            vkCmdCopyBuffer(command_buffer, source, destination, 1, &buffer_copy);

            result = vkEndCommandBuffer(command_buffer);
            if (result != VK_SUCCESS) {
                vkFreeCommandBuffers(this->device, this->command_pool, 1, &command_buffer);
                std::println("Failed to end command buffer");
                print_stacktrace_and_terminate();
            }

            // 5. 完全初始化的提交信息结构体
            VkSubmitInfo submit_info{};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.pNext = nullptr;
            submit_info.waitSemaphoreCount = 0;
            submit_info.pWaitSemaphores = nullptr;
            submit_info.pWaitDstStageMask = nullptr;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &command_buffer;
            submit_info.signalSemaphoreCount = 0;
            submit_info.pSignalSemaphores = nullptr;

            result = vkQueueSubmit(this->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
            if (result != VK_SUCCESS) {
                vkFreeCommandBuffers(this->device, this->command_pool, 1, &command_buffer);
                std::println("Failed to submit copy command to queue");
                print_stacktrace_and_terminate();
            }

            // 6. 等待队列完成
            result = vkQueueWaitIdle(this->graphics_queue);
            if (result != VK_SUCCESS) {
                vkFreeCommandBuffers(this->device, this->command_pool, 1, &command_buffer);
                std::println("Failed to wait for queue idle");
                print_stacktrace_and_terminate();
            }

            // 7. 清理命令缓冲区
            vkFreeCommandBuffers(this->device, this->command_pool, 1, &command_buffer);
        }

}
