#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include "source/vulkan_runtime/vulkan_runtime.h"
#include "source/loader/obj_parser.h"
#include "source/loader/gltf_parser.h"


int main() {

    using namespace std::chrono_literals;

    auto gltf_data_future = load_gltf_async("DamagedHelmet.glb");

    // 帧率计数器
    auto last_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    // 创建Vulkan运行时
    vulkan_runtime runtime;

    //runtime.add_object(positions2, normals2, tex_coords2, module2.indices, "room.png");
    //auto& adm = runtime.add_object(module1.vertices, module1.indices);

    auto gltf_data = gltf_data_future.get();

    for (const auto&[model, texture_data, texture_width, texture_height, texture_format] : gltf_data) {
        runtime.add_object(model.vertices, model.indices,  texture_data, texture_width, texture_height, texture_format);
    }

    std::println("模型加载成功!");

    std::println("\n开始渲染循环 (按ESC退出)...");

    // 主循环
    while (!glfwWindowShouldClose(runtime.get_window())) {
        glfwPollEvents();

        if (glfwGetKey(runtime.get_window(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(runtime.get_window(), GLFW_TRUE);
        }

        // 测试：按R键重新加载（如果使用OBJ）
        static bool r_pressed = false;
        if (glfwGetKey(runtime.get_window(), GLFW_KEY_R) == GLFW_PRESS && !r_pressed) {
            r_pressed = true;
            std::println("重新加载模型...");
            // 这里可以添加重新加载逻辑
        } else if (glfwGetKey(runtime.get_window(), GLFW_KEY_R) == GLFW_RELEASE) {
            r_pressed = false;
        }
        runtime.render_frame_with_objects();
        frame_count++;

        // 计算帧率
        auto current_time = std::chrono::high_resolution_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - last_time).count();

        if (elapsed >= 1000) {
            const float fps = static_cast<float>(frame_count) * 1000.0f / static_cast<float>(elapsed);
            std::print("\rFPS: {}", fps);
            std::fflush(stdout);
            frame_count = 0;
            last_time = current_time;
        }

    }
    std::println("\n\n程序正常退出");
    return EXIT_SUCCESS;
}