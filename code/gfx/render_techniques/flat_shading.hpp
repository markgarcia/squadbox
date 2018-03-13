#ifndef SQUADBOX_GFX_RENDER_TECHNIQUES_FLAT_SHADING_HPP
#define SQUADBOX_GFX_RENDER_TECHNIQUES_FLAT_SHADING_HPP

#pragma once

#include "../gpu_mesh.hpp"
#include "../render_job.hpp"

namespace squadbox::gfx {

class vulkan_manager;
class camera;
class render_manager;
class render_thread;

}

namespace squadbox::gfx::render_techniques {

class flat_shading {
public:
    using mesh_type = gpu_mesh<gpu_mesh_position<gpu_mesh_usage::vertex>, gpu_mesh_normal<gpu_mesh_usage::vertex>>;

private:
    struct render_data_t : render_job_command_buffer_base {
        vk::UniqueDescriptorSet descriptor_set;
        vk::UniqueBuffer uniform_buffer;
        vk::UniqueDeviceMemory uniform_buffer_memory;
        mesh_type mesh;
    };

public:
    using render_data = std::shared_ptr<render_data_t>;

    flat_shading(const vulkan_manager& vulkan_manager, const vk::RenderPass& render_pass);

    render_data prepare_render_data(mesh_type&& mesh) const;

    void render(render_thread& render_thread,
                gsl::not_null<render_data> render_job_data,
                const vk::Viewport& viewport, const camera& camera, const glm::mat4& model_matrix,
                const glm::vec4& model_color, const glm::vec4& ambient_color) const;

private:
    struct persistent_data {
        vk::UniqueShaderModule vert_shader;
        vk::UniqueShaderModule frag_shader;
        vk::UniqueDescriptorSetLayout descriptor_set_layout;
        vk::UniqueDescriptorPool descriptor_pool;
        vk::UniquePipelineLayout pipeline_layout;
        vk::UniquePipeline graphics_pipeline;

        std::unique_ptr<std::mutex> descriptor_pool_mutex = std::make_unique<std::mutex>();
    };

    persistent_render_data<persistent_data> m_persistent_render_data;

    struct ubo_t {
        glm::mat4 model_view;
        glm::mat4 projection;
        glm::vec4 model_color;
        glm::vec4 ambient_color;
    };

    static const std::uint32_t vertex_ubo_binding_idx = 0;

    gsl::not_null<const vulkan_manager*> m_vulkan_manager;
};

}

#endif