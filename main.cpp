#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include "vulkan_runtime.h"
#include "obj_parser.h"

// 创建测试立方体数据（备用）
void create_test_cube(std::vector<glm::vec3>& positions,
                      std::vector<glm::vec2>& texcoords,
                      std::vector<uint32_t>& indices) {
    // 立方体的8个顶点
    positions = {
        // 前面
        {-0.5f, -0.5f, 0.5f},  // 0
        {0.5f, -0.5f, 0.5f},   // 1
        {0.5f, 0.5f, 0.5f},    // 2
        {-0.5f, 0.5f, 0.5f},   // 3
        // 后面
        {-0.5f, -0.5f, -0.5f}, // 4
        {0.5f, -0.5f, -0.5f},  // 5
        {0.5f, 0.5f, -0.5f},   // 6
        {-0.5f, 0.5f, -0.5f}   // 7
    };

    texcoords = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f},  // 前面4个顶点
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}   // 后面4个顶点
    };

    // 12个三角形（36个索引）
    indices = {
        // 前面
        0, 1, 2, 2, 3, 0,
        // 右面
        1, 5, 6, 6, 2, 1,
        // 后面
        5, 4, 7, 7, 6, 5,
        // 左面
        4, 0, 3, 3, 7, 4,
        // 顶面
        3, 2, 6, 6, 7, 3,
        // 底面
        4, 5, 1, 1, 0, 4
    };
}

int main() {
    try {
        std::cout << "=== Vulkan OBJ加载测试 ===" << std::endl;

        // 创建Vulkan运行时
        vulkan_runtime runtime;

        static bool use_obj = true;  // 设置为true来测试OBJ文件


        if (use_obj) {
            // 尝试加载OBJ文件
            try {
                const auto module1 = obj_loader::load_from_file("adm.obj");
                const auto module2 = obj_loader::load_from_file("room.obj");


                // 验证数据
                if (module1.vertices.empty() || module1.indices.empty()) {
                    std::cerr << "OBJ数据为空，使用测试立方体" << std::endl;
                    throw std::runtime_error("OBJ数据为空");
                }

                // 检查索引有效性
                for (const uint32_t idx : module1.indices) {
                    if (idx >= module1.vertices.size()) {
                        std::cerr << "错误: 索引 " << idx << " 超出顶点范围!" << std::endl;
                        throw std::runtime_error("索引无效");
                    }
                }

                // 提取位置和纹理坐标
                std::vector<glm::vec3> positions1;
                std::vector<glm::vec3> normals1;
                std::vector<glm::vec2> tex_coords1;
                positions1.reserve(module1.vertices.size());
                tex_coords1.reserve(module1.vertices.size());

                for (const auto& vertex : module1.vertices) {
                    positions1.push_back(vertex.position);
                    normals1.push_back(vertex.normal);
                    tex_coords1.push_back(vertex.tex_coord);
                }

                std::vector<glm::vec3> positions2;
                std::vector<glm::vec3> normals2;
                std::vector<glm::vec2> tex_coords2;
                positions2.reserve(module2.vertices.size());
                tex_coords2.reserve(module2.vertices.size());

                for (const auto& vertex : module2.vertices) {
                    positions2.push_back(vertex.position);
                    normals2.push_back(vertex.normal);
                    tex_coords2.push_back(vertex.tex_coord);
                }


                //runtime.add_mesh(positions2, normals2, tex_coords2, module2.indices, "room.png");
                runtime.add_mesh(positions1, normals1, tex_coords1, module1.indices);
                std::cout << "OBJ模型加载成功!" << std::endl;

            } catch (const std::exception& e) {
                std::cerr << "OBJ加载失败: " << e.what() << std::endl;
                std::cout << "使用测试立方体..." << std::endl;

                // 使用测试立方体
                std::vector<glm::vec3> positions;
                std::vector<glm::vec2> tex_coords;
                std::vector<uint32_t> indices;
                create_test_cube(positions, tex_coords, indices);
                runtime.set_buffer(positions, tex_coords, indices);
            }
        } else {
            // 直接使用测试立方体
            std::vector<glm::vec3> positions;
            std::vector<glm::vec2> tex_coords;
            std::vector<uint32_t> indices;
            create_test_cube(positions, tex_coords, indices);

            std::cout << "测试立方体数据:" << std::endl;
            std::cout << "  顶点数: " << positions.size() << std::endl;
            std::cout << "  索引数: " << indices.size() << std::endl;

            runtime.set_buffer(positions, tex_coords, indices);
        }

        // 帧率计数器
        auto last_time = std::chrono::high_resolution_clock::now();
        int frame_count = 0;

        std::cout << "\n开始渲染循环 (按ESC退出)..." << std::endl;

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
                std::cout << "重新加载模型..." << std::endl;
                // 这里可以添加重新加载逻辑
            } else if (glfwGetKey(runtime.get_window(), GLFW_KEY_R) == GLFW_RELEASE) {
                r_pressed = false;
            }

            try {
                runtime.render_frame_with_meshes();
                frame_count++;

                // 计算帧率
                auto current_time = std::chrono::high_resolution_clock::now();
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - last_time).count();

                if (elapsed >= 1000) {
                    const float fps = static_cast<float>(frame_count) * 1000.0f / static_cast<float>(elapsed);
                    std::cout << "\rFPS: " << fps << std::flush;
                    frame_count = 0;
                    last_time = current_time;
                }

            } catch (const std::exception& e) {
                std::cerr << "\n渲染错误: " << e.what() << std::endl;
                break;
            }
        }

        std::cout << "\n\n程序正常退出" << std::endl;
        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "\n=== 程序异常 ===" << std::endl;
        std::cerr << "错误: " << e.what() << std::endl;
        std::cout << "\n按 Enter 键退出..." << std::endl;
        std::cin.get();
        return EXIT_FAILURE;
    }
}


/*
int main() {
    try {
        std::cout << "=== Vulkan Runtime 测试（带UV支持）===" << std::endl;

        vulkan_runtime runtime;

        const auto module = obj_loader::load_from_file("room.obj");
        const png_texture_data texture_data = png_texture_loader::load_png_texture("room.png");


        runtime.set_buffer(module.positions, module.tex_coords, module.indices);
        runtime.load_png_texture_from_data(texture_data);
        runtime.export_loaded_texture_to_png("a.png");

        // 帧率计数器
        auto last_time = std::chrono::high_resolution_clock::now();
        int frame_count = 0;

        std::cout << "\n开始渲染循环 (按ESC退出)..." << std::endl;

        // 主循环
        while (!glfwWindowShouldClose(runtime.get_window())) {
            glfwPollEvents();

            // 检查ESC键
            if (glfwGetKey(runtime.get_window(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(runtime.get_window(), GLFW_TRUE);
            }

            // 可选：按空格键切换显示模式
            static bool show_uv_debug = true;
            static bool key_pressed = false;
            if (glfwGetKey(runtime.get_window(), GLFW_KEY_SPACE) == GLFW_PRESS) {

                if (!key_pressed) {
                    show_uv_debug = !show_uv_debug;
                    key_pressed = true;
                    std::cout << (show_uv_debug ? "切换到UV调试模式" : "切换到纹理采样模式") << std::endl;
                }
            } else {
                key_pressed = false;
            }

            try {
                runtime.render_frame();
                frame_count++;

                // 计算帧率
                auto current_time = std::chrono::high_resolution_clock::now();
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - last_time).count();

                if (elapsed >= 1000) {
                    float fps = frame_count * 1000.0f / elapsed;
                    std::cout << "\rFPS: " << fps;
                    frame_count = 0;
                    last_time = current_time;
                }

            } catch (const std::exception& e) {
                std::cerr << "\n渲染错误: " << e.what() << std::endl;
                break;
            }
        }


        std::cout << "\n\n程序正常退出" << std::endl;
        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "\n=== 程序异常 ===" << std::endl;
        std::cerr << "错误: " << e.what() << std::endl;
        std::cout << "\n按 Enter 键退出..." << std::endl;
        std::cin.get();
        return EXIT_FAILURE;
    }
}
*/