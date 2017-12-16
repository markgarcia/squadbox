#include "vulkan_utils.hpp"

#include <vulkan/vulkan.hpp>

namespace squadbox::gfx::vk_utils {

std::uint32_t get_memory_type_index(const vk::Device& device, const vk::PhysicalDeviceMemoryProperties& device_memory_properties,
                                    const vk::MemoryRequirements& memory_requirements, vk::MemoryPropertyFlags required_memory_props) {
    for (std::uint32_t i = 0; i < device_memory_properties.memoryTypeCount; ++i) {
        if ((memory_requirements.memoryTypeBits & (1 << i))
            && (device_memory_properties.memoryTypes[i].propertyFlags & required_memory_props) == required_memory_props) {
            return i;
        }
    }

    throw std::runtime_error("Vulkan: failed to find memory type");
}

vk::UniqueDeviceMemory alloc_memory(const vk::Device& device, const vk::PhysicalDeviceMemoryProperties& device_memory_properties,
                                    const vk::MemoryRequirements& memory_requirements, vk::MemoryPropertyFlags required_memory_props) {
    std::uint32_t memory_type_index;
    try {
        memory_type_index = get_memory_type_index(device, device_memory_properties, memory_requirements, required_memory_props);
    }
    catch (const std::runtime_error& e) {
        std::throw_with_nested(std::runtime_error("Vulkan: failed to allocate required memory type"));
    }

    vk::MemoryAllocateInfo memory_alloc_info;
    memory_alloc_info
        .setAllocationSize(memory_requirements.size)
        .setMemoryTypeIndex(memory_type_index);

    return device.allocateMemoryUnique(memory_alloc_info);
}

vk::UniqueCommandBuffer create_primary_command_buffer(const vk::Device& device, const vk::CommandPool& command_pool) {
    vk::CommandBufferAllocateInfo command_buffer_alloc_info;
    command_buffer_alloc_info
        .setCommandPool(command_pool)
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount(1);

    return std::move(device.allocateCommandBuffersUnique(command_buffer_alloc_info)[0]);
}

}