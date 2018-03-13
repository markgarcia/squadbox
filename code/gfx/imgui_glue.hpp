#ifndef SQUADBOX_GFX_IMGUI_GLUE_HPP
#define SQUADBOX_GFX_IMGUI_GLUE_HPP

#pragma once

#include "render_job.hpp"

#include <imgui.h>
#include <vulkan/vulkan.hpp>

#include <gsl/gsl>

#include <chrono>
#include <array>


struct GLFWwindow;

namespace squadbox::gfx {

class vulkan_manager;
class render_manager;

class imgui_glue {
public:
    struct key_info {
        int key;
        int action;
    };

    struct mouse_button_info {
        int button;
        int action;
    };

    imgui_glue(gsl::not_null<GLFWwindow*> window, const vulkan_manager& vulkan_manager, const render_manager& render_manager);
    imgui_glue(const imgui_glue&) = delete;
    ~imgui_glue();

    void new_frame(std::chrono::duration<double> delta);
    render_job render(const vk::CommandBufferInheritanceInfo& command_buffer_inheritance_info);

    render_job load_font_textures();

    void key_event(const key_info& key_info);
    void mouse_button_event(const mouse_button_info& mouse_button_info);
    void scroll_event(double y_offset);
    void char_event(unsigned int c);

private:
    gsl::not_null<GLFWwindow*> m_window;
    vk::Device m_device;
    vk::PhysicalDeviceMemoryProperties m_device_memory_props;

    /*
    shaders/imgui.frag:
    layout(set=0, binding=0) uniform sampler2D sTexture;
    */
    static const int font_sampler_descriptor_set_idx = 0;
    static const int font_sampler_binding_idx = 0;

    struct persistent_data {
        vk::UniqueShaderModule vert_shader;
        vk::UniqueShaderModule frag_shader;
        vk::UniqueSampler font_sampler;
        vk::UniqueDescriptorSetLayout descriptor_set_layout;
        vk::UniqueDescriptorPool descriptor_pool;
        vk::UniqueDescriptorSet descriptor_set;
        vk::UniquePipelineLayout pipeline_layout;
        vk::UniquePipeline graphics_pipeline;
        vk::UniqueCommandPool command_pool;

        vk::UniqueImage font_image;
        vk::UniqueDeviceMemory font_image_memory;
        vk::UniqueImageView font_image_view;
    };

    persistent_render_data<persistent_data> m_persistent_render_data;

    struct render_job_data : render_job_command_buffer_base {
        vk::UniqueBuffer vertex_buffer;
        vk::UniqueBuffer index_buffer;
        vk::UniqueDeviceMemory vertex_index_buffers_memory;
        vk::DeviceSize vertex_buffer_size = 0;
        vk::DeviceSize index_buffer_size = 0;
        vk::DeviceSize vertex_index_buffers_memory_size = 0;
        vk::DeviceSize vertex_buffer_memory_offset;
        vk::DeviceSize index_buffer_memory_offset;
    };

    render_job_pool<render_job_data, 2> m_render_job_pool;

    std::array<bool, 3> m_pressed_mouse_buttons;
    double m_mouse_wheel_pos = 0.0;
};

}

#endif