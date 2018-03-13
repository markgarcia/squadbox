#ifndef SQUADBOX_GFX_VULKAN_UTILS_HPP
#define SQUADBOX_GFX_VULKAN_UTILS_HPP

#pragma once

#include <vulkan/vulkan.hpp>

namespace squadbox::gfx::vk_utils {

std::uint32_t get_memory_type_index(const vk::PhysicalDeviceMemoryProperties& device_memory_properties,
                                    const vk::MemoryRequirements& memory_requirements, vk::MemoryPropertyFlags required_memory_props);

vk::UniqueDeviceMemory alloc_memory(const vk::Device& device, const vk::PhysicalDeviceMemoryProperties& device_memory_properties,
                                    const vk::MemoryRequirements& memory_requirements, vk::MemoryPropertyFlags required_memory_props);

vk::UniqueCommandBuffer create_primary_command_buffer(const vk::Device& device, const vk::CommandPool& command_pool);

template<typename T>
void copy_to_memory(const vk::Device& device, const vk::DeviceMemory& memory, const T& source, const vk::DeviceSize offset = 0) {
    auto dest = static_cast<T*>(device.mapMemory(memory, offset, sizeof(T)));
    *dest = source;

    vk::MappedMemoryRange mapped_memory;
    mapped_memory
        .setMemory(memory)
        .setOffset(offset)
        .setSize(sizeof(T));

    device.flushMappedMemoryRanges({ mapped_memory });
    device.unmapMemory(memory);
}

}

#endif