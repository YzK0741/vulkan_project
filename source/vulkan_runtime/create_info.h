//
// Created by 小叶 on 2026/5/24.
//

#ifndef VULKAN_PROJECT_CREATE_INFO_H
#define VULKAN_PROJECT_CREATE_INFO_H

#include <vulkan/vulkan.h>
#include <string_view>


namespace vulkan_runtime {
    struct create_info {
        int width = 1080, height = 960;
        std::string_view window_name;
        VkInstance instance = VK_NULL_HANDLE;
        VkPresentModeKHR preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    };
}

#endif //VULKAN_PROJECT_CREATE_INFO_H
