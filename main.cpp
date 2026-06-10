#define GLFW_INCLUDE_VULKAN
#include <ranges>
#include <GLFW/glfw3.h>
#include "source/vulkan_runtime/vulkan_runtime.h"
#include "source/loader/obj_parser.h"
#include "source/loader/gltf_parser.h"


int main(const int argc, const char** argv) {


    //print_stacktrace_and_terminate();
    using namespace std::chrono_literals;

    const std::string_view module_path = argc > 1 ? argv[1] : "DamagedHelmet.glb";
    const float module_size = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 2.0f; // NOLINT(*-err34-c)

    print_model_info(module_path);

    auto gltf_data_future = load_gltf_async(module_path);
    auto pbr_data = load_gltf_pbr_async(module_path);

    // 创建Vulkan运行时
    vulkan_runtime::runtime runtime;

    vulkan_runtime::vulkan_renderable_object* object = nullptr;

    auto pbr_model = pbr_data.get();

    for (const auto &key: pbr_model.textures | std::views::keys) {
        std::println("{}", key);
    }

    std::fflush(stdout);

    runtime.add_pbr_object(pbr_model.model.vertices, pbr_model.model.indices, pbr_model.textures);

    //for (auto gltf_data = gltf_data_future.get();
    //    const auto&[model, texture_data, texture_width, texture_height, texture_format] : gltf_data) {
    //    object = &runtime.add_object(model.vertices, model.indices,  texture_data, texture_width, texture_height, texture_format);
    //}

    //if (!object) {
    //    print_stacktrace_and_terminate();
    //}



    std::println("模型加载成功!");

    std::println("\n开始渲染循环 (按ESC退出)...");

    if (const auto result = runtime.start_rendering(); !result) {
        std::println("error occurred {}", result.error());
        print_stacktrace_and_terminate();
    }

    // 主循环
    while (!glfwWindowShouldClose(runtime.get_window())) {
        glfwPollEvents();

        if (glfwGetKey(runtime.get_window(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(runtime.get_window(), GLFW_TRUE);
        }

        std::this_thread::sleep_for(0.5s);

        std::print("\rFPS: {}", runtime.get_frame_speed());
    }

    std::println();

    if (auto result = runtime.stop_rendering(); !result) {
        std::println("error occurred while stopping rendering {}", result.error());
        print_stacktrace_and_terminate();
    }

    std::println("\n程序正常退出");
    return EXIT_SUCCESS;
}