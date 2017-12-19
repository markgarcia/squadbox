#include "render_manager.hpp"

#include "render_job.hpp"
#include "vulkan_manager.hpp"
#include "vulkan_utils.hpp"

#include <GLFW/glfw3.h>

#include <chrono>

namespace squadbox::gfx {

render_manager::render_manager(const vulkan_manager& vulkan_manager)
    : m_vulkan_manager(&vulkan_manager) {
    m_depth_stencil_format = [](const vk::PhysicalDevice& physical_device) {
        std::array<vk::Format, 1> depth_stencil_formats = {
            //vk::Format::eD32SfloatS8Uint,
            vk::Format::eD32Sfloat,
            //vk::Format::eD24UnormS8Uint,
            //vk::Format::eD16UnormS8Uint,
            //vk::Format::eD16Unorm
        };

        for (const auto& format : depth_stencil_formats) {
            auto format_props = physical_device.getFormatProperties(format);
            if (format_props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
                return format;
            }
        }

        throw std::runtime_error("Vulkan: unable to find suitable depth format.");
    }(m_vulkan_manager->physical_device());

    m_render_pass = [](const vk::Device& device, const vk::SurfaceFormatKHR& surface_format, const vk::Format& depth_stencil_format) {
        std::array<vk::AttachmentDescription, 2> attachments;
        attachments[0]  // color
            .setFormat(surface_format.format)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

        attachments[1]  // depth/stencil
            .setFormat(depth_stencil_format)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        vk::AttachmentReference color_reference;
        color_reference
            .setAttachment(0)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        vk::AttachmentReference depth_stencil_reference;
        depth_stencil_reference
            .setAttachment(1)
            .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        std::array<vk::SubpassDescription, 1> subpasses;
        subpasses[0]
            .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setPColorAttachments(&color_reference)
            .setColorAttachmentCount(1)
            .setPDepthStencilAttachment(&depth_stencil_reference);

        vk::RenderPassCreateInfo render_pass_ci;
        render_pass_ci
            .setPAttachments(attachments.data())
            .setAttachmentCount(attachments.size())
            .setPSubpasses(subpasses.data())
            .setSubpassCount(subpasses.size());

        return device.createRenderPassUnique(render_pass_ci);
    }(m_vulkan_manager->device(), m_vulkan_manager->surface_format(), m_depth_stencil_format);

    m_primary_command_pool = [](const vk::Device& device, std::uint32_t graphics_queue_family_index) {
        vk::CommandPoolCreateInfo command_pool_ci;
        command_pool_ci
            .setQueueFamilyIndex(graphics_queue_family_index);
        return device.createCommandPoolUnique(command_pool_ci);
    }(m_vulkan_manager->device(), m_vulkan_manager->graphics_queue_family_index());

    int width, height;
    glfwGetFramebufferSize(m_vulkan_manager->window(), &width, &height);
    resize_framebuffer(width, height);

    for (auto& frame : m_frames) {
        frame.fence = m_vulkan_manager->device().createFenceUnique({ vk::FenceCreateFlagBits::eSignaled });
        frame.framebuffer_image_acquire_semaphore = m_vulkan_manager->device().createSemaphoreUnique({});
    }
}

render_manager::~render_manager() {
    if (m_vulkan_manager && m_vulkan_manager->device()) {
        m_vulkan_manager->device().waitIdle();
    }
}

void render_manager::resize_framebuffer(const std::uint32_t width, const std::uint32_t height) {
    m_vulkan_manager->device().waitIdle();

    auto new_swapchain = [](const vk::PhysicalDevice& physical_device, const vk::Device& device,
                            const vk::SurfaceKHR& surface, const vk::SurfaceFormatKHR& surface_format,
                            const vk::SwapchainKHR& swapchain,
                            const std::uint32_t graphics_queue_family_index, const std::uint32_t present_queue_family_index,
                            const std::uint32_t width, const std::uint32_t height) {
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
        {
            if (surface_capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) {
                pre_transform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
            }
            else {
                pre_transform = surface_capabilities.currentTransform;
            }
        }

        vk::CompositeAlphaFlagBitsKHR composite_alpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        {
            std::array<vk::CompositeAlphaFlagBitsKHR, 4> composite_alpha_flags = {
                vk::CompositeAlphaFlagBitsKHR::eOpaque,
                vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
                vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
                vk::CompositeAlphaFlagBitsKHR::eInherit,
            };
            for (auto flag : composite_alpha_flags) {
                if (surface_capabilities.supportedCompositeAlpha & flag) {
                    composite_alpha = flag;
                    break;
                }
            }
        }

        auto present_modes = physical_device.getSurfacePresentModesKHR(surface);
        std::array<vk::PresentModeKHR, 2> required_present_modes = { vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eImmediate };
        auto present_mode = std::find_first_of(present_modes.begin(), present_modes.end(), required_present_modes.begin(), required_present_modes.end());
        if (present_mode == present_modes.end()) throw std::runtime_error("Vulkan: required present mode(s) not found");

        vk::SwapchainCreateInfoKHR swapchain_ci;
        swapchain_ci
            .setSurface(surface)
            .setMinImageCount(surface_capabilities.minImageCount)
            .setImageExtent(swapchain_extent)
            .setImageFormat(surface_format.format)
            .setImageColorSpace(surface_format.colorSpace)
            .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc)
            .setImageSharingMode(vk::SharingMode::eExclusive)
            .setPreTransform(pre_transform)
            .setCompositeAlpha(composite_alpha)
            .setImageArrayLayers(1)
            .setPresentMode(*present_mode)
            .setClipped(true)
            .setOldSwapchain(swapchain);

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
    }(m_vulkan_manager->physical_device(), m_vulkan_manager->device(), m_vulkan_manager->surface(), m_vulkan_manager->surface_format(), m_swapchain.get(),
      m_vulkan_manager->graphics_queue_family_index(), m_vulkan_manager->present_queue_family_index(), width, height);

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
                .subresourceRange
                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setBaseMipLevel(0)
                    .setLevelCount(1)
                    .setBaseArrayLayer(0)
                    .setLayerCount(1);

            auto image_view = device.createImageViewUnique(image_view_ci);
            swapchain_images_store.emplace_back(image, std::move(image_view));
        }

        return swapchain_images_store;
    }(m_vulkan_manager->device(), m_vulkan_manager->surface_format(), new_swapchain.get());

    auto new_depth_stencil = [](const vk::PhysicalDevice& physical_device, const vk::Device& device,
                                const vk::Format& depth_stencil_format,
                                const std::uint32_t width, const std::uint32_t height) {
        vk::ImageCreateInfo depth_stencil_buffer_image_ci;
        depth_stencil_buffer_image_ci
            .setImageType(vk::ImageType::e2D)
            .setFormat(depth_stencil_format)
            .setExtent({ width, height, 1 })
            .setMipLevels(1)
            .setArrayLayers(1)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(vk::ImageTiling::eOptimal)
            .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment);

        auto depth_stencil_buffer_image = device.createImageUnique(depth_stencil_buffer_image_ci);
        auto depth_stencil_buffer_memory = vk_utils::alloc_memory(device, physical_device.getMemoryProperties(),
                                                          device.getImageMemoryRequirements(depth_stencil_buffer_image.get()),
                                                          vk::MemoryPropertyFlagBits::eDeviceLocal);
        device.bindImageMemory(depth_stencil_buffer_image.get(), depth_stencil_buffer_memory.get(), 0);

        vk::ImageViewCreateInfo depth_stencil_buffer_image_view_ci;
        depth_stencil_buffer_image_view_ci
            .setImage(depth_stencil_buffer_image.get())
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(depth_stencil_format)
            .subresourceRange
                .setAspectMask(vk::ImageAspectFlagBits::eDepth /*| vk::ImageAspectFlagBits::eStencil*/)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1);

        auto depth_stencil_buffer_image_view = device.createImageViewUnique(depth_stencil_buffer_image_view_ci);

        return std::make_tuple(std::move(depth_stencil_buffer_memory), std::move(depth_stencil_buffer_image), std::move(depth_stencil_buffer_image_view));
    }(m_vulkan_manager->physical_device(), m_vulkan_manager->device(), m_depth_stencil_format, width, height);

    auto new_framebuffers = [](const vk::Device& device, const vk::RenderPass& render_pass,
                               const std::vector<std::tuple<vk::Image, vk::UniqueImageView>>& swapchain_images,
                               const vk::ImageView& depth_stencil_buffer_image_view,
                               const std::uint32_t width, const std::uint32_t height) {
        std::array<vk::ImageView, 2> attachments;
        attachments[1] = depth_stencil_buffer_image_view;

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
    }(m_vulkan_manager->device(), m_render_pass.get(), new_swapchain_images,
      std::get<vk::UniqueImageView>(new_depth_stencil).get(), width, height);

    m_depth_stencil = std::move(new_depth_stencil);
    m_framebuffers = std::move(new_framebuffers);
    m_swapchain_images = std::move(new_swapchain_images);
    m_swapchain = std::move(new_swapchain);

    m_framebuffer_width = width;
    m_framebuffer_height = height;
}


void render_manager::begin_frame() {
    auto& current_frame = m_frames[m_current_frame_idx];
    const auto& device = m_vulkan_manager->device();

    while (true) {
        auto result = device.waitForFences({ current_frame.fence.get() }, true, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1)).count());
        if (result == vk::Result::eSuccess) break;
    }

    device.resetFences({ current_frame.fence.get() });
    current_frame.jobs.clear();

    current_frame.primary_command_buffer = gfx::vk_utils::create_primary_command_buffer(device, m_primary_command_pool.get());

    current_frame.framebuffer_idx
        = device.acquireNextImageKHR(
            m_swapchain.get(), std::numeric_limits<std::uint64_t>::max(),
            current_frame.framebuffer_image_acquire_semaphore.get(), nullptr).value;
    current_frame.framebuffer = get_framebuffer(current_frame.framebuffer_idx);

    current_frame.primary_command_buffer->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    std::array<vk::ClearValue, 2> clear_values;
    clear_values[0].color = m_clear_color;
    clear_values[1].depthStencil = { 1.0f, 0 };

    vk::RenderPassBeginInfo render_pass_begin_info;
    render_pass_begin_info
        .setRenderPass(m_render_pass.get())
        .setFramebuffer(current_frame.framebuffer)
        .setPClearValues(clear_values.data())
        .setClearValueCount(clear_values.size())
        .renderArea.extent
        .setWidth(framebuffer_width())
        .setHeight(framebuffer_height());

    current_frame.primary_command_buffer->beginRenderPass(render_pass_begin_info, vk::SubpassContents::eSecondaryCommandBuffers);

    vk::CommandBufferInheritanceInfo command_buffer_inheritance_info;
    command_buffer_inheritance_info
        .setRenderPass(render_pass())
        .setSubpass(0)
        .setFramebuffer(current_frame.framebuffer);
}

void render_manager::end_frame() {
    auto& current_frame = m_frames[m_current_frame_idx];

    current_frame.primary_command_buffer->endRenderPass();
    current_frame.primary_command_buffer->end();

    auto graphics_queue = m_vulkan_manager->device().getQueue(m_vulkan_manager->graphics_queue_family_index(), 0);

    vk::PipelineStageFlags pipe_stage_flags = vk::PipelineStageFlagBits::eBottomOfPipe;

    vk::SubmitInfo submit_info;
    submit_info
        .setPCommandBuffers(&current_frame.primary_command_buffer.get())
        .setCommandBufferCount(1)
        .setPWaitSemaphores(&current_frame.framebuffer_image_acquire_semaphore.get())
        .setWaitSemaphoreCount(1)
        .setPWaitDstStageMask(&pipe_stage_flags);

    graphics_queue.submit({ submit_info }, current_frame.fence.get());

    vk::PresentInfoKHR present_info;
    present_info
        .setPSwapchains(&m_swapchain.get())
        .setSwapchainCount(1)
        .setPImageIndices(&current_frame.framebuffer_idx);

    graphics_queue.presentKHR(present_info);

    m_current_frame_idx = (m_current_frame_idx + 1) % num_frames();
}

void render_manager::add_render_job(const render_job& render_job) {
    auto& current_frame = m_frames[m_current_frame_idx];
    current_frame.jobs.push_back(render_job);
    current_frame.primary_command_buffer->executeCommands({ render_job.command_buffer() });
}

void render_manager::add_render_job(render_job&& render_job) {
    auto& current_frame = m_frames[m_current_frame_idx];
    current_frame.jobs.push_back(std::move(render_job));
    current_frame.primary_command_buffer->executeCommands({ render_job.command_buffer() });
}

vk::CommandBufferInheritanceInfo render_manager::command_buffer_inheritance_info() {
    auto& current_frame = m_frames[m_current_frame_idx];

    vk::CommandBufferInheritanceInfo command_buffer_inheritance_info;
    command_buffer_inheritance_info
        .setRenderPass(render_pass())
        .setSubpass(0)
        .setFramebuffer(current_frame.framebuffer);

    return command_buffer_inheritance_info;
}

void render_manager::render_immediately(const render_job& render_job) {
    auto queue = m_vulkan_manager->device().getQueue(m_vulkan_manager->graphics_queue_family_index(), 0);

    vk::SubmitInfo submit_info;
    submit_info
        .setPCommandBuffers(&render_job.command_buffer())
        .setCommandBufferCount(1);

    queue.submit({ submit_info }, nullptr);

    m_vulkan_manager->device().waitIdle();
}

}