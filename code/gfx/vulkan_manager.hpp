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

    gsl::not_null<GLFWwindow*> window() const { return m_window; }

    const vk::Instance& instance() const { return m_instance.get(); }
    const vk::PhysicalDevice& physical_device() const { return m_physical_device; }
    const vk::Device& device() const { return m_device.get(); }
    const vk::SurfaceKHR& surface() const { return m_surface.get(); }

    std::uint32_t graphics_queue_family_index() const { return m_graphics_queue_family_index; }
    std::uint32_t present_queue_family_index() const { return m_present_queue_family_index; }
    const vk::SurfaceFormatKHR& surface_format() const { return m_surface_format; }

private:
    gsl::not_null<GLFWwindow*> m_window;

    vk::UniqueInstance m_instance;
    vk::PhysicalDevice m_physical_device;
    vk::UniqueSurfaceKHR m_surface;
    vk::UniqueDevice m_device;

    std::size_t m_graphics_queue_family_index;
    std::size_t m_present_queue_family_index;
    vk::SurfaceFormatKHR m_surface_format;
};

}

#endif