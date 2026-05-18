//
// Created by 小叶 on 2026/4/12.
//

#ifndef VULKAN_PROJECT_CREATE_INFO_H
#define VULKAN_PROJECT_CREATE_INFO_H

#include <vulkan/vulkan.h>

namespace vulkan_core{

    struct create_info {
        int w = 1080, h = 960;
        VkPresentModeKHR preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    };
}



#endif //VULKAN_PROJECT_CREATE_INFO_H