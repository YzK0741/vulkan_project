#include "obj_parser.h"

void print_model_info(const obj_model_data& model) {
    std::cout << "=== 模型信息 ===" << std::endl;
    std::cout << "顶点数量: " << model.vertices.size() << std::endl;
    std::cout << "索引数量: " << model.indices.size() << std::endl;

    // 打印顶点数据
    std::cout << "\n顶点数据:" << std::endl;
    for (size_t i = 0; i < model.vertices.size(); i++) {
        const auto& v = model.vertices[i];
        std::cout << "顶点[" << i << "]:" << std::endl;
        std::cout << "  位置: (" << v.position.x << ", "
                  << v.position.y << ", " << v.position.z << ")" << std::endl;
        std::cout << "  纹理: (" << v.tex_coord.x << ", "
                  << v.tex_coord.y << ")" << std::endl;
        std::cout << "  法线: (" << v.normal.x << ", "
                  << v.normal.y << ", " << v.normal.z << ")" << std::endl;
    }

    // 打印索引数据
    std::cout << "\n索引数据:" << std::endl;
    for (size_t i = 0; i < model.indices.size(); i += 3) {
        if (i + 2 < model.indices.size()) {
            std::cout << "三角形[" << i/3 << "]: "
                      << model.indices[i] << ", "
                      << model.indices[i+1] << ", "
                      << model.indices[i+2] << std::endl;
        }
    }

    // 验证索引有效性
    bool indices_valid = true;
    for (uint32_t idx : model.indices) {
        if (idx >= model.vertices.size()) {
            std::cout << "错误: 索引 " << idx << " 超出顶点范围!" << std::endl;
            indices_valid = false;
        }
    }

    if (indices_valid) {
        std::cout << "\n✓ 索引有效!" << std::endl;
    }
}