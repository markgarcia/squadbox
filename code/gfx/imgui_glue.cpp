#include "imgui_glue.hpp"

#include "render_manager.hpp"
#include "vulkan_manager.hpp"
#include "vulkan_utils.hpp"

#include <imgui.h>
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

// Much of these based on https://github.com/ocornut/imgui/tree/master/examples/vulkan_example

namespace squadbox::gfx {

imgui_glue::imgui_glue(gsl::not_null<GLFWwindow*> window, const vulkan_manager& vulkan_manager, const render_manager& render_manager)
    : m_window(window), m_device(vulkan_manager.device()) {
    m_device_memory_props = vulkan_manager.physical_device().getMemoryProperties();

    static const std::uint32_t vert_shader_spv[] = {
        #include "../shaders/compiled/imgui.vert.spv.c"
    };

    static const std::uint32_t frag_shader_spv[] = {
        #include "../shaders/compiled/imgui.frag.spv.c"
    };

    m_persistent_render_data->vert_shader = [](const vk::Device& device) {
        vk::ShaderModuleCreateInfo vert_shader_ci;
        vert_shader_ci
            .setPCode(vert_shader_spv)
            .setCodeSize(sizeof(vert_shader_spv));

        return device.createShaderModuleUnique(vert_shader_ci);
    }(m_device);

    m_persistent_render_data->frag_shader = [](const vk::Device& device) {
        vk::ShaderModuleCreateInfo frag_shader_ci;
        frag_shader_ci
            .setPCode(frag_shader_spv)
            .setCodeSize(sizeof(frag_shader_spv));

        return device.createShaderModuleUnique(frag_shader_ci);
    }(m_device);

    m_persistent_render_data->font_sampler = [](const vk::Device& device) {
        vk::SamplerCreateInfo sampler_ci;
        sampler_ci
            .setMagFilter(vk::Filter::eLinear)
            .setMinFilter(vk::Filter::eLinear)
            .setMipmapMode(vk::SamplerMipmapMode::eLinear)
            .setMinLod(-1000)
            .setMaxLod(1000)
            .setMaxAnisotropy(1.0f);

        return device.createSamplerUnique(sampler_ci);
    }(m_device);

    m_persistent_render_data->descriptor_set_layout = [](const vk::Device& device, const vk::Sampler& font_sampler) {
        std::array<vk::Sampler, 1> samplers = { font_sampler };
        
        std::array<vk::DescriptorSetLayoutBinding, 1> layout_bindings;
        layout_bindings[0]
            .setBinding(font_sampler_binding_idx)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setStageFlags(vk::ShaderStageFlagBits::eFragment)
            .setPImmutableSamplers(samplers.data())
            .setDescriptorCount(samplers.size());

        vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_ci;
        descriptor_set_layout_ci
            .setPBindings(layout_bindings.data())
            .setBindingCount(layout_bindings.size());

        return device.createDescriptorSetLayoutUnique(descriptor_set_layout_ci);
    }(m_device, m_persistent_render_data->font_sampler.get());

    m_persistent_render_data->descriptor_pool = [](const vk::Device& device) {
        vk::DescriptorPoolSize descriptor_pool_size;
        descriptor_pool_size
            .setType(vk::DescriptorType::eCombinedImageSampler)
            .setDescriptorCount(1);

        vk::DescriptorPoolCreateInfo descriptor_pool_ci;
        descriptor_pool_ci
            .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
            .setPPoolSizes(&descriptor_pool_size)
            .setPoolSizeCount(1)
            .setMaxSets(1);

        return device.createDescriptorPoolUnique(descriptor_pool_ci);
    }(m_device);

    m_persistent_render_data->descriptor_set = [](const vk::Device& device, const vk::DescriptorSetLayout& layout,
                                                  const vk::DescriptorPool& pool) {
        vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
        descriptor_set_alloc_info
            .setDescriptorPool(pool)
            .setPSetLayouts(&layout)
            .setDescriptorSetCount(1);

        return std::move(device.allocateDescriptorSetsUnique(descriptor_set_alloc_info)[0]);
    }(m_device, m_persistent_render_data->descriptor_set_layout.get(), m_persistent_render_data->descriptor_pool.get());

    m_persistent_render_data->pipeline_layout = [](const vk::Device& device, const vk::DescriptorSetLayout& descriptor_set_layout) {
        /*
        shaders/imgui.vert:
        layout(push_constant) uniform uPushConstant {
            vec2 uScale;
            vec2 uTranslate;
        } pc;
        */

        vk::PushConstantRange push_constant_range;
        push_constant_range
            .setStageFlags(vk::ShaderStageFlagBits::eVertex)
            .setOffset(0)
            .setSize(sizeof(float) * 2 * 2);

        std::array<vk::DescriptorSetLayout, 1> layouts = { descriptor_set_layout };

        vk::PipelineLayoutCreateInfo pipeline_layout_ci;
        pipeline_layout_ci
            .setPSetLayouts(layouts.data())
            .setSetLayoutCount(layouts.size())
            .setPPushConstantRanges(&push_constant_range)
            .setPushConstantRangeCount(1);

        return device.createPipelineLayoutUnique(pipeline_layout_ci);
    }(m_device, m_persistent_render_data->descriptor_set_layout.get());

    m_persistent_render_data->graphics_pipeline = [](const vk::Device& device, const vk::RenderPass& render_pass, const vk::PipelineLayout& pipeline_layout,
                                                     const vk::ShaderModule& vertex_shader_module, const vk::ShaderModule& fragment_shader_module) {
        vk::GraphicsPipelineCreateInfo graphics_pipeline_ci;
        std::vector<vk::DynamicState> enabled_dynamic_states;

        std::array<vk::PipelineShaderStageCreateInfo, 2> stages;
        stages[0]
            .setStage(vk::ShaderStageFlagBits::eVertex)
            .setModule(vertex_shader_module)
            .setPName("main");

        stages[1]
            .setStage(vk::ShaderStageFlagBits::eFragment)
            .setModule(fragment_shader_module)
            .setPName("main");
        
        graphics_pipeline_ci
            .setPStages(stages.data())
            .setStageCount(stages.size());

        vk::VertexInputBindingDescription vert_input_binding_desc;
        vert_input_binding_desc
            .setBinding(0)
            .setStride(sizeof(ImDrawVert))
            .setInputRate(vk::VertexInputRate::eVertex);

        std::array<vk::VertexInputAttributeDescription, 3> vert_input_attr_desc;
        vert_input_attr_desc[0]
            .setLocation(0)
            .setBinding(vert_input_binding_desc.binding)
            .setFormat(vk::Format::eR32G32Sfloat)
            .setOffset(offsetof(ImDrawVert, pos));
        vert_input_attr_desc[1]
            .setLocation(1)
            .setBinding(vert_input_binding_desc.binding)
            .setFormat(vk::Format::eR32G32Sfloat)
            .setOffset(offsetof(ImDrawVert, uv));
        vert_input_attr_desc[2]
            .setLocation(2)
            .setBinding(vert_input_binding_desc.binding)
            .setFormat(vk::Format::eR8G8B8A8Unorm)
            .setOffset(offsetof(ImDrawVert, col));

        vk::PipelineVertexInputStateCreateInfo pipeline_vert_input_state_ci;
        pipeline_vert_input_state_ci
            .setPVertexBindingDescriptions(&vert_input_binding_desc)
            .setVertexBindingDescriptionCount(1)
            .setPVertexAttributeDescriptions(vert_input_attr_desc.data())
            .setVertexAttributeDescriptionCount(vert_input_attr_desc.size());
        graphics_pipeline_ci.setPVertexInputState(&pipeline_vert_input_state_ci);

        vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_ci;
        pipeline_input_assembly_state_ci
            .setTopology(vk::PrimitiveTopology::eTriangleList);
        graphics_pipeline_ci.setPInputAssemblyState(&pipeline_input_assembly_state_ci);

        vk::PipelineRasterizationStateCreateInfo pipeline_raster_state_ci;
        pipeline_raster_state_ci
            .setPolygonMode(vk::PolygonMode::eFill)
            .setCullMode(vk::CullModeFlagBits::eNone)
            .setFrontFace(vk::FrontFace::eClockwise)
            .setLineWidth(1.0f);
        graphics_pipeline_ci.setPRasterizationState(&pipeline_raster_state_ci);

        vk::PipelineColorBlendAttachmentState color_blend_attachment;
        color_blend_attachment
            .setBlendEnable(true)
            .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
            .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
            .setColorBlendOp(vk::BlendOp::eAdd)
            .setSrcAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
            .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
            .setAlphaBlendOp(vk::BlendOp::eAdd)
            .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

        vk::PipelineColorBlendStateCreateInfo pipeline_color_blend_state_ci;
        pipeline_color_blend_state_ci
            .setPAttachments(&color_blend_attachment)
            .setAttachmentCount(1);
        graphics_pipeline_ci.setPColorBlendState(&pipeline_color_blend_state_ci);

        vk::PipelineViewportStateCreateInfo pipeline_viewport_state_ci;
        pipeline_viewport_state_ci
            .setViewportCount(1)
            .setScissorCount(1);
        graphics_pipeline_ci.setPViewportState(&pipeline_viewport_state_ci);

        enabled_dynamic_states.emplace_back(vk::DynamicState::eViewport);
        enabled_dynamic_states.emplace_back(vk::DynamicState::eScissor);

        vk::PipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_ci;
        pipeline_depth_stencil_state_ci
            .setDepthTestEnable(false)
            .setDepthWriteEnable(false);
        graphics_pipeline_ci.setPDepthStencilState(&pipeline_depth_stencil_state_ci);

        vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_ci;
        pipeline_multisample_state_ci
            .setRasterizationSamples(vk::SampleCountFlagBits::e1);
        graphics_pipeline_ci.setPMultisampleState(&pipeline_multisample_state_ci);

        vk::PipelineDynamicStateCreateInfo pipeline_dynamic_state_ci;
        pipeline_dynamic_state_ci
            .setPDynamicStates(enabled_dynamic_states.data())
            .setDynamicStateCount(enabled_dynamic_states.size());
        graphics_pipeline_ci.setPDynamicState(&pipeline_dynamic_state_ci);

        graphics_pipeline_ci
            .setRenderPass(render_pass)
            .setLayout(pipeline_layout);

        return device.createGraphicsPipelineUnique(nullptr, graphics_pipeline_ci);
    }(m_device, render_manager.render_pass(), m_persistent_render_data->pipeline_layout.get(),
      m_persistent_render_data->vert_shader.get(), m_persistent_render_data->frag_shader.get());

    m_persistent_render_data->command_pool = [](const vk::Device& device, std::uint32_t graphics_queue_family_idx) {
        vk::CommandPoolCreateInfo command_pool_ci;
        command_pool_ci
            .setQueueFamilyIndex(graphics_queue_family_idx);

        return device.createCommandPoolUnique(command_pool_ci);
    }(m_device, vulkan_manager.graphics_queue_family_index());

    {
        ImGuiIO& io = ImGui::GetIO();
        io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
        io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
        io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
        io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
        io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
        io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
        io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
        io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
        io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
        io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
        io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
        io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
        io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
        io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
        io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
        io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
        io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
        io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

        io.RenderDrawListsFn = nullptr;

        io.GetClipboardTextFn = [](void* user_data) {
            return glfwGetClipboardString(static_cast<GLFWwindow*>(user_data));
        };

        io.SetClipboardTextFn = [](void* user_data, const char* text) {
            glfwSetClipboardString(static_cast<GLFWwindow*>(user_data), text);
        };

        io.ClipboardUserData = window.get();

        #ifdef _WIN32
        io.ImeWindowHandle = glfwGetWin32Window(window);
        #endif
    }
}

imgui_glue::~imgui_glue() {
    ImGui::Shutdown();
}

/*std::tuple<vk::UniqueCommandBuffer, vk::UniqueBuffer, vk::UniqueDeviceMemory>*/
render_job imgui_glue::load_font_textures() {
    ImGuiIO& imgui_io = ImGui::GetIO();

    gsl::span<unsigned char> font_image_pixels;
    std::uint32_t font_image_width, font_image_height;

    {
        unsigned char* temp_font_image_pixels;
        int temp_width, temp_height;
        imgui_io.Fonts->GetTexDataAsRGBA32(&temp_font_image_pixels, &temp_width, &temp_height);

        font_image_width = static_cast<std::uint32_t>(temp_width);
        font_image_height = static_cast<std::uint32_t>(temp_height);
        font_image_pixels = { temp_font_image_pixels, static_cast<decltype(font_image_pixels)::size_type>(font_image_width * font_image_height * 4) };
    }

    auto [new_font_image, new_font_image_memory]
        = [](const vk::Device& device, const vk::PhysicalDeviceMemoryProperties& device_memory_props,
             std::uint32_t font_image_width, std::uint32_t font_image_height) {
        vk::ImageCreateInfo image_ci;
        image_ci
            .setImageType(vk::ImageType::e2D)
            .setFormat(vk::Format::eR8G8B8A8Unorm)
            .setExtent({ static_cast<std::uint32_t>(font_image_width), static_cast<std::uint32_t>(font_image_height), 1 })
            .setMipLevels(1)
            .setArrayLayers(1)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(vk::ImageTiling::eOptimal)
            .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst)
            .setSharingMode(vk::SharingMode::eExclusive)
            .setInitialLayout(vk::ImageLayout::eUndefined);

        auto font_image = device.createImageUnique(image_ci);

        auto memory_reqs = device.getImageMemoryRequirements(font_image.get());
        auto font_image_memory = vk_utils::alloc_memory(device, device_memory_props, memory_reqs, vk::MemoryPropertyFlagBits::eDeviceLocal);
        device.bindImageMemory(font_image.get(), font_image_memory.get(), 0);

        return std::make_tuple(std::move(font_image), std::move(font_image_memory));
    }(m_device, m_device_memory_props, font_image_width, font_image_height);

    auto new_font_image_view = [](const vk::Device& device, const vk::Image& font_image) {
        vk::ImageViewCreateInfo image_view_ci;
        image_view_ci
            .setImage(font_image)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(vk::Format::eR8G8B8A8Unorm)
            .subresourceRange
            .setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setLevelCount(1)
            .setLayerCount(1);

        return device.createImageViewUnique(image_view_ci);
    }(m_device, new_font_image.get());

    [](const vk::Device& device, const vk::Sampler& font_sampler, const vk::DescriptorSet& descriptor_set,
       const vk::ImageView& font_image_view) {
        vk::DescriptorImageInfo descriptor_image_info;
        descriptor_image_info
            .setSampler(font_sampler)
            .setImageView(font_image_view)
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        std::array<vk::WriteDescriptorSet, 1> descriptor_writes;
        descriptor_writes[0]
            .setDstSet(descriptor_set)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setPImageInfo(&descriptor_image_info)
            .setDescriptorCount(1);

        device.updateDescriptorSets(descriptor_writes, nullptr);
    }(m_device, m_persistent_render_data->font_sampler.get(), m_persistent_render_data->descriptor_set.get(), new_font_image_view.get());

    auto [font_staging_buffer, font_staging_buffer_memory]
        = [](const vk::Device& device, const vk::PhysicalDeviceMemoryProperties& device_memory_props,
            std::size_t font_staging_buffer_size) {
        vk::BufferCreateInfo buffer_ci;
        buffer_ci
            .setSize(font_staging_buffer_size)
            .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
            .setSharingMode(vk::SharingMode::eExclusive);

        auto font_staging_buffer = device.createBufferUnique(buffer_ci);

        auto memory_reqs = device.getBufferMemoryRequirements(font_staging_buffer.get());
        auto font_staging_buffer_memory = vk_utils::alloc_memory(device, device_memory_props, memory_reqs, vk::MemoryPropertyFlagBits::eHostVisible);
        device.bindBufferMemory(font_staging_buffer.get(), font_staging_buffer_memory.get(), 0);

        return std::make_tuple(std::move(font_staging_buffer), std::move(font_staging_buffer_memory));
    }(m_device, m_device_memory_props, font_image_pixels.size());

    [](const vk::Device& device, const vk::DeviceMemory& font_staging_buffer_memory,
       gsl::span<unsigned char> font_image_pixels) {
        auto mapped_memory_location = device.mapMemory(font_staging_buffer_memory, 0, font_image_pixels.size());
        std::copy(font_image_pixels.begin(), font_image_pixels.end(), static_cast<unsigned char*>(mapped_memory_location));

        vk::MappedMemoryRange mapped_memory_range;
        mapped_memory_range
            .setMemory(font_staging_buffer_memory)
            .setSize(font_image_pixels.size());

        device.flushMappedMemoryRanges({ mapped_memory_range });
        device.unmapMemory(font_staging_buffer_memory);
    }(m_device, font_staging_buffer_memory.get(), font_image_pixels);

    auto font_image_copy_command_buffer = [](const vk::Device& device, const vk::CommandPool& command_pool,
       const vk::Buffer& font_staging_buffer, const vk::Image& font_image,
       std::uint32_t font_image_width, std::uint32_t font_image_height) {
        vk::CommandBufferAllocateInfo command_buffer_alloc_info;
        command_buffer_alloc_info
            .setCommandPool(command_pool)
            .setLevel(vk::CommandBufferLevel::ePrimary)
            .setCommandBufferCount(1);

        auto command_buffer = std::move(device.allocateCommandBuffersUnique(command_buffer_alloc_info)[0]);
        command_buffer->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

        {
            vk::ImageMemoryBarrier image_copy_barrier;
            image_copy_barrier
                .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(font_image)
                .subresourceRange
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setLevelCount(1)
                .setLayerCount(1);

            command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits(),
                nullptr, nullptr, { image_copy_barrier });
        }

        {
            vk::BufferImageCopy copy_region;
            copy_region
                .imageSubresource
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setLayerCount(1);
            copy_region
                .imageExtent
                .setWidth(font_image_width)
                .setHeight(font_image_height)
                .setDepth(1);

            command_buffer->copyBufferToImage(font_staging_buffer, font_image, vk::ImageLayout::eTransferDstOptimal, { copy_region });
        }

        {
            vk::ImageMemoryBarrier image_use_barrier;
            image_use_barrier
                .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(font_image)
                .subresourceRange
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setLevelCount(1)
                .setLayerCount(1);

            command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits(),
                nullptr, nullptr, { image_use_barrier });
        }

        command_buffer->end();

        return command_buffer;
    }(m_device, m_persistent_render_data->command_pool.get(), font_staging_buffer.get(), new_font_image.get(), font_image_width, font_image_height);

    m_persistent_render_data->font_image = std::move(new_font_image);
    m_persistent_render_data->font_image_memory = std::move(new_font_image_memory);
    m_persistent_render_data->font_image_view = std::move(new_font_image_view);
    imgui_io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(static_cast<VkImage>(m_persistent_render_data->font_image.get())));

    return render_job { std::move(font_image_copy_command_buffer), std::make_tuple(std::move(font_staging_buffer), std::move(font_staging_buffer_memory)), m_persistent_render_data };
}

void imgui_glue::new_frame(std::chrono::duration<double> delta) {
    ImGuiIO& io = ImGui::GetIO();

    int w, h;
    int display_w, display_h;
    glfwGetWindowSize(m_window, &w, &h);
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

    io.DeltaTime = static_cast<float>(delta.count());

    if (glfwGetWindowAttrib(m_window, GLFW_FOCUSED))
    {
        double mouse_x, mouse_y;
        glfwGetCursorPos(m_window, &mouse_x, &mouse_y);
        io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
    }
    else
    {
        io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
    }

    for (int i = 0; i < 3; i++) {
        io.MouseDown[i] = m_pressed_mouse_buttons[i] || glfwGetMouseButton(m_window, i) != 0;
        m_pressed_mouse_buttons[i] = false;
    }

    io.MouseWheel = static_cast<float>(m_mouse_wheel_pos);
    m_mouse_wheel_pos = 0.0f;

    glfwSetInputMode(m_window, GLFW_CURSOR, io.MouseDrawCursor ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);

    ImGui::NewFrame();
}

render_job imgui_glue::render(const vk::CommandBufferInheritanceInfo& command_buffer_inheritance_info) {
    ImGui::Render();

    const auto& imgui_draw_data = *ImGui::GetDrawData();
    const auto required_vertex_buffer_size = std::max(1, imgui_draw_data.TotalVtxCount) * sizeof(ImDrawVert);
    const auto required_index_buffer_size = std::max(1, imgui_draw_data.TotalIdxCount) * sizeof(ImDrawIdx);

    vk::CommandBufferAllocateInfo command_buffer_alloc_info;
    command_buffer_alloc_info
        .setCommandPool(m_persistent_render_data->command_pool.get())
        .setLevel(vk::CommandBufferLevel::eSecondary)
        .setCommandBufferCount(1);

    auto render_job = m_render_job_pool.create(m_device, command_buffer_alloc_info, m_persistent_render_data);

    if (!render_job->vertex_buffer || !render_job->index_buffer
        || render_job->vertex_buffer_size < required_vertex_buffer_size
        || render_job->index_buffer_size < required_index_buffer_size) {
        auto new_vertex_buffer = [](const vk::Device& device, const vk::DeviceSize required_vertex_buffer_size) {
            vk::BufferCreateInfo vertex_buffer_ci;
            vertex_buffer_ci
                .setSize(required_vertex_buffer_size)
                .setUsage(vk::BufferUsageFlagBits::eVertexBuffer)
                .setSharingMode(vk::SharingMode::eExclusive);

            return device.createBufferUnique(vertex_buffer_ci);
        }(m_device, required_vertex_buffer_size);

        auto new_index_buffer = [](const vk::Device& device, const vk::DeviceSize required_index_buffer_size) {
            vk::BufferCreateInfo index_buffer_ci;
            index_buffer_ci
                .setSize(required_index_buffer_size)
                .setUsage(vk::BufferUsageFlagBits::eIndexBuffer)
                .setSharingMode(vk::SharingMode::eExclusive);

            return device.createBufferUnique(index_buffer_ci);
        }(m_device, required_index_buffer_size);

        auto[ new_vertex_index_buffers_memory, new_vertex_index_buffers_memory_size,
              new_vertex_buffer_offset, new_index_buffer_offset ]
            = [](const vk::Device& device, const vk::PhysicalDeviceMemoryProperties& device_memory_props,
                 const vk::Buffer& new_vertex_buffer, const vk::Buffer& new_index_buffer,
                 const vk::DeviceSize current_vertex_index_buffers_memory_size) {
            const auto vertex_buffer_memory_reqs = device.getBufferMemoryRequirements(new_vertex_buffer);
            const auto index_buffer_memory_reqs = device.getBufferMemoryRequirements(new_index_buffer);

            const vk::DeviceSize vertex_buffer_offset = 0;
            const vk::DeviceSize index_buffer_offset =
                vertex_buffer_memory_reqs.size
                + index_buffer_memory_reqs.alignment
                - (vertex_buffer_memory_reqs.size % index_buffer_memory_reqs.alignment);

            const auto required_device_memory_size = vertex_buffer_offset + index_buffer_offset + index_buffer_memory_reqs.size;

            if (current_vertex_index_buffers_memory_size < required_device_memory_size) {
                const auto vertex_buffer_memory_type_index = vk_utils::get_memory_type_index(device_memory_props, vertex_buffer_memory_reqs, vk::MemoryPropertyFlagBits::eHostVisible);
                const auto index_buffer_memory_type_index = vk_utils::get_memory_type_index(device_memory_props, index_buffer_memory_reqs, vk::MemoryPropertyFlagBits::eHostVisible);

                assert(vertex_buffer_memory_type_index == index_buffer_memory_type_index);

                vk::MemoryAllocateInfo memory_alloc_info;
                memory_alloc_info
                    .setAllocationSize(required_device_memory_size)
                    .setMemoryTypeIndex(vertex_buffer_memory_type_index);

                return std::make_tuple(device.allocateMemoryUnique(memory_alloc_info), required_device_memory_size,
                                       vertex_buffer_offset, index_buffer_offset);
            }
            else {
                return std::make_tuple(vk::UniqueDeviceMemory { nullptr }, required_device_memory_size,
                                       vertex_buffer_offset, index_buffer_offset);
            }
        }(m_device, m_device_memory_props, new_vertex_buffer.get(), new_index_buffer.get(), render_job->vertex_index_buffers_memory_size);

        render_job->vertex_buffer = std::move(new_vertex_buffer);
        render_job->vertex_buffer_size = required_vertex_buffer_size;
        render_job->vertex_buffer_memory_offset = new_vertex_buffer_offset;

        render_job->index_buffer = std::move(new_index_buffer);
        render_job->index_buffer_size = required_index_buffer_size;
        render_job->index_buffer_memory_offset = new_index_buffer_offset;

        if (new_vertex_index_buffers_memory) {
            render_job->vertex_index_buffers_memory = std::move(new_vertex_index_buffers_memory);
            render_job->vertex_index_buffers_memory_size = new_vertex_index_buffers_memory_size;
        }

        m_device.bindBufferMemory(render_job->vertex_buffer.get(), render_job->vertex_index_buffers_memory.get(), render_job->vertex_buffer_memory_offset);
        m_device.bindBufferMemory(render_job->index_buffer.get(), render_job->vertex_index_buffers_memory.get(), render_job->index_buffer_memory_offset);
    }

    [](const vk::Device& device, const vk::DeviceMemory& vertex_index_buffers_memory,
       const vk::DeviceSize vertex_buffer_size, const vk::DeviceSize vertex_buffer_offset,
       const ImDrawData& imgui_draw_data) {
        ImDrawVert* vertex_dst = static_cast<ImDrawVert*>(device.mapMemory(vertex_index_buffers_memory, vertex_buffer_offset, vertex_buffer_size));

        for (const auto& cmd_list : gsl::make_span(imgui_draw_data.CmdLists, imgui_draw_data.CmdListsCount)) {
            std::copy_n(cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.size(), vertex_dst);
            vertex_dst += cmd_list->VtxBuffer.size();
        }

        vk::MappedMemoryRange vertex_mapped_memory;
        vertex_mapped_memory
            .setMemory(vertex_index_buffers_memory)
            .setOffset(vertex_buffer_offset)
            .setSize(vertex_buffer_size);

        device.flushMappedMemoryRanges({ vertex_mapped_memory });
        device.unmapMemory(vertex_index_buffers_memory);
    }(m_device, render_job->vertex_index_buffers_memory.get(),
      render_job->vertex_buffer_size, render_job->vertex_buffer_memory_offset,
      imgui_draw_data);

    [](const vk::Device& device, const vk::DeviceMemory& vertex_index_buffers_memory,
       const vk::DeviceSize index_buffer_size, const vk::DeviceSize index_buffer_offset,
       const ImDrawData& imgui_draw_data) {
        ImDrawIdx* index_dst = static_cast<ImDrawIdx*>(device.mapMemory(vertex_index_buffers_memory, index_buffer_offset, index_buffer_size));

        for (const auto& cmd_list : gsl::make_span(imgui_draw_data.CmdLists, imgui_draw_data.CmdListsCount)) {
            std::copy_n(cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.size(), index_dst);
            index_dst += cmd_list->IdxBuffer.size();
        }

        vk::MappedMemoryRange index_mapped_memory;
        index_mapped_memory
            .setMemory(vertex_index_buffers_memory)
            .setOffset(index_buffer_offset)
            .setSize(index_buffer_size);

        device.flushMappedMemoryRanges({ index_mapped_memory });
        device.unmapMemory(vertex_index_buffers_memory);
    }(m_device, render_job->vertex_index_buffers_memory.get(),
      render_job->index_buffer_size, render_job->index_buffer_memory_offset,
      imgui_draw_data);

    const auto& command_buffer = render_job.command_buffer();

    vk::CommandBufferBeginInfo command_buffer_begin_info;
    command_buffer_begin_info
        .setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue)
        .setPInheritanceInfo(&command_buffer_inheritance_info);

    command_buffer.begin(command_buffer_begin_info);

    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_persistent_render_data->graphics_pipeline.get());
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_persistent_render_data->pipeline_layout.get(), 0, { m_persistent_render_data->descriptor_set.get() }, nullptr);
    command_buffer.bindVertexBuffers(0, { render_job->vertex_buffer.get() }, { 0 });
    command_buffer.bindIndexBuffer(render_job->index_buffer.get(), 0, vk::IndexType::eUint16);

    {
        vk::Viewport viewport;
        viewport
            .setX(0)
            .setY(0)
            .setWidth(ImGui::GetIO().DisplaySize.x)
            .setHeight(ImGui::GetIO().DisplaySize.y)
            .setMinDepth(0.0f)
            .setMaxDepth(1.0f);

        command_buffer.setViewport(0, { viewport });
    }

    {
        std::array<float, 2> scale;
        scale[0] = 2.0f / ImGui::GetIO().DisplaySize.x;
        scale[1] = 2.0f / ImGui::GetIO().DisplaySize.y;
        command_buffer.pushConstants<float>(m_persistent_render_data->pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex, 0, scale);

        std::array<float, 2> translate;
        translate[0] = -1.0f;
        translate[1] = -1.0f;
        command_buffer.pushConstants<float>(m_persistent_render_data->pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex, sizeof(scale), translate);
    }

    {
        std::uint32_t index_offset = 0;
        std::int32_t vertex_offset = 0;
        for (const auto& cmd_list : gsl::make_span(imgui_draw_data.CmdLists, imgui_draw_data.CmdListsCount)) {
            for (const auto& cmd : cmd_list->CmdBuffer) {
                if (cmd.UserCallback)
                {
                    cmd.UserCallback(cmd_list, &cmd);
                }
                else {
                    vk::Rect2D scissor;
                    scissor.offset
                        .setX(std::max(0, static_cast<std::int32_t>(cmd.ClipRect.x)))
                        .setY(std::max(0, static_cast<std::int32_t>(cmd.ClipRect.y)));
                    scissor.extent
                        .setWidth(static_cast<std::uint32_t>(cmd.ClipRect.z - cmd.ClipRect.x))
                        .setHeight(static_cast<std::uint32_t>(cmd.ClipRect.w - cmd.ClipRect.y + 1));

                    command_buffer.setScissor(0, { scissor });
                    command_buffer.drawIndexed(cmd.ElemCount, 1, index_offset, vertex_offset, 0);
                }

                index_offset += cmd.ElemCount;
            }

            vertex_offset += cmd_list->VtxBuffer.size();
        }
    }

    command_buffer.end();

    return render_job;
}

void imgui_glue::key_event(const key_info& key_info) {
    ImGuiIO& io = ImGui::GetIO();
    if (key_info.action == GLFW_PRESS)
        io.KeysDown[key_info.key] = true;
    if (key_info.action == GLFW_RELEASE)
        io.KeysDown[key_info.key] = false;

    io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
    io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
}

void imgui_glue::mouse_button_event(const mouse_button_info& mouse_button_info) {
    if (mouse_button_info.action == GLFW_PRESS && mouse_button_info.button >= 0 && mouse_button_info.button < 3) {
        m_pressed_mouse_buttons[mouse_button_info.button] = true;
    }
}

void imgui_glue::scroll_event(double y_offset) {
    m_mouse_wheel_pos += y_offset;
}

void imgui_glue::char_event(unsigned int c) {
    ImGuiIO& io = ImGui::GetIO();
    if (c > 0 && c < 0x10000) io.AddInputCharacter(static_cast<unsigned short>(c));
}

}