cmake_minimum_required(VERSION 4.1)
project(vulkan_project)
set(CMAKE_CXX_STANDARD 23)

# === 关键：明确指定使用MSYS2 Clang环境 ===
set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

# === 查找依赖包 ===
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_executable(vulkan_project main.cpp
        source/vulkan_core/vulkan_core.h
        source/vulkan_core/vulkan_core.cpp
        source/vulkan_runtime/vulkan_runtime.h
        source/vulkan_core/vulkan_utility.h
        source/vulkan_core/vulkan_utility.cpp
        source/loader/tinygltf_impl.cpp
        source/loader/obj_parser.cpp
        source/loader/obj_parser.h
        source/loader/png_loader.h
        source/vulkan_core/vulkan_buffer.cpp
        source/vulkan_core/vulkan_buffer.h
        source/loader/gltf_parser.cpp
        source/loader/gltf_parser.h
        source/utility.h
        source/utility.cpp
        source/vulkan_runtime/module.h
        source/vulkan_core/create_info.h
        source/vulkan_runtime/vulkan_runtime.cpp
)

target_link_libraries(${PROJECT_NAME}
        Vulkan::Vulkan
        glfw
)

target_compile_options(vulkan_project PRIVATE
        $<$<CONFIG:Release>:
        -O3
        -fno-rtti
        -fno-exceptions
        -ffunction-sections
        -fdata-sections
        -flto
        -fno-unwind-tables      # 移除 unwind 表
        -fno-asynchronous-unwind-tables  # 移除异步 unwind
        -fmerge-all-constants  # 合并常量
        >
)

target_link_options(vulkan_project PRIVATE
        $<$<CONFIG:Release>:
        -flto
        -s
        >
        -Wall
        -Wextra
        -Werror
)
