#ifndef SQUADBOX_GFX_GPU_MEMORY_POOL_HPP
#define SQUADBOX_GFX_GPU_MEMORY_POOL_HPP

#pragma once

#include <vulkan/vulkan.hpp>

#include <boost/thread/synchronized_value.hpp>
#include <boost/pool/object_pool.hpp>
#include <boost/intrusive/list.hpp>
#include <gsl/gsl>
#include <memory>
#include <deque>
#include <mutex>

namespace squadbox::gfx {

class gpu_memory_pool {
public:
    friend class gpu_memory;

    gpu_memory_pool(const vk::Device& device, const vk::PhysicalDevice& physical_device);
    gpu_memory_pool(gpu_memory_pool&& rhs) = delete;
    ~gpu_memory_pool();

    gpu_memory allocate_gpu_local(const vk::MemoryRequirements& requirements);
    gpu_memory allocate_gpu_local_mappable(const vk::MemoryRequirements& requirements);

private:
    static constexpr vk::DeviceSize gpu_local_max_block_size = 32 * 1024 * 1024;        // 32 MiB
    static constexpr vk::DeviceSize gpu_local_mappable_max_block_size = 32 * 1024 * 1024;     // 32 MiB
    static constexpr vk::DeviceSize host_uncached_max_block_size = 32 * 1024 * 1024;    // 32 MiB

    struct memory_block {
        std::mutex mutex;
        vk::UniqueDeviceMemory block;
        vk::DeviceSize size;

        using suballocation_base_hook = boost::intrusive::list_base_hook<
            boost::intrusive::tag<class suballocation_base_hook_tag>,
            boost::intrusive::link_mode<boost::intrusive::normal_link>
        >;

        using free_suballocation_base_hook = boost::intrusive::list_base_hook<
            boost::intrusive::tag<class free_suballocation_base_hook_tag>,
            boost::intrusive::link_mode<boost::intrusive::normal_link>
        >;

        struct suballocation : suballocation_base_hook, free_suballocation_base_hook {
            memory_block* block;
            vk::DeviceSize offset;
            vk::DeviceSize size;
            bool is_free = false;
        };

        boost::intrusive::list<suballocation, boost::intrusive::base_hook<suballocation_base_hook>> suballocations;
        boost::intrusive::list<suballocation, boost::intrusive::base_hook<free_suballocation_base_hook>> free_suballocations;

        suballocation* max_free_suballocation = nullptr;
        boost::synchronized_value<boost::object_pool<memory_block::suballocation>>* suballocation_pool;
    };

    boost::synchronized_value<std::deque<memory_block>> m_gpu_local_blocks;
    boost::synchronized_value<std::deque<memory_block>> m_gpu_local_mappable_blocks;
    boost::synchronized_value<std::deque<memory_block>> m_host_uncached_blocks;

    boost::synchronized_value<boost::object_pool<memory_block::suballocation>> m_suballocation_pool;

    gsl::not_null<const vk::Device*> m_device;

    std::uint32_t m_gpu_local_memory_type_index;
    std::uint32_t m_gpu_local_mappable_memory_type_index;
    std::uint32_t m_host_uncached_memory_type_index;

    memory_block* try_allocate_memory_block(std::uint32_t memory_type_index, std::deque<memory_block>& storage, vk::DeviceSize max_size, vk::DeviceSize min_size);
    memory_block::suballocation* try_allocate(memory_block& block, const vk::MemoryRequirements& requirements);
};

class gpu_memory {
public:
    friend class gpu_memory_pool;

    gpu_memory(gpu_memory&& rhs);
    ~gpu_memory();

    gpu_memory& operator=(gpu_memory&& rhs);

    const vk::DeviceMemory& handle() const { return m_handle; }
    const vk::DeviceSize& offset() const { return m_suballocation->offset; }
    const vk::DeviceSize& size() const { return m_suballocation->size; }

private:
    gpu_memory(gsl::not_null<gpu_memory_pool::memory_block::suballocation*> suballocation)
        : m_handle(suballocation->block->block.get()),
          m_offset(suballocation->offset),
          m_size(suballocation->size),
          m_suballocation(suballocation) {}

    vk::DeviceMemory m_handle;
    vk::DeviceSize m_offset;
    vk::DeviceSize m_size;
    gpu_memory_pool::memory_block::suballocation* m_suballocation;
};

}


#endif