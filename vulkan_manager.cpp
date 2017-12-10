#include "vulkan_manager.hpp"

#include <GLFW/glfw3.h>

#include <array>

namespace squadbox {

vulkan_manager::vulkan_manager(gsl::not_null<GLFWwindow*> window) {
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
    }(m_instance.get(), window);

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

    m_command_pool = [](const vk::Device& device, std::uint32_t graphics_queue_family_index) {
        vk::CommandPoolCreateInfo command_pool_ci;
        command_pool_ci.setQueueFamilyIndex(graphics_queue_family_index);
        return device.createCommandPoolUnique(command_pool_ci);
    }(m_device.get(), m_graphics_queue_family_index);

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

    m_render_pass = [](const vk::Device& device, const vk::SurfaceFormatKHR& surface_format) {
        vk::AttachmentDescription color_attachment_desc;
        color_attachment_desc
            .setFormat(surface_format.format)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

        vk::AttachmentReference color_reference;
        color_reference
            .setAttachment(0)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        vk::SubpassDescription subpass_desc;
        subpass_desc
            .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setPColorAttachments(&color_reference)
            .setColorAttachmentCount(1);

        vk::RenderPassCreateInfo render_pass_ci;
        render_pass_ci
            .setPAttachments(&color_attachment_desc)
            .setAttachmentCount(1)
            .setPSubpasses(&subpass_desc)
            .setSubpassCount(1);

        return device.createRenderPassUnique(render_pass_ci);
    }(m_device.get(), m_surface_format);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    resize_framebuffer(width, height);
}

void vulkan_manager::resize_framebuffer(std::uint32_t width, std::uint32_t height) {
    m_device->waitIdle();

    auto new_swapchain = [](const vk::PhysicalDevice& physical_device, const vk::Device& device,
                            const vk::SurfaceKHR& surface, const vk::SurfaceFormatKHR& surface_format,
                            const vk::SwapchainKHR& swapchain,
                            std::uint32_t graphics_queue_family_index, std::uint32_t present_queue_family_index,
                            std::uint32_t width, std::uint32_t height) {
        vk::Extent2D swapchain_extent;

        auto surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(surface);

        if (surface_capabilities.currentExtent.width == std::numeric_limits<decltype(surface_capabilities.currentExtent.width)>::max()) {
            swapchain_extent.width = std::clamp(width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
            swapchain_extent.height = std::clamp(height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
        }
        else {
            swapchain_extent = surface_capabilities.currentExtent;
        }

        vk::SurfaceTransformFlagBitsKHR pre_transform;
        if (surface_capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) {
            pre_transform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
        }
        else {
            pre_transform = surface_capabilities.currentTransform;
        }

        vk::SwapchainCreateInfoKHR swapchain_ci;
        swapchain_ci
            .setSurface(surface)
            .setImageFormat(surface_format.format)
            .setImageColorSpace(surface_format.colorSpace)
            .setMinImageCount(surface_capabilities.minImageCount)
            .setImageExtent(swapchain_extent)
            .setPreTransform(pre_transform)
            .setPresentMode(vk::PresentModeKHR::eFifo)
            .setOldSwapchain(swapchain)
            .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);

        if (graphics_queue_family_index != present_queue_family_index) {
            std::array<decltype(graphics_queue_family_index), 2> queue_families = { graphics_queue_family_index, present_queue_family_index };
            swapchain_ci
                .setImageSharingMode(vk::SharingMode::eConcurrent)
                .setPQueueFamilyIndices(queue_families.data())
                .setQueueFamilyIndexCount(queue_families.size());
        }
        else {
            swapchain_ci
                .setImageSharingMode(vk::SharingMode::eExclusive)
                .setPQueueFamilyIndices(nullptr)
                .setQueueFamilyIndexCount(0);
        }

        return device.createSwapchainKHRUnique(swapchain_ci);
    }(m_physical_device, m_device.get(), m_surface.get(), m_surface_format, m_swapchain.get(),
      m_graphics_queue_family_index, m_present_queue_family_index, width, height);

    auto new_swapchain_images = [](const vk::Device& device, const vk::SurfaceFormatKHR& surface_format,
                                   const vk::SwapchainKHR& swapchain) {
        auto swapchain_images = device.getSwapchainImagesKHR(swapchain);

        std::vector<std::tuple<vk::Image, vk::UniqueImageView>> swapchain_images_store;
        swapchain_images_store.reserve(swapchain_images.size());

        for (auto&& image : swapchain_images) {
            vk::ImageViewCreateInfo image_view_ci;
            image_view_ci
                .setImage(image)
                .setViewType(vk::ImageViewType::e2D)
                .setFormat(surface_format.format)
                .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

            auto image_view = device.createImageViewUnique(image_view_ci);
            swapchain_images_store.emplace_back(image, std::move(image_view));
        }

        return swapchain_images_store;
    }(m_device.get(), m_surface_format, new_swapchain.get());

    auto new_framebuffers = [](const vk::Device& device, const vk::RenderPass& render_pass,
                               const std::vector<std::tuple<vk::Image, vk::UniqueImageView>>& swapchain_images,
                               std::uint32_t width, std::uint32_t height) {
        std::array<vk::ImageView, 1> attachments;

        vk::FramebufferCreateInfo framebuffer_ci;
        framebuffer_ci
            .setRenderPass(render_pass)
            .setPAttachments(attachments.data())
            .setAttachmentCount(attachments.size())
            .setWidth(width)
            .setHeight(height)
            .setLayers(1);

        std::vector<vk::UniqueFramebuffer> framebuffers;
        framebuffers.reserve(swapchain_images.size());

        for (auto&& swapchain_image : swapchain_images) {
            attachments[0] = std::get<vk::UniqueImageView>(swapchain_image).get();
            framebuffers.emplace_back(device.createFramebufferUnique(framebuffer_ci));
        }

        return framebuffers;
    }(m_device.get(), m_render_pass.get(), new_swapchain_images, width, height);

    m_framebuffers = std::move(new_framebuffers);
    m_swapchain_images = std::move(new_swapchain_images);
    m_swapchain = std::move(new_swapchain);
}

}