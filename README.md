# vulkan_project
Modern C++ Vulkan study project with GLTF support and RAII-style resource management.

## Features

### 1. Core Graphics

- Vulkan 1.3 pipeline with MSAA anti-aliasing
- Dynamic viewport and scissor states
- Depth testing and color blending

### 2. Memory Management

- VMA (Vulkan Memory Allocator) integration (via VMA)
- RAII wrappers with automatic cleanup
- enable_destruct_stack<> for deterministic resource cleanup

### 3. Modern C++

- std::print
- std::source_location
- constexpr
- std::ranges
- aggregate initialization
- CRTP

### GLTF Support

- Implemented GLTF loading in gltf_loader

## Quick Start

### Requirements

- CMake 3.20+ 
- Vulkan SDK 1.2+
- glm 1.0+
- GLFW 1.0+
- Tinygltf
- Recent Clang (e.g., 16, 17)

### Building 

```bash
    git clone https://github.com/YzK0741/vulkan_project.git
    cd vulkan_project
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release
```
If you want to try it yourself you can look up the main.cpp believe me it's easy.


## Visions

### CreateInfo-driven construction

### Dynamic pipeline creating

### PBR-NBR mixed rendering

### Better GLTF loading

### Concepts and template based scene graph binding

## License
MIT, look at [`LICENSE`](LICENSE)

## Preview
![Running screen shot](./screenshots/running.png)