#include "vulkan_manager.hpp"

#include <GLFW/glfw3.h>

#include <array>

namespace squadbox::gfx {

vulkan_manager::vulkan_manager(gsl::not_null<GLFWwindow*> window)
    : m_window(window) {
    auto required_glfw_extensions = []() {
        std::uint32_t extensions_count;
        auto extensions_arr = glfwGetRequiredInstanceExtensions(&extensions_count);
        return gsl::make_span(extensions_arr, extensions_count);
    }();

    m_instance = [](gsl::span<const char*> required_glfw_extensions) {
        vk::InstanceCreateInfo instance_ci;
        instance_ci
            .setPpEnabledExtensionNames(!required_glfw_extensions.empty() ? required_glfw_extensions.data() : nullptr)
            .setEnabledExtensionCount(required_glfw_extensions.size());

        return vk::createInstanceUnique(instance_ci);
    }(required_glfw_extensions);

    m_surface = [](const vk::Instance& instance, gsl::not_null<GLFWwindow*> window) {
        VkSurfaceKHR surface;
        auto result = vk::Result { glfwCreateWindowSurface(static_cast<VkInstance>(instance), window, nullptr, &surface) };
        if (result != vk::Result::eSuccess) throw std::runtime_error(vk::to_string(result));

        return vk::UniqueSurfaceKHR { vk::SurfaceKHR { surface }, vk::SurfaceKHRDeleter { instance } };
    }(m_instance.get(), m_window);

    m_physical_device = [](const vk::Instance& instance) {
        auto physical_devices = instance.enumeratePhysicalDevices();
        if (physical_devices.empty()) throw std::runtime_error("No Vulkan device found.");

        return physical_devices[0];
    }(m_instance.get());

    {
        m_graphics_queue_family_index = std::numeric_limits<decltype(m_graphics_queue_family_index)>::max();
        m_present_queue_family_index = std::numeric_limits<decltype(m_present_queue_family_index)>::max();

        auto queue_families = m_physical_device.getQueueFamilyProperties();
        auto graphics_queue_family = std::find_if(queue_families.begin(), queue_families.end(), [](vk::QueueFamilyProperties queue) {
            return queue.queueFlags & vk::QueueFlagBits::eGraphics;
        });

        if (graphics_queue_family != queue_families.end()) {
            m_graphics_queue_family_index = std::distance(queue_families.begin(), graphics_queue_family);
        }

        m_present_queue_family_index = std::numeric_limits<decltype(m_present_queue_family_index)>::max();

        if (m_physical_device.getSurfaceSupportKHR(m_graphics_queue_family_index, m_surface.get()) == VK_TRUE) {
            m_present_queue_family_index = m_graphics_queue_family_index;
        }
        else {
            for (std::uint32_t i = 0; i < queue_families.size(); ++i) {
                if (m_physical_device.getSurfaceSupportKHR(i, m_surface.get()) == VK_TRUE) {
                    m_present_queue_family_index = i;
                    break;
                }
            }
        }

        if (m_graphics_queue_family_index == std::numeric_limits<decltype(m_graphics_queue_family_index)>::max()) {
            throw std::runtime_error("No Vulkan graphics queue found.");
        }

        if (m_present_queue_family_index == std::numeric_limits<decltype(m_present_queue_family_index)>::max()) {
            throw std::runtime_error("No Vulkan present queue found.");
        }
    }

    m_device = [](const vk::PhysicalDevice& physical_device, std::uint32_t graphics_queue_family_index) {
        vk::DeviceQueueCreateInfo queue_ci;
        float queue_priorities[] = { 0.0f };
        queue_ci
            .setQueueFamilyIndex(graphics_queue_family_index)
            .setQueueCount(1)
            .setPQueuePriorities(queue_priorities);

        std::array<const char*, 1> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        vk::DeviceCreateInfo device_ci;
        device_ci
            .setPQueueCreateInfos(&queue_ci)
            .setQueueCreateInfoCount(1)
            .setPpEnabledExtensionNames(!device_extensions.empty() ? device_extensions.data() : nullptr)
            .setEnabledExtensionCount(device_extensions.size());

        return physical_device.createDeviceUnique(device_ci);
    }(m_physical_device, m_graphics_queue_family_index);

    {
        auto surface_formats = m_physical_device.getSurfaceFormatsKHR(m_surface.get());
        if (surface_formats.size() == 1) {
            if (surface_formats[0].format == vk::Format::eUndefined) {
                m_surface_format.format = vk::Format::eB8G8R8A8Unorm;
                m_surface_format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
            }
            else {
                m_surface_format = surface_formats[0];;
            }
        }
        else {
            auto result = std::find_if(surface_formats.begin(), surface_formats.end(), [](vk::SurfaceFormatKHR format) {
                return format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
            });

            if (result != surface_formats.end()) {
                m_surface_format = *result;
            }
            else {
                m_surface_format = surface_formats[0];
            }
        }
    }
}

vulkan_manager::~vulkan_manager() {
    if (m_device) {
        m_device->waitIdle();
    }
}

}