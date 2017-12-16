#ifndef SQUADBOX_GFX_VULKAN_MANAGER_HPP
#define SQUADBOX_GFX_VULKAN_MANAGER_HPP

#pragma once

#include <gsl/gsl>

#include <vulkan/vulkan.hpp>

struct GLFWwindow;

namespace squadbox::gfx {

class vulkan_manager {
public:
    vulkan_manager(gsl::not_null<GLFWwindow*> window);
    vulkan_manager(vulkan_manager&&) = default;
    ~vulkan_manager();

    void resize_framebuffer(std::uint32_t width, std::uint32_t height);

    const vk::Instance& instance() const { return m_instance.get(); }
    const vk::PhysicalDevice& physical_device() const { return m_physical_device; }
    const vk::Device& device() const { return m_device.get(); }
    const vk::SurfaceKHR& surface() const { return m_surface.get(); }
    const vk::RenderPass& render_pass() const { return m_render_pass.get(); }
    const vk::SwapchainKHR& swapchain() const { return m_swapchain.get(); }

    vk::Framebuffer get_framebuffer(std::uint32_t idx) const { return m_framebuffers[idx].get(); }
    std::uint32_t num_frames() const { return static_cast<std::uint32_t>(m_framebuffers.size()); }
    std::uint32_t framebuffer_width() const { return m_framebuffer_width; }
    std::uint32_t framebuffer_height() const { return m_framebuffer_height; }

    std::uint32_t graphics_queue_family_index() const { return m_graphics_queue_family_index; }
    std::uint32_t present_queue_family_index() const { return m_present_queue_family_index; }
    const vk::SurfaceFormatKHR& surface_format() const { return m_surface_format; }

private:
    vk::UniqueInstance m_instance;
    vk::PhysicalDevice m_physical_device;
    vk::UniqueSurfaceKHR m_surface;
    vk::UniqueDevice m_device;
    vk::UniqueRenderPass m_render_pass;
    vk::UniqueSwapchainKHR m_swapchain;
    std::vector<std::tuple<vk::Image, vk::UniqueImageView>> m_swapchain_images;
    std::vector<vk::UniqueFramebuffer> m_framebuffers;

    std::size_t m_graphics_queue_family_index;
    std::size_t m_present_queue_family_index;
    vk::SurfaceFormatKHR m_surface_format;
    std::uint32_t m_framebuffer_width;
    std::uint32_t m_framebuffer_height;
};

}

#endif