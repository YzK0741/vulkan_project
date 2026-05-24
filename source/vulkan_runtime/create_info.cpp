//
// Created by 小叶 on 2026/5/24.
//

#include "create_info.h"

#include "vulkan_runtime.h"

namespace vulkan_runtime {
    runtime::runtime(const create_info &create_info):
    core({create_info.width, create_info.height, create_info.window_name, create_info.instance, create_info.preferred_present_mode}),
    creator(core.device, core.physical_device, core.command_pool, core.graphics_queue),
    vertex_buffer(core.device),
    index_buffer(core.device){

    }
}
