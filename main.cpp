#include "imgui_glue.hpp"
#include "vulkan_manager.hpp"
#include "vulkan_utils.hpp"
#include "glfw_wrappers.hpp"

#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/thread/future.hpp>

#include <chrono>


int main() {
    using namespace squadbox;

#if _DEBUG
    // Until support for setting environment variables in CMake for Visual Studio is made...
    _putenv("VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_standard_validation");
#endif
    
    boost::basic_thread_pool thread_pool;
    
    auto& glfw = init_glfw();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfw_window window { glfwCreateWindow(800, 600, "squadbox", nullptr, nullptr) };

    auto vulkan_manager = init_vulkan(window.get());

    imgui_glue imgui_glue { window.get(), vulkan_manager };

    {
        auto load_resources = imgui_glue.load_font_textures();
        auto queue = vulkan_manager.device().getQueue(vulkan_manager.graphics_queue_family_index(), 0);

        vk::SubmitInfo submit_info;
        submit_info
            .setPCommandBuffers(&std::get<vk::UniqueCommandBuffer>(load_resources).get())
            .setCommandBufferCount(1);

        queue.submit({ submit_info }, nullptr);

        vulkan_manager.device().waitIdle();
    }

    auto framebuffer_resize_callback = glfw_framebuffer_resize_callback(window.get(), [&vulkan_manager](GLFWwindow*, int width, int height) {
        vulkan_manager.resize_framebuffer(width, height);
    });

    auto key_callback = glfw_key_callback(window.get(), [&imgui_glue](GLFWwindow*, int key, int /*scancode*/, int action, int /*mode*/) {
        imgui_glue::key_info key_info;
        key_info.key = key;
        key_info.action = action;
        imgui_glue.key_event(key_info);
    });

    auto mouse_callback = glfw_mouse_button_callback(window.get(), [&imgui_glue](GLFWwindow*, int button, int action, int /*mods*/) {
        imgui_glue::mouse_button_info mouse_button_info;
        mouse_button_info.button = button;
        mouse_button_info.action = action;
        imgui_glue.mouse_button_event(mouse_button_info);
    });

    auto scroll_callback = glfw_scroll_callback(window.get(), [&imgui_glue](GLFWwindow*, double /*x_offset*/, double y_offset) {
        imgui_glue.scroll_event(y_offset);
    });

    auto char_callback = glfw_char_callback(window.get(), [&imgui_glue](GLFWwindow*, unsigned int c) {
        imgui_glue.char_event(c);
    });

    auto current_time = std::chrono::high_resolution_clock::now();
    //std::chrono::duration<double> delta_accumulator;

    vk::ClearValue clear_value = { 0 };

    auto command_pool = [](const vk::Device& device, std::uint32_t graphics_queue_family_index) {
        vk::CommandPoolCreateInfo command_pool_ci;
        command_pool_ci
            .setQueueFamilyIndex(graphics_queue_family_index);
        return device.createCommandPoolUnique(command_pool_ci);
    }(vulkan_manager.device(), vulkan_manager.graphics_queue_family_index());

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
            vulkan_manager.device().resetCommandPool(command_pool.get(), {});

            auto framebuffer_image_acquire_semaphore = vulkan_manager.device().createSemaphoreUnique({});

            auto primary_command_buffer = vk_utils::create_primary_command_buffer(vulkan_manager.device(), command_pool.get());
            vk::UniqueCommandBuffer imgui_render_command_buffer;
            {
                auto current_framebuffer_idx
                    = vulkan_manager.device().acquireNextImageKHR(
                            vulkan_manager.swapchain(), std::numeric_limits<std::uint64_t>::max(),
                            framebuffer_image_acquire_semaphore.get(), nullptr).value;
                vulkan_manager.set_current_framebuffer_idx(current_framebuffer_idx);

                primary_command_buffer->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

                vk::RenderPassBeginInfo render_pass_begin_info;
                render_pass_begin_info
                    .setRenderPass(vulkan_manager.render_pass())
                    .setFramebuffer(vulkan_manager.current_framebuffer())
                    .setPClearValues(&clear_value)
                    .setClearValueCount(1)
                    .renderArea.extent
                    .setWidth(vulkan_manager.framebuffer_width())
                    .setHeight(vulkan_manager.framebuffer_height());

                primary_command_buffer->beginRenderPass(render_pass_begin_info, vk::SubpassContents::eSecondaryCommandBuffers);

                vk::CommandBufferInheritanceInfo command_buffer_inheritance_info;
                command_buffer_inheritance_info
                    .setRenderPass(vulkan_manager.render_pass())
                    .setSubpass(0)
                    .setFramebuffer(vulkan_manager.current_framebuffer());

                auto imgui_render_future = boost::async(thread_pool, [&imgui_glue, command_buffer_inheritance_info]() {
                    return imgui_glue.render(command_buffer_inheritance_info);
                });

                imgui_render_command_buffer = imgui_render_future.get();
                primary_command_buffer->executeCommands({ imgui_render_command_buffer.get() });

                primary_command_buffer->endRenderPass();

                primary_command_buffer->end();
            }

            auto graphics_queue = vulkan_manager.device().getQueue(vulkan_manager.graphics_queue_family_index(), 0);

            vk::PipelineStageFlags pipe_stage_flags = vk::PipelineStageFlagBits::eBottomOfPipe;
            
            vk::SubmitInfo submit_info;
            submit_info
                .setPCommandBuffers(&primary_command_buffer.get())
                .setCommandBufferCount(1)
                .setPWaitSemaphores(&framebuffer_image_acquire_semaphore.get())
                .setWaitSemaphoreCount(1)
                .setPWaitDstStageMask(&pipe_stage_flags);

            auto draw_fence = vulkan_manager.device().createFenceUnique({});

            graphics_queue.submit({ submit_info }, draw_fence.get());

            while (true) {
                auto result = vulkan_manager.device().waitForFences({ draw_fence.get() }, true, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count());
                if (result == vk::Result::eSuccess) break;
            }

            {
                auto swapchain = vulkan_manager.swapchain();
                auto framebuffer_idx = vulkan_manager.current_framebuffer_idx();

                vk::PresentInfoKHR present_info;
                present_info
                    .setPSwapchains(&swapchain)
                    .setSwapchainCount(1)
                    .setPImageIndices(&framebuffer_idx);

                graphics_queue.presentKHR(present_info);
            }
        }
        
        current_time = new_time;
    }
}