#ifndef SQUADBOX_VULKAN_UTILS_HPP
#define SQUADBOX_VULKAN_UTILS_HPP

#pragma once

#include <vulkan/vulkan.hpp>

namespace squadbox::vk_utils {

std::uint32_t get_memory_type_index(const vk::Device& device, const vk::PhysicalDeviceMemoryProperties& device_memory_properties,
                                    const vk::MemoryRequirements& memory_requirements, vk::MemoryPropertyFlags required_memory_props);

vk::UniqueDeviceMemory alloc_memory(const vk::Device& device, const vk::PhysicalDeviceMemoryProperties& device_memory_properties,
                                    const vk::MemoryRequirements& memory_requirements, vk::MemoryPropertyFlags required_memory_props);

vk::UniqueCommandBuffer create_primary_command_buffer(const vk::Device& device, const vk::CommandPool& command_pool);

}

#endif