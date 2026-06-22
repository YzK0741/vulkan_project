#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "source/vulkan_runtime/vulkan_runtime.h"
#include "source/loader/obj_parser.h"
#include "source/loader/gltf_parser.h"
int main(const int argc, const char** argv) {
    using namespace std::chrono_literals;

    const std::string_view module_path = argc > 1 ? argv[1] : "DamagedHelmet.glb";
    //const float module_size = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 2.0f; // NOLINT(*-err34-c)


    auto pbr_future = load_pbr_gltf_async(module_path);

    // 帧率计数器
    auto last_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    // 创建Vulkan运行时
    vulkan_runtime::runtime runtime;

    const auto models =  pbr_future.get();
    auto pbr_model = models[0];

    std::println("model count {}", models.size());



    runtime.add_object_pbr(pbr_model.model.vertices, pbr_model.model.indices, pbr_model.textures);



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