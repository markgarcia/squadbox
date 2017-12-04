#ifndef SQUADBOX_VULKAN_MANAGER_HPP
#define SQUADBOX_VULKAN_MANAGER_HPP

#pragma once

#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#include <gsl/gsl>
#undef _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>


namespace squadbox {

class vulkan_manager {
public:
    friend vulkan_manager init_vulkan(gsl::not_null<GLFWwindow*> window);

    void resize_framebuffer(std::uint32_t width, std::uint32_t height);

    const vk::UniqueInstance& instance() const { return m_instance; }
    const vk::PhysicalDevice& physical_device() const { return m_physical_device; }
    const vk::UniqueDevice& device() const { return m_device; }
    const vk::UniqueSurfaceKHR& surface() const { return m_surface; }

    std::uint32_t graphics_queue_family_index() const { return m_graphics_queue_family_index; }
    std::uint32_t present_queue_family_index() const { return m_present_queue_family_index; }

private:
    vulkan_manager(gsl::not_null<GLFWwindow*> window);

    vk::UniqueInstance m_instance;
    vk::PhysicalDevice m_physical_device;
    vk::UniqueSurfaceKHR m_surface;
    vk::UniqueDevice m_device;
    vk::UniqueCommandPool m_command_pool;
    vk::UniqueSwapchainKHR m_swapchain;
    std::vector<std::tuple<vk::Image, vk::UniqueImageView>> m_swapchain_images;

    std::uint32_t m_graphics_queue_family_index;
    std::uint32_t m_present_queue_family_index;
    vk::SurfaceFormatKHR m_surface_format;
};

inline vulkan_manager init_vulkan(gsl::not_null<GLFWwindow*> window) {
    return { window };
}

}

#endif