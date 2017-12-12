#ifndef SQUADBOX_VULKAN_MANAGER_HPP
#define SQUADBOX_VULKAN_MANAGER_HPP

#pragma once

#include <gsl/gsl>

#include <vulkan/vulkan.hpp>

struct GLFWwindow;

namespace squadbox {

class vulkan_manager {
public:
    friend vulkan_manager init_vulkan(gsl::not_null<GLFWwindow*> window);

    void resize_framebuffer(std::uint32_t width, std::uint32_t height);

    const vk::Instance& instance() const { return m_instance.get(); }
    const vk::PhysicalDevice& physical_device() const { return m_physical_device; }
    const vk::Device& device() const { return m_device.get(); }
    const vk::SurfaceKHR& surface() const { return m_surface.get(); }
    const vk::RenderPass& render_pass() const { return m_render_pass.get(); }

    std::uint32_t graphics_queue_family_index() const { return m_graphics_queue_family_index; }
    std::uint32_t present_queue_family_index() const { return m_present_queue_family_index; }
    const vk::SurfaceFormatKHR& surface_format() const { return m_surface_format; }

private:
    vulkan_manager(gsl::not_null<GLFWwindow*> window);

    vk::UniqueInstance m_instance;
    vk::PhysicalDevice m_physical_device;
    vk::UniqueSurfaceKHR m_surface;
    vk::UniqueDevice m_device;
    vk::UniqueRenderPass m_render_pass;
    vk::UniqueSwapchainKHR m_swapchain;
    std::vector<std::tuple<vk::Image, vk::UniqueImageView>> m_swapchain_images;
    std::vector<vk::UniqueFramebuffer> m_framebuffers;

    std::uint32_t m_graphics_queue_family_index;
    std::uint32_t m_present_queue_family_index;
    vk::SurfaceFormatKHR m_surface_format;
};

inline vulkan_manager init_vulkan(gsl::not_null<GLFWwindow*> window) {
    return { window };
}

}

#endif