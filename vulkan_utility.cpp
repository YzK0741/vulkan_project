//
// Created by 小叶 on 2026/1/24.
//

#include <string_view>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>
#include <tuple>

#include <tinygltf/tiny_gltf.h>
#include <stb/stb_image.h>
#include "vulkan_utility.h"


std::vector<glm::vec3> extract_positions(const tinygltf::Model& model) {
    const int position_accessor_index = model.meshes[0].primitives[0].attributes.at("POSITION");
    std::vector<glm::vec3> positions;

    const auto& accessor = model.accessors[position_accessor_index];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];

    // 验证数据类型
    if (accessor.type != TINYGLTF_TYPE_VEC3 ||
        accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
        return positions;
        }

    // 直接创建适当大小的vector
    positions.resize(accessor.count);

    // 计算数据大小和起始位置
    const size_t data_size = accessor.count * sizeof(glm::vec3);
    const unsigned char* srcData = buffer.data.data() +
                                   bufferView.byteOffset +
                                   accessor.byteOffset;

    // 一次性复制所有数据
    memcpy(positions.data(), srcData, data_size);

    return positions;
}

std::vector<uint32_t> extract_indices(const tinygltf::Model& model) {
    const int indices_accessor_index = model.meshes[0].primitives[0].indices;
    std::vector<uint32_t> indices;

    const auto& accessor = model.accessors[indices_accessor_index];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];

    // 验证必须是标量类型
    if (accessor.type != TINYGLTF_TYPE_SCALAR) {
        std::cerr << "错误：索引属性不是SCALAR类型" << std::endl;
        return indices;
    }

    // 准备数据
    indices.resize(accessor.count);

    // 计算数据起始位置
    const unsigned char* srcData = buffer.data.data() +
                                   bufferView.byteOffset +
                                   accessor.byteOffset;

    // 根据不同的索引类型处理
    switch (accessor.componentType) {
        // 情况1：32位无符号整数（最常见）
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            // 直接内存拷贝（最快）
            memcpy(indices.data(), srcData, accessor.count * sizeof(uint32_t));
            break;
        }

        // 情况2：16位无符号整数（也很常见）
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            const auto* srcIndices = reinterpret_cast<const uint16_t*>(srcData);

            // 需要转换到32位
            for (size_t i = 0; i < accessor.count; ++i) {
                indices[i] = static_cast<uint32_t>(srcIndices[i]);
            }
            break;
        }

        // 情况3：8位无符号整数
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
            const auto* srcIndices = reinterpret_cast<const uint8_t*>(srcData);

            for (size_t i = 0; i < accessor.count; ++i) {
                indices[i] = static_cast<uint32_t>(srcIndices[i]);
            }
            break;
        }

        // 情况4：有符号整数（较少见）
        case TINYGLTF_COMPONENT_TYPE_INT: {
            const auto* srcIndices = reinterpret_cast<const int32_t*>(srcData);

            for (size_t i = 0; i < accessor.count; ++i) {
                indices[i] = static_cast<uint32_t>(srcIndices[i]);
            }
            break;
        }

        // 情况5：有符号短整型
        case TINYGLTF_COMPONENT_TYPE_SHORT: {
            const auto* srcIndices = reinterpret_cast<const int16_t*>(srcData);

            for (size_t i = 0; i < accessor.count; ++i) {
                indices[i] = static_cast<uint32_t>(srcIndices[i]);
            }
            break;
        }

        default:
            std::cerr << "不支持的索引组件类型: " << accessor.componentType << std::endl;
            indices.clear();
    }

    return indices;
}

// 辅助函数：安全的归一化法线
void NormalizeNormals(std::vector<glm::vec3>& normals) {
    for (auto& normal : normals) {
        const float lengthSquared = normal.x * normal.x +
                             normal.y * normal.y +
                             normal.z * normal.z;

        if (constexpr float epsilon = 0.00001f; lengthSquared > epsilon) {
            const float invLength = 1.0f / std::sqrt(lengthSquared);
            normal.x *= invLength;
            normal.y *= invLength;
            normal.z *= invLength;
        } else {
            // 避免零长度法线，设置为默认向上向量
            normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
}

std::vector<glm::vec3> extract_normals(const tinygltf::Model& model) {
    const int normals_accessor_index = model.meshes[0].primitives[0].attributes.at("NORMAL");
    std::vector<glm::vec3> normals;

    const auto& accessor = model.accessors[normals_accessor_index];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];

    // 验证法线数据类型
    if (accessor.type != TINYGLTF_TYPE_VEC3) {
        std::cerr << "错误：法线属性不是VEC3类型" << std::endl;
        return normals;
    }

    // 调整大小
    normals.resize(accessor.count);

    // 计算数据起始位置
    const unsigned char* srcData = buffer.data.data() +
                                   bufferView.byteOffset +
                                   accessor.byteOffset;

    // 根据不同的组件类型处理
    switch (accessor.componentType) {
        // 情况1：浮点数（最常见）
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
            // 直接内存拷贝（最快）
            size_t dataSize = static_cast<size_t>(accessor.count) * sizeof(glm::vec3);
            memcpy(normals.data(), srcData, dataSize);

            // 确保法线是单位向量（可选的归一化步骤）
            NormalizeNormals(normals);
            break;
        }

        // 情况2：有符号字节（8位归一化）
        case TINYGLTF_COMPONENT_TYPE_BYTE: {
            const auto* byteData = reinterpret_cast<const int8_t*>(srcData);

            for (size_t i = 0; i < static_cast<size_t>(accessor.count); ++i) {
                // 从[-127, 127]映射到[-1.0, 1.0]
                const size_t baseIdx = i * 3;
                normals[i] = glm::vec3(
                    static_cast<float>(byteData[baseIdx]) / 127.0f,
                    static_cast<float>(byteData[baseIdx + 1]) / 127.0f,
                    static_cast<float>(byteData[baseIdx + 2]) / 127.0f
                );
            }
            break;
        }

        // 情况3：无符号字节（8位归一化）
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
            const auto* byteData = reinterpret_cast<const uint8_t*>(srcData);

            for (size_t i = 0; i < static_cast<size_t>(accessor.count); ++i) {
                // 从[0, 255]映射到[-1.0, 1.0]
                const size_t baseIdx = i * 3;
                const float normalizedX = static_cast<float>(byteData[baseIdx]) / 255.0f;
                const float normalizedY = static_cast<float>(byteData[baseIdx + 1]) / 255.0f;
                const float normalizedZ = static_cast<float>(byteData[baseIdx + 2]) / 255.0f;

                normals[i] = glm::vec3(
                    normalizedX * 2.0f - 1.0f,
                    normalizedY * 2.0f - 1.0f,
                    normalizedZ * 2.0f - 1.0f
                );
            }
            break;
        }

        // 情况4：有符号短整型（16位归一化）
        case TINYGLTF_COMPONENT_TYPE_SHORT: {
            const auto* shortData = reinterpret_cast<const int16_t*>(srcData);

            for (size_t i = 0; i < static_cast<size_t>(accessor.count); ++i) {
                // 从[-32767, 32767]映射到[-1.0, 1.0]
                const size_t baseIdx = i * 3;
                normals[i] = glm::vec3(
                    static_cast<float>(shortData[baseIdx]) / 32767.0f,
                    static_cast<float>(shortData[baseIdx + 1]) / 32767.0f,
                    static_cast<float>(shortData[baseIdx + 2]) / 32767.0f
                );
            }
            break;
        }

        // 情况5：无符号短整型（16位归一化）
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            const auto* shortData = reinterpret_cast<const uint16_t*>(srcData);

            for (size_t i = 0; i < static_cast<size_t>(accessor.count); ++i) {
                // 从[0, 65535]映射到[-1.0, 1.0]
                const size_t baseIdx = i * 3;
                const float normalizedX = static_cast<float>(shortData[baseIdx]) / 65535.0f;
                const float normalizedY = static_cast<float>(shortData[baseIdx + 1]) / 65535.0f;
                const float normalizedZ = static_cast<float>(shortData[baseIdx + 2]) / 65535.0f;

                normals[i] = glm::vec3(
                    normalizedX * 2.0f - 1.0f,
                    normalizedY * 2.0f - 1.0f,
                    normalizedZ * 2.0f - 1.0f
                );
            }
            break;
        }

        // 情况6：有符号整数（罕见）
        case TINYGLTF_COMPONENT_TYPE_INT: {
            const auto* intData = reinterpret_cast<const int32_t*>(srcData);

            for (size_t i = 0; i < static_cast<size_t>(accessor.count); ++i) {
                const size_t baseIdx = i * 3;
                normals[i] = glm::vec3(
                    static_cast<float>(intData[baseIdx]),
                    static_cast<float>(intData[baseIdx + 1]),
                    static_cast<float>(intData[baseIdx + 2])
                );
            }
            break;
        }

        // 情况7：无符号整数（罕见）
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            const auto* intData = reinterpret_cast<const uint32_t*>(srcData);

            for (size_t i = 0; i < static_cast<size_t>(accessor.count); ++i) {
                const size_t baseIdx = i * 3;
                normals[i] = glm::vec3(
                    static_cast<float>(intData[baseIdx]),
                    static_cast<float>(intData[baseIdx + 1]),
                    static_cast<float>(intData[baseIdx + 2])
                );
            }
            break;
        }

        default:
            std::cerr << "不支持的组件类型: " << accessor.componentType << std::endl;
            normals.clear();
    }

    return normals;
}

std::vector<glm::vec2> extract_tex_coords(const tinygltf::Model& model) {
    const int tex_coord_accessor_index = model.meshes[0].primitives[0].attributes.at("TEXCOORD_0");
    std::vector<glm::vec2> texCoords;

    const auto& accessor = model.accessors[tex_coord_accessor_index];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];

    // 验证必须是VEC2类型
    if (accessor.type != TINYGLTF_TYPE_VEC2) {
        std::cerr << "错误：TEXCOORD属性不是VEC2类型" << std::endl;
        return texCoords;
    }

    // 准备存储空间
    texCoords.resize(accessor.count);

    // 获取原始数据指针
    const unsigned char* srcData = buffer.data.data() +
                                   bufferView.byteOffset +
                                   accessor.byteOffset;

    // 根据数据类型处理（避免narrowing）
    switch (accessor.componentType) {
        // 情况1：浮点数（最直接）
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
            const auto* floatData = reinterpret_cast<const float*>(srcData);

            // 使用static_cast确保安全转换
            for (size_t i = 0; i < accessor.count; ++i) {
                texCoords[i] = glm::vec2(
                    static_cast<float>(floatData[i * 2]),
                    static_cast<float>(floatData[i * 2 + 1])
                );
            }
            break;
        }

        // 情况2：无符号字节（需要归一化）
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
            const auto* byteData = reinterpret_cast<const uint8_t*>(srcData);

            for (size_t i = 0; i < accessor.count; ++i) {
                // 使用明确的浮点除法，避免整数除法
                texCoords[i] = glm::vec2(
                    static_cast<float>(byteData[i * 2]) / 255.0f,
                    static_cast<float>(byteData[i * 2 + 1]) / 255.0f
                );
            }
            break;
        }

        // 情况3：有符号字节（需要归一化和偏移）
        case TINYGLTF_COMPONENT_TYPE_BYTE: {
            const auto* byteData = reinterpret_cast<const int8_t*>(srcData);

            for (size_t i = 0; i < accessor.count; ++i) {
                // 从 [-128, 127] 映射到 [0, 1]
                texCoords[i] = glm::vec2(
                    (static_cast<float>(byteData[i * 2]) + 128.0f) / 255.0f,
                    (static_cast<float>(byteData[i * 2 + 1]) + 128.0f) / 255.0f
                );
            }
            break;
        }

        // 情况4：无符号短整型
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            const auto* shortData = reinterpret_cast<const uint16_t*>(srcData);

            for (size_t i = 0; i < accessor.count; ++i) {
                // 使用较大的浮点常量避免精度问题
                texCoords[i] = glm::vec2(
                    static_cast<float>(shortData[i * 2]) / 65535.0f,
                    static_cast<float>(shortData[i * 2 + 1]) / 65535.0f
                );
            }
            break;
        }

        // 情况5：有符号短整型
        case TINYGLTF_COMPONENT_TYPE_SHORT: {
            const auto* shortData = reinterpret_cast<const int16_t*>(srcData);

            for (size_t i = 0; i < accessor.count; ++i) {
                // 从 [-32768, 32767] 映射到 [0, 1]
                texCoords[i] = glm::vec2(
                    (static_cast<float>(shortData[i * 2]) + 32768.0f) / 65535.0f,
                    (static_cast<float>(shortData[i * 2 + 1]) + 32768.0f) / 65535.0f
                );
            }
            break;
        }

        // 情况6：无符号整数（较少见）
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            const auto* intData = reinterpret_cast<const uint32_t*>(srcData);

            for (size_t i = 0; i < accessor.count; ++i) {
                // 使用double避免精度丢失
                texCoords[i] = glm::vec2(
                    static_cast<float>(static_cast<double>(intData[i * 2]) / 4294967295.0),
                    static_cast<float>(static_cast<double>(intData[i * 2 + 1]) / 4294967295.0)
                );
            }
            break;
        }

        default:
            std::cerr << "不支持的TEXCOORD组件类型: " << accessor.componentType << std::endl;
            texCoords.clear();
    }

    return texCoords;
}

std::vector<vertex> make_vertexes(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals, const std::vector<glm::vec2>& tex_coords) {
    std::vector<vertex> vertexes;
    vertexes.resize(positions.size());
    for (int i = 0; i < positions.size(); i++) {
        vertexes.emplace_back(positions[i], normals[i], tex_coords[i]);
    }
    return vertexes;
}

std::pair<std::vector<vertex>, std::vector<uint32_t>>
make_unique_vertex(const std::vector<vertex>& vertices, const std::vector<uint32_t>& indices) {

    // 输入验证
    if (vertices.empty()) {
        throw std::invalid_argument("顶点数组不能为空");
    }

    if (indices.empty()) {
        throw std::invalid_argument("索引数组不能为空");
    }

    // 检查索引是否有效
    for (uint32_t idx : indices) {
        if (idx >= vertices.size()) {
            throw std::out_of_range("索引 " + std::to_string(idx) +
                                   " 超出顶点数组范围");
        }
    }

    std::vector<vertex> unique_vertices;
    std::vector<uint32_t> unique_indices;

    // 使用自定义哈希和相等比较器
    std::unordered_map<vertex, uint32_t> vertex_map;

    // 预分配内存提高性能
    unique_vertices.reserve(vertices.size());
    unique_indices.reserve(indices.size());
    vertex_map.reserve(vertices.size());

    // 处理每个索引
    for (uint32_t original_idx : indices) {
        const vertex& v = vertices[original_idx];

        // 查找或插入顶点
        auto result = vertex_map.find(v);

        if (result != vertex_map.end()) {
            // 顶点已存在，使用现有索引
            unique_indices.push_back(result->second);
        } else {
            // 新顶点
            auto new_idx = static_cast<uint32_t>(unique_vertices.size());
            unique_vertices.push_back(v);
            unique_indices.push_back(new_idx);
            vertex_map[v] = new_idx;
        }
    }

    // 输出去重统计信息
    if (!vertices.empty()) {
        const float reduction_percent = 100.0f *
            (1.0f - static_cast<float>(unique_vertices.size()) / static_cast<float>(vertices.size()));

        std::cout << "顶点去重统计:" << std::endl;
        std::cout << "  原始顶点数: " << vertices.size() << std::endl;
        std::cout << "  唯一顶点数: " << unique_vertices.size() << std::endl;
        std::cout << "  索引数: " << unique_indices.size() << std::endl;
        std::cout << "  减少比例: " << std::fixed << std::setprecision(1)
                  << reduction_percent << "%" << std::endl;
    }

    // 可选：压缩内存
    unique_vertices.shrink_to_fit();
    unique_indices.shrink_to_fit();

    return {std::move(unique_vertices), std::move(unique_indices)};
}



std::pair<std::vector<vertex>, std::vector<uint32_t>>
get_gltf_module_from_file(const std::string_view path) {
    if (const std::filesystem::path file_path(path); !is_regular_file(file_path)) {
        throw std::runtime_error(std::format("module file '{}' does not exist", path));
    }

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;
    const auto ret = loader.LoadASCIIFromFile(&model, &err, &warn, std::string(path));

    if (!warn.empty()) {
        std::cerr << "警告: " << warn << std::endl;
    }

    if (!err.empty()) {
        throw std::runtime_error(std::format("错误: {}", err));
    }

    if (!ret) {
        throw std::runtime_error("加载失败");
    }

    std::vector<glm::vec3> positions = extract_positions(model);// 顶点位置
    std::vector<uint32_t> indices = extract_indices(model);// 索引
    std::vector<glm::vec3> normals = extract_normals(model);// 法线
    std::vector<glm::vec2> tex_coords = extract_tex_coords(model);//uv
    std::vector<vertex> vertexes = make_vertexes(positions, normals, tex_coords);
    return make_unique_vertex(vertexes, indices);
}

std::tuple<std::vector<glm::vec3>, std::vector<glm::vec3>, std::vector<glm::vec2>>
unpack(const std::vector<vertex> &vertexes) {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> tex_coords;
    positions.resize(vertexes.size());
    normals.resize(vertexes.size());
    tex_coords.resize(vertexes.size());
    for (const auto& vertex : vertexes) {
        positions.push_back(vertex.position);
        normals.push_back(vertex.normal);
        tex_coords.push_back(vertex.tex_coord);
    }
    return {std::move(positions), std::move(normals), std::move(tex_coords)};
}

uint32_t find_memory_type(const uint32_t& type_filter, const VkMemoryPropertyFlags& properties, const VkPhysicalDevice& physical_device) {
    // 获取物理设备的内存属性
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

    // 遍历所有内存类型
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        // 检查内存类型是否满足过滤条件
        // type_filter 是一个位掩码，每个位对应一个内存类型
        if ((type_filter & (1 << i)) &&
            // 检查内存属性是否满足要求
            ((mem_properties.memoryTypes[i].propertyFlags & properties) == properties)) {
            return i;  // 返回找到的内存类型索引
            }
    }

    // 如果没有找到合适的内存类型
    throw std::runtime_error("failed to find suitable memory type!");
}

stb_texture create_texture_image_from_file() {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    const VkDeviceSize image_size = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }
    const stb_texture texture = {texWidth, texHeight, pixels, image_size};
    return texture;
}

// 最简单的函数：将stb_texture转换为buffer_resource（只包含图像数据）
buffer_resource create_buffer_from_stb(VkDevice device,
                                      VkPhysicalDevice physical_device,
                                      const stb_texture& texture) {

    // 1. 创建缓冲区
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = texture.image_size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |  // 可以用于传输
                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |  // 可以作为顶点数据
                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT;    // 可以作为索引数据
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("无法创建缓冲区!");
    }

    // 2. 分配内存
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                  physical_device);

    VkDeviceMemory memory;
    if (vkAllocateMemory(device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        throw std::runtime_error("无法分配缓冲区内存!");
    }

    // 3. 绑定内存
    vkBindBufferMemory(device, buffer, memory, 0);

    // 4. 复制数据到缓冲区
    void* data;
    vkMapMemory(device, memory, 0, texture.image_size, 0, &data);
    memcpy(data, texture.pixels, static_cast<size_t>(texture.image_size));
    vkUnmapMemory(device, memory);

    // 5. 创建并返回buffer_resource
    buffer_resource resource(device);
    resource.set(buffer, memory, texture.image_size, device);

    return resource;
}