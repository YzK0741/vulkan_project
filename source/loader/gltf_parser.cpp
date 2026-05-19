//
// Created by 小叶 on 2026/2/9.
//

#include <print>
#include <tinygltf/tiny_gltf.h>
#include <glm/glm.hpp>
#include  <vulkan/vulkan.h>
#include "../utility.h"
#include "gltf_parser.h"
// 改进的辅助函数：从 accessor 读取数据（考虑类型大小和步长）
template<typename T>
std::vector<T> get_accessor_data(const tinygltf::Model& model, int accessor_index) {
    const auto& accessor = model.accessors[accessor_index];

    // 检查 bufferView 是否有效
    if (accessor.bufferView < 0) {
        std::println(stderr, "警告: accessor {} 没有 bufferView", accessor_index);
        return {};
    }

    const auto& buffer_view = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[buffer_view.buffer];

    // 计算元素大小（考虑类型）
    int component_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    int element_size = component_size * accessor.type;  // type 是元素数量（如 3 表示 vec3）

    std::vector<T> data(accessor.count * accessor.type);
    const uint8_t* ptr = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;

    // 考虑字节步长（stride）
    size_t stride = buffer_view.byteStride ? buffer_view.byteStride : element_size;

    for (size_t i = 0; i < accessor.count; ++i) {
        memcpy(&data[i * accessor.type], ptr + i * stride, element_size);
    }

    return data;
}

VkFormat get_vk_format_from_image(const tinygltf::Image& image) {
    if (image.bits == 8) {
        switch (image.component) {
            case 1: return VK_FORMAT_R8_UNORM;
            case 2: return VK_FORMAT_R8G8_UNORM;
            case 3: return VK_FORMAT_R8G8B8_UNORM;
            case 4: return VK_FORMAT_R8G8B8A8_UNORM;
            default: return VK_FORMAT_UNDEFINED;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

// 主处理函数
// 主处理函数
std::vector<gltf_data> process_gltf_model(const tinygltf::Model& model) {
    std::vector<gltf_data> meshes_data;

    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            model_data mesh_data;

            // 1. 检查是否有必要的数据
            if (!primitive.attributes.contains("POSITION")) {
                std::println("警告: 图元缺少位置数据");
                continue;
            }

            // 2. 提取顶点数据
            std::vector<glm::vec3> positions;
            std::vector<glm::vec3> normals;
            std::vector<glm::vec2> tex_coords;

            // 读取位置
            if (primitive.attributes.contains("POSITION")) {
                auto pos_data = get_accessor_data<float>(model, primitive.attributes.at("POSITION"));
                for (size_t i = 0; i < pos_data.size(); i += 3) {
                    positions.emplace_back(pos_data[i], pos_data[i + 1], pos_data[i + 2]);
                }
            }

            // 读取法线（可选）
            if (primitive.attributes.contains("NORMAL")) {
                auto normal_data = get_accessor_data<float>(model, primitive.attributes.at("NORMAL"));
                for (size_t i = 0; i < normal_data.size(); i += 3) {
                    normals.emplace_back(normal_data[i], normal_data[i + 1], normal_data[i + 2]);
                }
            }

            // 读取纹理坐标（可选）
            if (primitive.attributes.contains("TEXCOORD_0")) {
                auto tex_data = get_accessor_data<float>(model, primitive.attributes.at("TEXCOORD_0"));
                for (size_t i = 0; i < tex_data.size(); i += 2) {
                    tex_coords.emplace_back(tex_data[i], tex_data[i + 1]);
                }
            }

            // 3. 构建顶点数组
            mesh_data.vertices.resize(positions.size());
            for (size_t i = 0; i < positions.size(); ++i) {
                mesh_data.vertices[i].position = positions[i];
                mesh_data.vertices[i].normal = i < normals.size() ? normals[i] : glm::vec3(0.0f);
                mesh_data.vertices[i].tex_coord = i < tex_coords.size() ? tex_coords[i] : glm::vec2(0.0f);
            }

            // 4. 处理索引（修复版本）
            if (primitive.indices >= 0) {
                const auto& index_accessor = model.accessors[primitive.indices];

                // 检查 bufferView 是否有效
                if (index_accessor.bufferView < 0) {
                    std::println(stderr, "警告: 索引 accessor 没有 bufferView");
                    // 如果没有索引buffer，生成顺序索引
                    mesh_data.indices.resize(mesh_data.vertices.size());
                    for (size_t i = 0; i < mesh_data.vertices.size(); ++i) {
                        mesh_data.indices[i] = static_cast<uint32_t>(i);
                    }
                    continue;
                }

                const auto& index_buffer_view = model.bufferViews[index_accessor.bufferView];
                const auto& index_buffer = model.buffers[index_buffer_view.buffer];

                // 计算数据指针
                const uint8_t* index_ptr = index_buffer.data.data() +
                    index_buffer_view.byteOffset + index_accessor.byteOffset;

                mesh_data.indices.resize(index_accessor.count);

                // 根据索引类型读取（支持所有 glTF 标准索引类型）
                switch (index_accessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                        const auto* indices8 = reinterpret_cast<const uint8_t*>(index_ptr);
                        for (size_t i = 0; i < index_accessor.count; ++i) {
                            mesh_data.indices[i] = indices8[i];
                        }
                        break;
                    }

                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                        const auto* indices16 = reinterpret_cast<const uint16_t*>(index_ptr);
                        for (size_t i = 0; i < index_accessor.count; ++i) {
                            mesh_data.indices[i] = indices16[i];
                        }
                        break;
                    }

                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                        const auto* indices32 = reinterpret_cast<const uint32_t*>(index_ptr);
                        for (size_t i = 0; i < index_accessor.count; ++i) {
                            mesh_data.indices[i] = indices32[i];
                        }
                        break;
                    }

                    default: {
                        std::println(stderr, "警告: 不支持的索引类型: {}", index_accessor.componentType);
                        // 不支持的类型，生成顺序索引
                        mesh_data.indices.resize(mesh_data.vertices.size());
                        for (size_t i = 0; i < mesh_data.vertices.size(); ++i) {
                            mesh_data.indices[i] = static_cast<uint32_t>(i);
                        }
                        break;
                    }
                }
            } else {
                // 如果没有索引，生成顺序索引
                mesh_data.indices.resize(mesh_data.vertices.size());
                for (size_t i = 0; i < mesh_data.vertices.size(); ++i) {
                    mesh_data.indices[i] = static_cast<uint32_t>(i);
                }
                std::println("没有索引数据，使用顺序索引");
            }

            std::vector<unsigned char> texture_data = {};
            int width = 0;
            int height = 0;
            VkFormat format = VK_FORMAT_UNDEFINED;
            if (primitive.material >= 0) {
                const tinygltf::Material& material = model.materials[primitive.material];

                if (const int& base_texture_index = material.pbrMetallicRoughness.baseColorTexture.index;
                    base_texture_index >= 0 && base_texture_index < model.textures.size()) {
                    const tinygltf::Texture& texture = model.textures[base_texture_index];

                    if (texture.source >= 0 && texture.source < model.images.size()) {
                        const tinygltf::Image& image = model.images[texture.source];
                        texture_data = image.image;
                        width = image.width;
                        height = image.height;
                        format = get_vk_format_from_image(image);

                        std::println("加载纹理成功，纹理信息: \n width: {} height: {}", width, height);
                    } else {
                        std::println(stderr, "找到了texture_index但没有发现source");
                    }
                } else {
                    std::println(stderr, "找到了material但没有发现texture_index");
                }
            } else {
                std::println(stderr,"没有纹理");
            }

            meshes_data.emplace_back(std::move(mesh_data), std::move(texture_data), width, height, format);
        }
    }
    return meshes_data;
}

std::vector<gltf_data> load_gltf(const std::string_view path) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    bool success = {false};
    if (path.ends_with("glb")) {
        success = loader.LoadBinaryFromFile(&model, &err, &warn, path.data());
    } else if (path.ends_with("gltf")) {
        success = loader.LoadASCIIFromFile(&model, &err, &warn, path.data());
    } else {
        std::println(stderr, "wrong name! name does no ends with 'glb' or 'gltf'");
        std::println("file path: {}", path);
        print_stacktrace_and_terminate();
    }
    if (!warn.empty())
        std::println("{}", warn);
    if (!err.empty())
        std::println("{}", err);

    if (!success){
        std::println("failed load model");
        print_stacktrace_and_terminate();
    }

    std::println("加载模型成功");

    return process_gltf_model(model);
}

std::future<std::vector<gltf_data>> load_gltf_async(std::string_view path) {
    return std::async(std::launch::async, load_gltf, path);
}