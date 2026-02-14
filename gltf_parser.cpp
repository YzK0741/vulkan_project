//
// Created by 小叶 on 2026/2/9.
//

#include <print>
#include <tinygltf/tiny_gltf.h>
#include "gltf_parser.h"
#include "utility.h"

void load_gltf(const std::string_view path) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    auto success = loader.LoadBinaryFromFile(&model, &err, &warn, path.data());

    if (!warn.empty())
        std::println("{}", warn);
    if (!err.empty())
        std::println("{}", err);

    if (!success){
        std::println("failed load model");
        print_stacktrace_and_terminate();
    }

    std::println("加载模型成功");

}
