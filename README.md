# vulkan_project
Modern C++ Vulkan study project with GLTF support and RAII-style resource management.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Vulkan](https://img.shields.io/badge/Vulkan-1.3-red)](https://www.vulkan.org/)
[![C++](https://img.shields.io/badge/C%2B%2B-20/23-blue)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C)](https://cmake.org/)

## Features

### Core Graphics

- Vulkan 1.3 pipeline with MSAA anti-aliasing
- Dynamic viewport and scissor states
- Depth testing and color blending

### Memory Management

- VMA (Vulkan Memory Allocator) integration (via VMA)
- RAII wrappers with automatic cleanup
- enable_destruct_stack<> for deterministic resource cleanup

### Modern C++

- std::print
- std::source_location
- constexpr
- std::ranges
- aggregate initialization
- CRTP

### GLTF Support

- Implemented GLTF loading in gltf_loader

## Preview
![Running screen shot](./screenshots/running.png)

## Quick Start

### Requirements

- CMake 3.20+ 
- Vulkan SDK 1.2+
- glm 1.0+
- GLFW 3.0+
- Tinygltf
- Recent Clang (e.g., 16, 17)

### Building 

```bash
    git clone https://github.com/YzK0741/vulkan_project.git
    cd vulkan_project
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build .
```
If you want to try it yourself you can look up the main.cpp, the code is designed to be straightforward.

## Current Limitations

### Constant Pipeline

### PBR is not ready

### May kill the gpu driver

## Visions

### CreateInfo-driven construction

### Dynamic pipeline creating

### PBR-NBR mixed rendering

### Better GLTF loading

### Concepts and template based scene graph binding

## License
MIT, look at [`LICENSE`](LICENSE)
