#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "source/vulkan_runtime/vulkan_runtime.h"
#include "source/loader/obj_parser.h"
#include "source/loader/gltf_parser.h"

int main(const int argc, const char** argv) {

    //print_stacktrace_and_terminate();
    using namespace std::chrono_literals;

    const std::string_view module_path = argc > 1 ? argv[1] : "DamagedHelmet.glb";
    const float module_size = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 2.0f; // NOLINT(*-err34-c)



    auto gltf_data_future = load_gltf_async(module_path);

    // 帧率计数器
    auto last_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    // 创建Vulkan运行时
    vulkan_runtime::runtime runtime;

    vulkan_runtime::vulkan_renderable_object* object = nullptr;

    for (auto gltf_data = gltf_data_future.get();
        const auto&[model, texture_data, texture_width, texture_height, texture_format] : gltf_data) {
        object = &runtime.add_object(model.vertices, model.indices,  texture_data, texture_width, texture_height, texture_format);
    }

    if (!object) {
        print_stacktrace_and_terminate();
    }

    object->change_mvp_method([module_size](vulkan_runtime::uniform_buffer_object& ubo, const vulkan_core::core& core) {
            // 1. 模型矩阵：根据您的模型大小调整
            static float angle = 0.0f;
            angle += glm::radians(0.1f);  // 每秒旋转

            ubo.model = glm::mat4(1.0f);
            ubo.model = glm::scale(ubo.model, glm::vec3(module_size,  module_size, module_size));  // 缩放

            // 修正旋转：将模型的Z轴向上转为Y轴向上
            ubo.model = glm::rotate(ubo.model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

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
        });

    std::println("模型加载成功!");

    std::println("\n开始渲染循环 (按ESC退出)...");

    // 主循环
    while (!glfwWindowShouldClose(runtime.get_window())) {
        glfwPollEvents();

        if (glfwGetKey(runtime.get_window(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(runtime.get_window(), GLFW_TRUE);
        }

        runtime.render_frame();
        frame_count++;

        // 计算帧率
        auto current_time = std::chrono::high_resolution_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - last_time).count();

        if (elapsed >= 1000) {
            const float fps = static_cast<float>(frame_count) * 1000.0f / static_cast<float>(elapsed);
            std::print("\rFPS: {:.2f}", fps);
            std::fflush(stdout);
            frame_count = 0;
            last_time = current_time;
        }

    }
    std::println("\n\n程序正常退出");
    return EXIT_SUCCESS;
}