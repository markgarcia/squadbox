#include "flat_shading.hpp"

#include "../vulkan_manager.hpp"
#include "../render_manager.hpp"
#include "../camera.hpp"
#include "../vulkan_utils.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace squadbox::gfx::render_techniques {

flat_shading::flat_shading(const vulkan_manager& vulkan_manager, const vk::RenderPass& render_pass)
    : m_vulkan_manager(&vulkan_manager) {
    static const std::uint32_t vert_shader_spv[] = {
        #include "../../shaders/compiled/flat.vert.spv.c"
    };

    static const std::uint32_t frag_shader_spv[] = {
        #include "../../shaders/compiled/flat.frag.spv.c"
    };

    m_persistent_render_data->vert_shader = [](const vk::Device& device) {
        vk::ShaderModuleCreateInfo vert_shader_ci;
        vert_shader_ci
            .setPCode(vert_shader_spv)
            .setCodeSize(sizeof(vert_shader_spv));

        return device.createShaderModuleUnique(vert_shader_ci);
    }(m_vulkan_manager->device());

    m_persistent_render_data->frag_shader = [](const vk::Device& device) {
        vk::ShaderModuleCreateInfo frag_shader_ci;
        frag_shader_ci
            .setPCode(frag_shader_spv)
            .setCodeSize(sizeof(frag_shader_spv));

        return device.createShaderModuleUnique(frag_shader_ci);
    }(m_vulkan_manager->device());

    m_persistent_render_data->descriptor_set_layout = [](const vk::Device& device) {       
        std::array<vk::DescriptorSetLayoutBinding, 1> layout_bindings;
        layout_bindings[0]
            .setBinding(vertex_ubo_binding_idx)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setStageFlags(vk::ShaderStageFlagBits::eVertex)
            .setDescriptorCount(1);

        vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_ci;
        descriptor_set_layout_ci
            .setPBindings(layout_bindings.data())
            .setBindingCount(layout_bindings.size());

        return device.createDescriptorSetLayoutUnique(descriptor_set_layout_ci);
    }(m_vulkan_manager->device());

    m_persistent_render_data->descriptor_pool = [](const vk::Device& device) {
        vk::DescriptorPoolSize descriptor_pool_size;
        descriptor_pool_size
            .setType(vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(1000);  //  TODO: Make descriptor pool allocator abstraction

        vk::DescriptorPoolCreateInfo descriptor_pool_ci;
        descriptor_pool_ci
            .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
            .setPPoolSizes(&descriptor_pool_size)
            .setPoolSizeCount(1)
            .setMaxSets(1);

        return device.createDescriptorPoolUnique(descriptor_pool_ci);
    }(m_vulkan_manager->device());

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
    }(m_vulkan_manager->device(), m_persistent_render_data->descriptor_set_layout.get());

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

        auto vert_input_binding_desc = mesh_type::vertex_input_binding_desc();
        auto vert_input_attr_desc = mesh_type::vertex_input_attr_desc();
        
        /*std::array<vk::VertexInputAttributeDescription, 3> vert_input_attr_desc;
        vert_input_attr_desc[0]
            .setLocation(0)
            .setBinding(vert_input_binding_desc.binding)
            .setFormat(vk::Format::eR32G32B32Sfloat)
            .setOffset(mesh_type::vertex_t::offset_of<mesh_features::position>());
        vert_input_attr_desc[1]
            .setLocation(1)
            .setBinding(vert_input_binding_desc.binding)
            .setFormat(vk::Format::eR32G32B32Sfloat)
            .setOffset(mesh_type::vertex_t::offset_of<mesh_features::normal>());*/

        vk::PipelineVertexInputStateCreateInfo pipeline_vert_input_state_ci;
        pipeline_vert_input_state_ci
            .setPVertexBindingDescriptions(vert_input_binding_desc.data())
            .setVertexBindingDescriptionCount(vert_input_binding_desc.size())
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
            .setViewportCount(1);
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
    }(m_vulkan_manager->device(), render_pass, m_persistent_render_data->pipeline_layout.get(),
      m_persistent_render_data->vert_shader.get(), m_persistent_render_data->frag_shader.get());
}

flat_shading::render_data flat_shading::prepare_render_data(mesh_type&& mesh) const {
    render_data_t render_data;

    render_data.mesh = std::move(mesh);

    {
        std::lock_guard<std::mutex> lock(*m_persistent_render_data->descriptor_pool_mutex);

        render_data.descriptor_set = [](const vk::Device& device, const vk::DescriptorSetLayout& layout,
            const vk::DescriptorPool& pool) {
            vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
            descriptor_set_alloc_info
                .setDescriptorPool(pool)
                .setPSetLayouts(&layout)
                .setDescriptorSetCount(1);

            return std::move(device.allocateDescriptorSetsUnique(descriptor_set_alloc_info)[0]);
        }(m_vulkan_manager->device(), m_persistent_render_data->descriptor_set_layout.get(), m_persistent_render_data->descriptor_pool.get());
    }

    std::tie(render_data.uniform_buffer, render_data.uniform_buffer_memory) = [](const vk::Device& device, const vk::PhysicalDeviceMemoryProperties& device_memory_props) {
        vk::BufferCreateInfo buffer_ci;
        buffer_ci
            .setSize(sizeof(ubo_t))
            .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
            .setSharingMode(vk::SharingMode::eExclusive);

        auto uniform_buffer = device.createBufferUnique(buffer_ci);
        auto uniform_buffer_memory_reqs = device.getBufferMemoryRequirements(uniform_buffer.get());
        auto uniform_buffer_memory = vk_utils::alloc_memory(device, device_memory_props, uniform_buffer_memory_reqs, vk::MemoryPropertyFlagBits::eHostVisible);

        return std::make_tuple(std::move(uniform_buffer), std::move(uniform_buffer_memory));
    }(m_vulkan_manager->device(), m_vulkan_manager->physical_device().getMemoryProperties());

    return std::make_shared<render_data_t>(std::move(render_data));
}

void flat_shading::render(render_thread& render_thread,
                          gsl::not_null<render_data> render_data,
                          const vk::Viewport& viewport, const camera& camera, const glm::mat4& model_matrix,
                          const glm::vec4& model_color, const glm::vec4& ambient_color) const {
    const auto& device = m_vulkan_manager->device();

    render_data->command_buffer = render_thread.allocate_command_buffer();
    const auto& command_buffer = render_data->command_buffer.get();

    vk::CommandBufferBeginInfo command_buffer_begin_info;
    command_buffer_begin_info
        .setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue)
        .setPInheritanceInfo(&render_thread.command_buffer_inheritance_info());

    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_persistent_render_data->graphics_pipeline.get());
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_persistent_render_data->pipeline_layout.get(), 0, { render_data->descriptor_set.get() }, nullptr);
    command_buffer.bindVertexBuffers(0, render_data->mesh.vertex_buffers(), render_data->mesh.vertex_buffer_offsets());
    command_buffer.bindIndexBuffer(render_data->mesh.index_buffer(), 0, vk::IndexType::eUint32);
    command_buffer.setViewport(0, { viewport });

    {
        ubo_t ubo;
        ubo.model_view = camera.view_matrix() * model_matrix;
        ubo.projection = camera.projection_matrix();
        ubo.model_color = model_color;
        ubo.ambient_color = ambient_color;

        vk_utils::copy_to_memory(device, render_data->uniform_buffer_memory.get(), ubo);
    }

    command_buffer.drawIndexed(render_data->mesh.index_count(), 1, 0, 0, 0);

    command_buffer.end();

    render_thread.add_render_job({ std::move(render_data.get()) });
}

}