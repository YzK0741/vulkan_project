//
// Created by 小叶 on 2026/4/12.
//

#ifndef VULKAN_PROJECT_CREATE_INFO_H
#define VULKAN_PROJECT_CREATE_INFO_H

#include <vulkan/vulkan.h>

namespace vulkan_core{

    struct create_info {
        int width = 1080, height = 960;
        std::string_view window_name;
        VkInstance instance = VK_NULL_HANDLE;
        VkPresentModeKHR preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    };
}



#endif //VULKAN_PROJECT_CREATE_INFO_H