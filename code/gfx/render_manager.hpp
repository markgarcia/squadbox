#ifndef SQUADBOX_GFX_RENDER_MANAGER_HPP
#define SQUADBOX_GFX_RENDER_MANAGER_HPP

#include <vulkan/vulkan.hpp>
#include <gsl/gsl>
#include <boost/thread/synchronized_value.hpp>
#include <boost/thread/sync_bounded_queue.hpp>
#include <functional>
#include <thread>

struct GLFWwindow;

namespace squadbox::gfx {

class vulkan_manager;
class render_job;
class render_manager;

class render_thread {
public:
    friend class render_manager;

    vk::UniqueDescriptorSet allocate_descriptor_set(const vk::DescriptorSetLayout& layout) const;
    vk::UniqueCommandBuffer allocate_command_buffer() const;

    void add_render_job(const render_job& render_job);
    void add_render_job(render_job&& render_job);

    const vk::CommandBufferInheritanceInfo& command_buffer_inheritance_info() const;

private:
    render_thread(const render_manager& render_manager);

    void add_job(std::function<void(render_thread&)> job);
    void finish_jobs();

    gsl::not_null<const render_manager*> m_render_manager;

    vk::UniqueCommandPool m_command_pool;
    vk::CommandBufferInheritanceInfo m_command_buffer_inheritance_info;

    std::thread m_thread;
    boost::sync_bounded_queue<std::function<void(render_thread&)>> m_job_queue;
    std::vector<squadbox::gfx::render_job> m_render_jobs;
};

class render_manager {
public:
    friend class render_thread;

    render_manager(const vulkan_manager& vulkan_manager);
    render_manager(render_manager&&) = default;
    ~render_manager();

    void resize_framebuffer(std::uint32_t width, std::uint32_t height);
    void set_clear_color(vk::ClearColorValue color) { m_clear_color = color; }

    void begin_frame();
    void end_frame();

    template<typename func_type>
    void render(func_type&& func) {
        boost::async(m_thread_pool, [this](auto&& func) {

        }, std::forward<func_type>(func))
    }

    void render_immediately(const render_job& render_job);

    const vk::RenderPass& render_pass() const { return m_render_pass.get(); }
    const vk::SwapchainKHR& swapchain() const { return m_swapchain.get(); }

    vk::Framebuffer get_framebuffer(std::uint32_t idx) const { return m_framebuffers[idx].get(); }
    std::uint32_t num_frames() const { return static_cast<std::uint32_t>(m_framebuffers.size()); }
    std::uint32_t framebuffer_width() const { return m_framebuffer_width; }
    std::uint32_t framebuffer_height() const { return m_framebuffer_height; }

private:
    vk::UniqueDescriptorSet allocate_descriptor_set(const vk::DescriptorSetLayout& layout) const;

    struct frame_data {
        std::uint32_t framebuffer_idx;
        vk::Framebuffer framebuffer;
        vk::UniqueCommandBuffer primary_command_buffer;
        vk::UniqueFence fence;
        vk::UniqueSemaphore framebuffer_image_acquire_semaphore;
        std::vector<squadbox::gfx::render_job> render_jobs;
    };

    gsl::not_null<const vulkan_manager*> m_vulkan_manager;
    vk::UniqueRenderPass m_render_pass;
    vk::UniqueSwapchainKHR m_swapchain;
    std::vector<std::tuple<vk::Image, vk::UniqueImageView>> m_swapchain_images;
    std::vector<vk::UniqueFramebuffer> m_framebuffers;
    std::tuple<vk::UniqueDeviceMemory, vk::UniqueImage, vk::UniqueImageView> m_depth_stencil;

    std::uint32_t m_framebuffer_width;
    std::uint32_t m_framebuffer_height;
    vk::Format m_depth_stencil_format;

    std::array<frame_data, 3> m_frames;
    std::uint32_t m_current_frame_idx = 0;
    vk::ClearColorValue m_clear_color;

    vk::UniqueCommandPool m_primary_command_pool;
    boost::synchronized_value<vk::UniqueDescriptorPool> m_descriptor_pool;
    std::vector<render_thread> m_render_threads;
};

}

#endif