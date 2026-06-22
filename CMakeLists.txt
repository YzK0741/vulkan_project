cmake_minimum_required(VERSION 4.1)
project(vulkan_project)
set(CMAKE_CXX_STANDARD 23)


# === 查找依赖包 ===
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(mimalloc REQUIRED)

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
        source/vulkan_core/vma/vma.cpp
        source/vulkan_core/vma/vma.h
        source/vulkan_core/vma/vma_waiter.cpp
        source/vulkan_core/vma/vma_waiter.h
        source/vulkan_runtime/texture.cpp
        source/vulkan_runtime/create_info.cpp
        source/vulkan_runtime/create_info.h
        source/shaders/shaders.cpp
        source/shaders/shaders.h
        source/vulkan_core/basic_pbr.cpp
        source/vulkan_core/basic_pbr.h
        source/platforms.h
        source/vulkan_core/vk_format.h
)

target_link_libraries(${PROJECT_NAME}
        Vulkan::Vulkan
        glfw
        backtrace
)

target_compile_options(vulkan_project PRIVATE
        $<$<CONFIG:Release>:
        -O3
        -fno-rtti
        -fno-exceptions
        -ffunction-sections
        -fdata-sections
        -flto
        -march=native
        -fno-unwind-tables      # 移除 unwind 表
        -fno-asynchronous-unwind-tables  # 移除异步 unwind
        -fmerge-all-constants  # 合并常量
        >
        $<$<CONFIG:Debug>:
        -g3                    # 最详细的调试信息
        -fno-omit-frame-pointer  # 保留帧指针
        -fstandalone-debug     # 独立的调试信息
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

target_compile_definitions(vulkan_project PRIVATE
        $<$<CONFIG:Debug>:
        _DEBUG
        BOOST_STACK_TRACE_USE_BACKTRACE
        >
)
