//
// Created by 小叶 on 2026/2/9.
//

#include <print>
#include <tinygltf/tiny_gltf.h>
#include "gltf_parser.h"

#include <iostream>

void load_gltf(const std::string_view path) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    auto success = loader.LoadBinaryFromFile(&model, &err, &warn, path.data());

    if (!warn.empty())
        std::cout << warn;
    if (!err.empty())
        std::cout << err;

    if (!success)
        throw std::runtime_error("failed load model");
    }

    std::println("加载模型成功")

}
