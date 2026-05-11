//
// Created by 小叶 on 2026/1/18.
//
#include <optional>
#include <set>
#include <limits>
#include <algorithm>
#include "vulkan_core.h"

using namespace vulkan_core;

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

logical_device create_logical_device(VkPhysicalDevice physical_device,
                                 const device_creation_info& create_info) {
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

    // 可选：获取其他队列
    // if (create_info.queue_families.compute_family) {
    //     vkGetDeviceQueue(device, create_info.queue_families.compute_family.value(), 0,
    //                      &logical_device.compute_queue);
    // }

    return logical_device;
}

bool check_device_extension_support(const VkPhysicalDevice& physical_device,
                                const std::vector<const char*>& required_extensions) {
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

    /**
     * @note a hack while vulkan returned '0x3BA04C28', replace it with VK_PRESENT_MODE_MAILBOX_KHR
     */
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
std::vector<char> read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        std::println("failed to open file: {}", filename);
        print_stacktrace_and_terminate();
    }

    const auto fileSize = file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

// 创建着色器模块
VkShaderModule create_shader_module(const std::vector<char>& code, const VkDevice& device) {
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