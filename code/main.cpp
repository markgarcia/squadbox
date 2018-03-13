#include "console_ui.hpp"
#include "gfx/imgui_glue.hpp"
#include "gfx/render_manager.hpp"
#include "gfx/vulkan_manager.hpp"
#include "gfx/vulkan_utils.hpp"
#include "gfx/glfw_wrappers.hpp"

#include <chrono>


int main() {
    using namespace squadbox;

#if _DEBUG
    // Until support for setting environment variables in CMake for Visual Studio is made...
    _putenv("VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_standard_validation;VK_LAYER_LUNARG_monitor");
#endif
    
    gfx::glfw_manager glfw_manager;
    gfx::glfw_window window { 800, 600, "squadbox", nullptr, nullptr };
    gfx::vulkan_manager vulkan_manager { window.get() };
    gfx::render_manager render_manager { vulkan_manager };
    gfx::imgui_glue imgui_glue { window.get(), vulkan_manager, render_manager };

    console_ui console_ui;
#if _DEBUG
    console_ui.show();
#endif

    auto framebuffer_resize_callback = [&render_manager](GLFWwindow*, int width, int height) {
        render_manager.resize_framebuffer(width, height);
    };

    auto key_callback = [&imgui_glue, &console_ui](GLFWwindow*, int key, int /*scancode*/, int action, int /*mode*/) {
        if (key == GLFW_KEY_GRAVE_ACCENT && action == GLFW_PRESS) {
            console_ui.toggle_visibility();
        }

        gfx::imgui_glue::key_info key_info;
        key_info.key = key;
        key_info.action = action;
        imgui_glue.key_event(key_info);
    };

    auto mouse_button_callback = [&imgui_glue](GLFWwindow*, int button, int action, int /*mods*/) {
        gfx::imgui_glue::mouse_button_info mouse_button_info;
        mouse_button_info.button = button;
        mouse_button_info.action = action;
        imgui_glue.mouse_button_event(mouse_button_info);
    };

    auto scroll_callback = [&imgui_glue](GLFWwindow*, double /*x_offset*/, double y_offset) {
        imgui_glue.scroll_event(y_offset);
    };

    auto char_callback = [&imgui_glue](GLFWwindow*, unsigned int c) {
        imgui_glue.char_event(c);
    };

    window.register_key_callback(key_callback);
    window.register_mouse_button_callback(mouse_button_callback);
    window.register_scroll_callback(scroll_callback);
    window.register_char_callback(char_callback);

    auto current_time = std::chrono::high_resolution_clock::now();
    //std::chrono::duration<double> delta_accumulator;

    render_manager.render_immediately(imgui_glue.load_font_textures());

    while (!glfwWindowShouldClose(window.get()))
    {
        auto new_time = std::chrono::high_resolution_clock::now();
        auto delta_time = new_time - current_time;

        // Updates
        {
            glfwPollEvents();

            imgui_glue.new_frame(delta_time);
            console_ui.update();
        }

        // Render
        {
            render_manager.begin_frame();

            render_manager.add_render_job(imgui_glue.render(render_manager.command_buffer_inheritance_info()));

            render_manager.end_frame();
        }
        
        current_time = new_time;
    }
}