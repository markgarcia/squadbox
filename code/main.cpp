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
    
    auto& glfw = gfx::init_glfw();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    gfx::glfw_window window { glfwCreateWindow(800, 600, "squadbox", nullptr, nullptr) };
    gfx::vulkan_manager vulkan_manager { window.get() };
    gfx::render_manager render_manager { vulkan_manager };
    gfx::imgui_glue imgui_glue { window.get(), vulkan_manager, render_manager };

    {
        auto render_job = imgui_glue.load_font_textures();
        auto queue = vulkan_manager.device().getQueue(vulkan_manager.graphics_queue_family_index(), 0);

    auto framebuffer_resize_callback = gfx::glfw_framebuffer_resize_callback(window.get(), [&render_manager](GLFWwindow*, int width, int height) {
        render_manager.resize_framebuffer(width, height);
    });

    auto key_callback = gfx::glfw_key_callback(window.get(), [&imgui_glue, &console_ui](GLFWwindow*, int key, int /*scancode*/, int action, int /*mode*/) {
        gfx::imgui_glue::key_info key_info;
        key_info.key = key;
        key_info.action = action;
        imgui_glue.key_event(key_info);
    });

    auto mouse_callback = gfx::glfw_mouse_button_callback(window.get(), [&imgui_glue](GLFWwindow*, int button, int action, int /*mods*/) {
        gfx::imgui_glue::mouse_button_info mouse_button_info;
        mouse_button_info.button = button;
        mouse_button_info.action = action;
        imgui_glue.mouse_button_event(mouse_button_info);
    });

    auto scroll_callback = gfx::glfw_scroll_callback(window.get(), [&imgui_glue](GLFWwindow*, double /*x_offset*/, double y_offset) {
        imgui_glue.scroll_event(y_offset);
    });

    auto char_callback = gfx::glfw_char_callback(window.get(), [&imgui_glue](GLFWwindow*, unsigned int c) {
        imgui_glue.char_event(c);
    });

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

            bool show_test_window = true;
            ImGui::ShowTestWindow(&show_test_window);
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