#include "imgui_glue.hpp"
#include "vulkan_manager.hpp"
#include "vulkan_utils.hpp"
#include "glfw_wrappers.hpp"

#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/thread/future.hpp>

#include <chrono>


struct frame {
    std::uint32_t framebuffer_idx;
    vk::Framebuffer framebuffer;
    std::vector<squadbox::render_job> jobs;
    vk::UniqueCommandBuffer command_buffer;
    vk::UniqueFence fence;
    vk::UniqueSemaphore framebuffer_image_acquire_semaphore;
};

int main() {
    using namespace squadbox;

#if _DEBUG
    // Until support for setting environment variables in CMake for Visual Studio is made...
    _putenv("VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_standard_validation;VK_LAYER_LUNARG_monitor");
#endif
    
    boost::basic_thread_pool thread_pool;
    
    auto& glfw = init_glfw();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    glfw_window window { glfwCreateWindow(800, 600, "squadbox", nullptr, nullptr) };
    vulkan_manager vulkan_manager { window.get() };
    imgui_glue imgui_glue { window.get(), vulkan_manager };

    {
        auto render_job = imgui_glue.load_font_textures();
        auto queue = vulkan_manager.device().getQueue(vulkan_manager.graphics_queue_family_index(), 0);

        vk::SubmitInfo submit_info;
        submit_info
            .setPCommandBuffers(&render_job.command_buffer())
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

    std::array<frame, 3> frames;
    for (auto& frame : frames) {
        frame.fence = vulkan_manager.device().createFenceUnique({ vk::FenceCreateFlagBits::eSignaled });
        frame.framebuffer_image_acquire_semaphore = vulkan_manager.device().createSemaphoreUnique({});
    }

    std::uint32_t current_frame_idx = 0;

    try {
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
                auto& current_frame = frames[current_frame_idx];

                while (true) {
                    auto result = vulkan_manager.device().waitForFences({ current_frame.fence.get() }, true, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(10)).count());
                    if (result == vk::Result::eSuccess) break;
                }

                vulkan_manager.device().resetFences({ current_frame.fence.get() });

                current_frame.jobs.clear();

                current_frame.command_buffer = vk_utils::create_primary_command_buffer(vulkan_manager.device(), command_pool.get());
                {
                    current_frame.framebuffer_idx
                        = vulkan_manager.device().acquireNextImageKHR(
                                vulkan_manager.swapchain(), std::numeric_limits<std::uint64_t>::max(),
                                current_frame.framebuffer_image_acquire_semaphore.get(), nullptr).value;
                    current_frame.framebuffer = vulkan_manager.get_framebuffer(current_frame.framebuffer_idx);

                    current_frame.command_buffer->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

                    vk::RenderPassBeginInfo render_pass_begin_info;
                    render_pass_begin_info
                        .setRenderPass(vulkan_manager.render_pass())
                        .setFramebuffer(current_frame.framebuffer)
                        .setPClearValues(&clear_value)
                        .setClearValueCount(1)
                        .renderArea.extent
                        .setWidth(vulkan_manager.framebuffer_width())
                        .setHeight(vulkan_manager.framebuffer_height());

                    current_frame.command_buffer->beginRenderPass(render_pass_begin_info, vk::SubpassContents::eSecondaryCommandBuffers);

                    vk::CommandBufferInheritanceInfo command_buffer_inheritance_info;
                    command_buffer_inheritance_info
                        .setRenderPass(vulkan_manager.render_pass())
                        .setSubpass(0)
                        .setFramebuffer(current_frame.framebuffer);

                    auto imgui_render_future = boost::async(thread_pool, [&imgui_glue, command_buffer_inheritance_info]() {
                        return imgui_glue.render(command_buffer_inheritance_info);
                    });

                    auto imgui_render_job = imgui_render_future.get();
                    current_frame.command_buffer->executeCommands({ imgui_render_job.command_buffer() });
                    current_frame.jobs.emplace_back(std::move(imgui_render_job));

                    current_frame.command_buffer->endRenderPass();

                    current_frame.command_buffer->end();
                }

                auto graphics_queue = vulkan_manager.device().getQueue(vulkan_manager.graphics_queue_family_index(), 0);

                vk::PipelineStageFlags pipe_stage_flags = vk::PipelineStageFlagBits::eBottomOfPipe;
            
                vk::SubmitInfo submit_info;
                submit_info
                    .setPCommandBuffers(&current_frame.command_buffer.get())
                    .setCommandBufferCount(1)
                    .setPWaitSemaphores(&current_frame.framebuffer_image_acquire_semaphore.get())
                    .setWaitSemaphoreCount(1)
                    .setPWaitDstStageMask(&pipe_stage_flags);

                graphics_queue.submit({ submit_info }, current_frame.fence.get());

                {
                    auto swapchain = vulkan_manager.swapchain();

                    vk::PresentInfoKHR present_info;
                    present_info
                        .setPSwapchains(&swapchain)
                        .setSwapchainCount(1)
                        .setPImageIndices(&current_frame.framebuffer_idx);

                    graphics_queue.presentKHR(present_info);
                }

                current_frame_idx = (current_frame_idx + 1) % vulkan_manager.num_frames();
            }
        
            current_time = new_time;
        }
    }
    catch (...) {
        vulkan_manager.device().waitIdle();
        throw;
    }

    vulkan_manager.device().waitIdle();
}