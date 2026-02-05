//
// Created by 小叶 on 2026/1/25.
//

#ifndef VULKAN_PROJECT_OBJ_PASER_H
#define VULKAN_PROJECT_OBJ_PASER_H

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <glm/glm.hpp>

#include "vulkan_utility.h"

// OBJ顶点索引结构
struct obj_vertex_index {
    int position_index;
    int texcoord_index;
    int normal_index;

    obj_vertex_index(int pos_idx = -1, int tex_idx = -1, int norm_idx = -1) noexcept
        : position_index(pos_idx), texcoord_index(tex_idx), normal_index(norm_idx) {}

    bool operator==(const obj_vertex_index& other) const {
        return position_index == other.position_index &&
               texcoord_index == other.texcoord_index &&
               normal_index == other.normal_index;
    }
};

// 自定义哈希函数
template<>
struct std::hash<obj_vertex_index> {
    std::size_t operator()(const obj_vertex_index& k) const noexcept {
        std::size_t seed = 0;
        hash_combine(seed, k.position_index);
        hash_combine(seed, k.texcoord_index);
        hash_combine(seed, k.normal_index);
        return seed;
    }

private:
    template <typename T>
    void hash_combine(std::size_t& seed, const T& val) const {
        seed ^= std::hash<T>{}(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
};

// OBJ模型数据结构
struct obj_model_data {
    std::vector<vertex> vertices;
    std::vector<uint32_t> indices;

    obj_model_data() = default;

    void clear() {
        vertices.clear();
        indices.clear();
    }

    [[nodiscard]] size_t vertex_count() const { return vertices.size(); }
    [[nodiscard]] size_t index_count() const { return indices.size(); }
};

// OBJ文件加载器类
class obj_loader {
public:
    static obj_model_data load_from_file(const std::string& filepath) {
        std::cout << "加载OBJ文件: " << filepath << std::endl;

        obj_model_data model;

        std::ifstream file(filepath);
        if (!file.is_open()) {
            throw std::runtime_error("无法打开OBJ文件: " + filepath);
        }

        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> texcoords;
        std::vector<glm::vec3> normals;

        std::unordered_map<obj_vertex_index, uint32_t> vertex_cache;

        std::string line;
        int line_num = 0;

        while (std::getline(file, line)) {
            line_num++;
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "v") {
                // 顶点位置
                glm::vec3 pos;
                iss >> pos.x >> pos.y >> pos.z;
                positions.push_back(pos);
            }
            else if (prefix == "vt") {
                // 纹理坐标
                glm::vec2 tex;
                iss >> tex.x >> tex.y;
                //tex.y = 1.0f - tex.y; // 翻转V坐标
                texcoords.push_back(tex);
            }
            else if (prefix == "vn") {
                // 法线
                glm::vec3 norm;
                iss >> norm.x >> norm.y >> norm.z;
                normals.push_back(glm::normalize(norm));
            }
            else if (prefix == "f") {
                // 面
                std::vector<obj_vertex_index> face_vertices;
                std::string vertex_str;

                while (iss >> vertex_str) {
                    obj_vertex_index vi = parse_vertex_index(vertex_str);

                    // 转换为从0开始的索引
                    if (vi.position_index > 0) vi.position_index -= 1;
                    if (vi.texcoord_index > 0) vi.texcoord_index -= 1;
                    if (vi.normal_index > 0) vi.normal_index -= 1;

                    face_vertices.push_back(vi);
                }

                // 三角化面（假设是凸多边形）
                if (face_vertices.size() >= 3) {
                    // 使用扇形三角化
                    for (size_t i = 1; i < face_vertices.size() - 1; i++) {
                        process_face_vertex(face_vertices[0], positions, texcoords, normals, model, vertex_cache);
                        process_face_vertex(face_vertices[i], positions, texcoords, normals, model, vertex_cache);
                        process_face_vertex(face_vertices[i + 1], positions, texcoords, normals, model, vertex_cache);
                    }
                }
            }
        }

        file.close();

        std::cout << "OBJ加载完成:" << std::endl;
        std::cout << "  位置数: " << positions.size() << std::endl;
        std::cout << "  纹理坐标数: " << texcoords.size() << std::endl;
        std::cout << "  法线数: " << normals.size() << std::endl;
        std::cout << "  生成顶点数: " << model.vertices.size() << std::endl;
        std::cout << "  生成索引数: " << model.indices.size() << std::endl;

        // 验证数据
        if (model.vertices.empty()) {
            throw std::runtime_error("没有生成任何顶点数据!");
        }

        // 打印前几个顶点和索引
        std::cout << "\n前5个顶点:" << std::endl;
        for (int i = 0; i < std::min(5, static_cast<int>(model.vertices.size())); i++) {
            const auto& v = model.vertices[i];
            std::cout << "  顶点[" << i << "]: pos=("
                      << v.position.x << ", " << v.position.y << ", " << v.position.z
                      << "), uv=(" << v.tex_coord.x << ", " << v.tex_coord.y << ")" << std::endl;
        }

        std::cout << "\n前5个三角形:" << std::endl;
        for (int i = 0; i < std::min(5, static_cast<int>(model.indices.size())/3); i++) {
            std::cout << "  三角形[" << i << "]: "
                      << model.indices[i*3] << ", "
                      << model.indices[i*3+1] << ", "
                      << model.indices[i*3+2] << std::endl;
        }

        return model;
    }

private:
    // 解析顶点索引字符串 "v/vt/vn"
    static obj_vertex_index parse_vertex_index(const std::string& vertex_str) {
        obj_vertex_index vi;

        std::stringstream ss(vertex_str);
        std::string token;
        int component_index = 0;

        while (std::getline(ss, token, '/')) {
            if (!token.empty()) {
                int value = std::stoi(token);

                switch (component_index) {
                    case 0: // 位置索引
                        vi.position_index = value;
                        break;
                    case 1: // 纹理坐标索引
                        vi.texcoord_index = value;
                        break;
                    case 2: // 法线索引
                        vi.normal_index = value;
                        break;
                    default:
                        break;
                }
            }
            component_index++;
        }

        return vi;
    }

    // 处理面的顶点
    static void process_face_vertex(
        const obj_vertex_index& vi,
        const std::vector<glm::vec3>& positions,
        const std::vector<glm::vec2>& texcoords,
        const std::vector<glm::vec3>& normals,
        obj_model_data& model,
        std::unordered_map<obj_vertex_index, uint32_t>& vertex_cache) {

        // 检查是否已存在此顶点
        auto it = vertex_cache.find(vi);
        if (it != vertex_cache.end()) {
            // 顶点已存在，添加索引
            model.indices.push_back(it->second);
            return;
        }

        // 创建新顶点
        vertex v{};

        // 设置位置
        if (vi.position_index >= 0 && vi.position_index < positions.size()) {
            v.position = positions[vi.position_index];
        } else {
            v.position = glm::vec3(0.0f);
            std::cerr << "警告: 位置索引 " << vi.position_index << " 超出范围!" << std::endl;
        }

        // 设置纹理坐标
        if (vi.texcoord_index >= 0 && vi.texcoord_index < texcoords.size()) {
            v.tex_coord = texcoords[vi.texcoord_index];
        } else {
            v.tex_coord = glm::vec2(0.0f);
            // 如果没有纹理坐标，使用默认值
            if (vi.texcoord_index != -1) {
                std::cerr << "警告: 纹理坐标索引 " << vi.texcoord_index << " 超出范围!" << std::endl;
            }
        }

        // 设置法线
        if (vi.normal_index >= 0 && vi.normal_index < normals.size()) {
            v.normal = normals[vi.normal_index];
        } else {
            // 计算面法线（简单处理）
            v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
            if (vi.normal_index != -1) {
                std::cerr << "警告: 法线索引 " << vi.normal_index << " 超出范围!" << std::endl;
            }
        }

        // 添加到顶点列表并缓存
        auto new_index = static_cast<uint32_t>(model.vertices.size());
        model.vertices.push_back(v);
        model.indices.push_back(new_index);
        vertex_cache[vi] = new_index;
    }
};

#endif // VULKAN_PROJECT_OBJ_PASER_H