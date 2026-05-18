//
// Created by 小叶 on 2026/5/17.
//
#include "vulkan_runtime.h"

namespace vulkan_runtime {
    runtime::runtime()
            : core(),
              creator(core.device, core.physical_device, core.command_pool, core.graphics_queue),
              vertex_buffer(core.device),
              index_buffer(core.device) {
        // 创建单一的全局管线
        graphics_pipeline = create_pipeline();
        create_pipeline_layout();
    }

    void runtime::create_uniform_buffers() {
        uniform_buffers.resize(core.swap_chain_images.size());
        uniform_buffers_memory.resize(core.swap_chain_images.size());
        uniform_buffers_mapped.resize(core.swap_chain_images.size());

        for (size_t i = 0; i < core.swap_chain_images.size(); i++) {
            constexpr VkDeviceSize buffer_size = sizeof(uniform_buffer_object);
            create_buffer(
                core.device,
                core.physical_device,
                buffer_size,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniform_buffers[i],
                uniform_buffers_memory[i]
            );

            vkMapMemory(core.device, uniform_buffers_memory[i], 0, buffer_size, 0, &uniform_buffers_mapped[i]);
        }
    }

    void runtime::update_descriptor_sets() const {
        if (descriptor_set == VK_NULL_HANDLE) {
            return;
        }

        std::array<VkWriteDescriptorSet, 1> descriptor_writes = {};

        // Uniform Buffer描述符
        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = uniform_buffers[0];  // 使用第一个缓冲区
        buffer_info.offset = 0;
        buffer_info.range = sizeof(uniform_buffer_object);

        descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet = descriptor_set;
        descriptor_writes[0].dstBinding = 0;
        descriptor_writes[0].dstArrayElement = 0;
        descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].pBufferInfo = &buffer_info;


        vkUpdateDescriptorSets(core.device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }

    void runtime::create_pipeline_layout() {
        VkPipelineLayoutCreateInfo pipeline_layout_info = {};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &core.descriptor_set_layout;
        pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(core.device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
            std::println("failed to create pipeline layout!");
            print_stacktrace_and_terminate();
        }
    }

    VkPipeline runtime::create_pipeline()  {
            // 1. 验证核心对象
            if (core.device == VK_NULL_HANDLE) {
                std::println("Vulkan device is not initialized");
                print_stacktrace_and_terminate();
            }

            if (core.swap_chain_extent.width == 0 || core.swap_chain_extent.height == 0) {
                std::println("Swap chain extent is not initialized");
                print_stacktrace_and_terminate();
            }

            if (core.renderpass == VK_NULL_HANDLE) {
                std::println("Render pass is not initialized");
                print_stacktrace_and_terminate();
            }

            this->pipeline_layout = this->core.pipeline_layout;
            if (pipeline_layout == VK_NULL_HANDLE) {
                std::println("Pipeline layout is not initialized");
                print_stacktrace_and_terminate();
            }

            // 使用全局的顶点输入描述（与vertex结构匹配）
            auto binding_description = vertex::get_binding_descriptions();
            auto attribute_descriptions = vertex::get_attribute_descriptions();

            VkPipelineVertexInputStateCreateInfo vertex_input_info{};
            vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(binding_description.size());
            vertex_input_info.pVertexBindingDescriptions = binding_description.data();
            vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
            vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

            // 读取并创建着色器模块
            VkShaderModule vert_shader_module = VK_NULL_HANDLE;
            VkShaderModule frag_shader_module = VK_NULL_HANDLE;
            auto vert_shader_code = read_file("vert.spv");
            auto frag_shader_code = read_file("frag.spv");

            if (vert_shader_code.empty() || frag_shader_code.empty()) {
                std::println("着色器文件为空");
                print_stacktrace_and_terminate();
            }

            vert_shader_module = create_shader_module(vert_shader_code, core.device);
            frag_shader_module = create_shader_module(frag_shader_code, core.device);

            VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
            vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
            vert_shader_stage_info.module = vert_shader_module;
            vert_shader_stage_info.pName = "main";

            VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
            frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            frag_shader_stage_info.module = frag_shader_module;
            frag_shader_stage_info.pName = "main";

            VkPipelineShaderStageCreateInfo shaderStages[] = {vert_shader_stage_info, frag_shader_stage_info};

            // 输入装配状态
            VkPipelineInputAssemblyStateCreateInfo input_assembly{};
            input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            input_assembly.primitiveRestartEnable = VK_FALSE;

            // 视口和裁剪
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(core.swap_chain_extent.width);
            viewport.height = static_cast<float>(core.swap_chain_extent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = core.swap_chain_extent;

            VkPipelineViewportStateCreateInfo viewport_state{};
            viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewport_state.viewportCount = 1;
            viewport_state.pViewports = &viewport;
            viewport_state.scissorCount = 1;
            viewport_state.pScissors = &scissor;

            // 光栅化状态
            VkPipelineRasterizationStateCreateInfo rasterizer{};
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE;
            rasterizer.rasterizerDiscardEnable = VK_FALSE;
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizer.lineWidth = 1.0f;
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterizer.depthBiasEnable = VK_FALSE;

            // 多重采样状态
            VkPipelineMultisampleStateCreateInfo multisampling{};
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.sampleShadingEnable = VK_FALSE;
            multisampling.rasterizationSamples = core.msaa_samples;
            multisampling.minSampleShading = 1.0f;
            multisampling.pSampleMask = nullptr;
            multisampling.alphaToCoverageEnable = VK_FALSE;
            multisampling.alphaToOneEnable = VK_FALSE;

            // 颜色混合状态
            VkPipelineColorBlendAttachmentState color_blend_attachment{};
            color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT |
                        VK_COLOR_COMPONENT_A_BIT;
            color_blend_attachment.blendEnable = VK_FALSE;

            VkPipelineColorBlendStateCreateInfo color_blending{};
            color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            color_blending.logicOpEnable = VK_FALSE;
            color_blending.attachmentCount = 1;
            color_blending.pAttachments = &color_blend_attachment;

            // 深度测试
            VkPipelineDepthStencilStateCreateInfo depthStencil{};
            depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.stencilTestEnable = VK_FALSE;

            // 创建管线
            VkGraphicsPipelineCreateInfo pipeline_info = {};
            pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipeline_info.stageCount = 2;
            pipeline_info.pStages = shaderStages;
            pipeline_info.pVertexInputState = &vertex_input_info;
            pipeline_info.pInputAssemblyState = &input_assembly;
            pipeline_info.pViewportState = &viewport_state;
            pipeline_info.pRasterizationState = &rasterizer;
            pipeline_info.pMultisampleState = &multisampling;
            pipeline_info.pColorBlendState = &color_blending;
            pipeline_info.pDepthStencilState = &depthStencil;
            pipeline_info.layout = pipeline_layout;
            pipeline_info.renderPass = core.renderpass;
            pipeline_info.subpass = 0;
            pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

            VkPipeline pipeline;
            if (vkCreateGraphicsPipelines(core.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS) {
                std::println("failed to create graphics pipeline!");
                print_stacktrace_and_terminate();
            }

            // 清理着色器模块
            vkDestroyShaderModule(core.device, vert_shader_module, nullptr);
            vkDestroyShaderModule(core.device, frag_shader_module, nullptr);

            return pipeline;
        }

    vulkan_renderable_object &runtime::add_object(
        const std::vector<vertex> &vertices,
        const std::vector<uint32_t> &indexes,
        const std::vector<unsigned char> &texture_data,
        int width, int height,
        VkFormat format
        ) {
            std::vector<glm::vec3> positions;
            std::vector<glm::vec3> normals;
            std::vector<glm::vec2> tex_coords;

            for (const auto&[position, normal, tex_coord] : vertices) {
                positions.push_back(position);
                normals.push_back(normal);
                tex_coords.push_back(tex_coord);
            }

            vulkan_renderable_object object = {vulkan_mesh_buffer(this->core, positions, normals, tex_coords, indexes)};

            vulkan_texture texture;
            if (texture_data.empty()) {
                const stb_texture stb_texture1 = create_default_texture();
                create_texture_from_stb(stb_texture1);
                texture.texture_image = texture_image;
                texture.texture_image_memory = texture_image_memory;
                texture.texture_image_view = texture_image_view;
                texture.texture_sampler = texture_sampler;
                texture_image = VK_NULL_HANDLE;
                texture_image_memory = VK_NULL_HANDLE;
                texture_image_view = VK_NULL_HANDLE;
                texture_sampler = VK_NULL_HANDLE;
            } else {
                create_vulkan_texture(texture_data, width, height, format);
                texture.texture_image = texture_image;
                texture.texture_image_memory = texture_image_memory;
                texture.texture_image_view = texture_image_view;
                texture.texture_sampler = texture_sampler;
                texture_image = VK_NULL_HANDLE;
                texture_image_memory = VK_NULL_HANDLE;
                texture_image_view = VK_NULL_HANDLE;
                texture_sampler = VK_NULL_HANDLE;
            }

            create_uniform_buffers();
            object.uniform_buffers.uniform_buffers = std::move(this->uniform_buffers);
            object.uniform_buffers.uniform_buffers_mapped = std::move(this->uniform_buffers_mapped);
            object.uniform_buffers.uniform_buffers_memory = std::move(this->uniform_buffers_memory);

            object.texture = texture;
            object.descriptor_set = make_descriptor_sets(object.uniform_buffers.uniform_buffers[0]);
            object.texture.update_to_descriptor_set(core.device,object.descriptor_set);

            this->objects.push_back(std::move(object));

            return *(objects.end() - 1);
        }

    vulkan_renderable_object &runtime::add_object(
        const std::vector<glm::vec3> &positions,
        const std::vector<glm::vec3> &normals,
        const std::vector<glm::vec2> &tex_coords,
        const std::vector<uint32_t> &indexes,
        const std::string_view path
        ){
            vulkan_renderable_object object = {vulkan_mesh_buffer(this->core, positions, normals, tex_coords, indexes)};

            // 创建纹理资源
            vulkan_texture texture;
            if (path.empty()) {
                const stb_texture stb_texture1 = create_default_texture();
                create_texture_from_stb(stb_texture1);
                texture.texture_image = texture_image;
                texture.texture_image_memory = texture_image_memory;
                texture.texture_image_view = texture_image_view;
                texture.texture_sampler = texture_sampler;
                texture_image = VK_NULL_HANDLE;
                texture_image_memory = VK_NULL_HANDLE;
                texture_image_view = VK_NULL_HANDLE;
                texture_sampler = VK_NULL_HANDLE;
                std::cout << "纹理资源创建成功，尺寸: " << stb_texture1.width << "x" << stb_texture1.height << std::endl;
            } else {
                load_png_texture(path.data());
                texture.texture_image = texture_image;
                texture.texture_image_memory = texture_image_memory;
                texture.texture_image_view = texture_image_view;
                texture.texture_sampler = texture_sampler;
                texture_image = VK_NULL_HANDLE;
                texture_image_memory = VK_NULL_HANDLE;
                texture_image_view = VK_NULL_HANDLE;
                texture_sampler = VK_NULL_HANDLE;
            }

            create_uniform_buffers();
            object.uniform_buffers.uniform_buffers = std::move(this->uniform_buffers);
            object.uniform_buffers.uniform_buffers_mapped = std::move(this->uniform_buffers_mapped);
            object.uniform_buffers.uniform_buffers_memory = std::move(this->uniform_buffers_memory);

            object.texture = texture;
            object.descriptor_set = make_descriptor_sets(object.uniform_buffers.uniform_buffers[0]);
            object.texture.update_to_descriptor_set(core.device,object.descriptor_set);

            this->objects.push_back(std::move(object));
            return *(this->objects.end() - 1);
        }

    vulkan_renderable_object &runtime::add_object(
        const std::vector<vertex> &vertices,
        const std::vector<uint32_t> &indexes,
        const std::string_view path
        ) {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> tex_coords;

        for (const auto&[position, normal, tex_coord] : vertices) {
            positions.push_back(position);
            normals.push_back(normal);
            tex_coords.push_back(tex_coord);
        }


        return this->add_object(positions, normals, tex_coords, indexes, "");
    }

    void runtime::render_frame_with_objects() {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(core.window, &width, &height);

        if (width == 0 || height == 0) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(16ms);
            return;
        }


        // 1. 等待前一帧
        core.wait_for_fences();

        // 2. 获取图像
        uint32_t image_index;
        VkResult result;
        core.get_image_index(image_index, result);

        // 处理交换链重建
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            core.recreate_swap_chain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            std::println(stderr,"无法获取交换链图像!");
            print_stacktrace_and_terminate();

        }

        // 3. 等待图像可用
        core.wait_usable_image(image_index);
        core.reset_fences();

        // 4. 更新Uniform Buffer
        //update_uniform_buffer(image_index);

        // 5. 记录命令缓冲区 - 使用mesh渲染
        record_command_buffer_with_objects(image_index);


        // 6. 提交并呈现
        core.submit_cmd_buffer();

        const VkSemaphore signal_semaphores[] = {
            core.render_finished_semaphores[core.current_frame]
        };

        result = core.present_image(signal_semaphores, image_index);

        // 7. 处理交换链重建
        if (result == VK_ERROR_OUT_OF_DATE_KHR ||
            result == VK_SUBOPTIMAL_KHR ||
            core.framebuffer_resized) {
            core.framebuffer_resized = false;
            core.recreate_swap_chain();
            }

        // 8. 前进到下一帧
        core.go_to_next_frame();
    }

    void runtime::update_uniform_buffer(const uint32_t current_image) const {
        uniform_buffer_object ubo{};

        // 1. 模型矩阵：根据您的模型大小调整
        static float angle = 0.0f;
        angle += glm::radians(1.0f);  // 每秒旋转

        ubo.model = glm::mat4(1.0f);
        ubo.model = glm::scale(ubo.model, glm::vec3(2.0f,  2.0f, 2.0f));  // 缩放

        // 修正旋转：将模型的Z轴向上转为Y轴向上
        ubo.model = glm::rotate(ubo.model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 0.1f));

        // 2. 向下平移（注意坐标系方向！）
        //ubo.model = glm::translate(ubo.model, glm::vec3(0.0f, -1.0f, 0.0f));  // 向下移动1个单位

        ubo.model = glm::rotate(ubo.model, angle, glm::vec3(0.0f, 0.0f, 1.0f));  // 持续旋转

        // 2. 视图矩阵：调整相机位置
        ubo.view = glm::lookAt(
            glm::vec3(3.0f, 3.0f, 3.0f),  // 相机位置
            glm::vec3(0.0f, 0.0f, 0.0f),  // 观察目标
            glm::vec3(0.0f, 0.0f, 1.0f)   // 上方向（Z轴向上）
        );

        // 3. 投影矩阵
        ubo.proj = glm::perspective(
            glm::radians(60.0f),  // 视野角度
            static_cast<float>(core.swap_chain_extent.width) /
                        static_cast<float>(core.swap_chain_extent.height),
            0.1f,    // 近平面
            100.0f   // 远平面
        );

        // Vulkan的Y轴是向下的，需要翻转
        ubo.proj[1][1] *= -1;

        // 复制数据
        if (current_image < uniform_buffers_mapped.size()) {
            memcpy(uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
        }
    }

    void runtime::load_png_texture(const std::string &filepath) {
        png_texture_data texture_data = png_texture_loader::load_png_texture(filepath);

        // 创建Vulkan纹理资源
        png_texture_loader::create_vulkan_texture(
            core.device,
            core.physical_device,
            core.command_pool,
            core.graphics_queue,
            texture_data,
            this->texture_image,
            this->texture_image_memory,
            this->texture_image_view,
            this->texture_sampler
        );

        update_descriptor_sets();  // 更新描述符集
        std::cout << "PNG纹理加载成功: " << filepath << std::endl;
    }

    bool runtime::has_geometry() const {
        return static_cast<bool>(vertex_buffer) && index_count > 0;
    }

    void runtime::create_descriptor_sets() {
        if (!core.descriptor_set_layout) {
            return; // 如果没有描述符集布局，跳过
        }

        // 分配描述符集
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = core.descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &core.descriptor_set_layout;

        if (vkAllocateDescriptorSets(core.device, &alloc_info, &descriptor_set) != VK_SUCCESS) {
            std::println(stderr,"无法分配描述符集!");
        }
    }

    VkDescriptorSet runtime::make_descriptor_sets(const VkBuffer &uniform_buffer) const  {
            if (!core.descriptor_set_layout) {
                return VK_NULL_HANDLE;
            }

            // 分配描述符集
            VkDescriptorSet descriptor_set_;
            VkDescriptorSetAllocateInfo alloc_info{};
            alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool = core.descriptor_pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &core.descriptor_set_layout;

            if (vkAllocateDescriptorSets(core.device, &alloc_info, &descriptor_set_) != VK_SUCCESS) {
                std::println("无法分配描述符集!");
                print_stacktrace_and_terminate();
            }

            // 关键：为这个描述符集配置Uniform Buffer！
            std::array<VkWriteDescriptorSet, 2> descriptor_writes{};

            // 1. Uniform Buffer描述符
            VkDescriptorBufferInfo buffer_info{};
            buffer_info.buffer = uniform_buffer;  // 使用第一个uniform buffer
            buffer_info.offset = 0;
            buffer_info.range = sizeof(uniform_buffer_object);

            descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[0].dstSet = descriptor_set_;
            descriptor_writes[0].dstBinding = 0;  // binding 0: UBO
            descriptor_writes[0].dstArrayElement = 0;
            descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor_writes[0].descriptorCount = 1;
            descriptor_writes[0].pBufferInfo = &buffer_info;

            // 2. 纹理描述符（暂时为空，稍后由纹理自己填充）
            descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[1].dstSet = descriptor_set_;
            descriptor_writes[1].dstBinding = 1;  // binding 1: 纹理
            descriptor_writes[1].dstArrayElement = 0;
            descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_writes[1].descriptorCount = 1;
            descriptor_writes[1].pImageInfo = nullptr;  // 稍后由纹理填充

            // 更新描述符集（先只更新UBO，纹理稍后）
            vkUpdateDescriptorSets(core.device, 1, &descriptor_writes[0], 0, nullptr);

            return descriptor_set_;
        }

    stb_texture runtime::create_default_texture()  {
        // 创建一个简单的2x2测试纹理（红、绿、蓝、白）
        int width = 2;
        int height = 2;
        VkDeviceSize image_size = width * height * 4; // RGBA

        auto* pixels = new stbi_uc[16];

        // 红色像素 (255, 0, 0, 255)
        pixels[0] = 255; pixels[1] = 0; pixels[2] = 0; pixels[3] = 255;
        // 绿色像素 (0, 255, 0, 255)
        pixels[4] = 0; pixels[5] = 255; pixels[6] = 0; pixels[7] = 255;
        // 蓝色像素 (0, 0, 255, 255)
        pixels[8] = 0; pixels[9] = 0; pixels[10] = 255; pixels[11] = 255;
        // 白色像素 (255, 255, 255, 255)
        pixels[12] = 255; pixels[13] = 255; pixels[14] = 255; pixels[15] = 255;

        stb_texture texture{};
        texture.width = width;
        texture.height = height;
        texture.image_size = image_size;
        texture.pixels = pixels;

        return texture;
    }

    void runtime::cleanup_texture_resources()  {
        if (texture_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(core.device, texture_sampler, nullptr);
            texture_sampler = VK_NULL_HANDLE;
        }

        if (texture_image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(core.device, texture_image_view, nullptr);
            texture_image_view = VK_NULL_HANDLE;
        }

        if (texture_image_memory != VK_NULL_HANDLE) {
            vkFreeMemory(core.device, texture_image_memory, nullptr);
            texture_image_memory = VK_NULL_HANDLE;
        }

        if (texture_image != VK_NULL_HANDLE) {
            vkDestroyImage(core.device, texture_image, nullptr);
            texture_image = VK_NULL_HANDLE;
        }
    }
}
