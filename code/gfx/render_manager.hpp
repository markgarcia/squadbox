#ifndef SQUADBOX_GFX_RENDER_MANAGER_HPP
#define SQUADBOX_GFX_RENDER_MANAGER_HPP

#include <vulkan/vulkan.hpp>
#include <gsl/gsl>

struct GLFWwindow;

namespace squadbox::gfx {

class vulkan_manager;
class render_job;

class render_manager {
public:
    render_manager(const vulkan_manager& vulkan_manager);
    render_manager(render_manager&&) = default;
    ~render_manager();

    void resize_framebuffer(std::uint32_t width, std::uint32_t height);
    void set_clear_color(vk::ClearValue color) { m_clear_color = color; }

    void begin_frame();
    void end_frame();

    void add_render_job(const render_job& render_job);
    void add_render_job(render_job&& render_job);

    void render_immediately(const render_job& render_job);

    vk::CommandBufferInheritanceInfo command_buffer_inheritance_info();

    const vk::RenderPass& render_pass() const { return m_render_pass.get(); }
    const vk::SwapchainKHR& swapchain() const { return m_swapchain.get(); }

    vk::Framebuffer get_framebuffer(std::uint32_t idx) const { return m_framebuffers[idx].get(); }
    std::uint32_t num_frames() const { return static_cast<std::uint32_t>(m_framebuffers.size()); }
    std::uint32_t framebuffer_width() const { return m_framebuffer_width; }
    std::uint32_t framebuffer_height() const { return m_framebuffer_height; }

private:
    struct frame_data {
        std::uint32_t framebuffer_idx;
        vk::Framebuffer framebuffer;
        vk::UniqueCommandBuffer primary_command_buffer;
        vk::UniqueFence fence;
        vk::UniqueSemaphore framebuffer_image_acquire_semaphore;
        std::vector<squadbox::gfx::render_job> jobs;
    };

    gsl::not_null<const vulkan_manager*> m_vulkan_manager;
    vk::UniqueRenderPass m_render_pass;
    vk::UniqueSwapchainKHR m_swapchain;
    std::vector<std::tuple<vk::Image, vk::UniqueImageView>> m_swapchain_images;
    std::vector<vk::UniqueFramebuffer> m_framebuffers;
    std::uint32_t m_framebuffer_width;
    std::uint32_t m_framebuffer_height;
    vk::UniqueCommandPool m_primary_command_pool;

    std::array<frame_data, 3> m_frames;
    std::uint32_t m_current_frame_idx = 0;
    vk::ClearValue m_clear_color;
};

}

#endif