#include "gpu_memory_pool.hpp"

namespace squadbox::gfx {

gpu_memory_pool::gpu_memory_pool(const vk::Device& device, const vk::PhysicalDevice& physical_device)
    : m_device(&device) {
    auto get_memory_type_index = [memory_props = physical_device.getMemoryProperties()](vk::MemoryPropertyFlags flags) {
        for (std::uint32_t i = 0; i < memory_props.memoryTypeCount; ++i) {
            if (memory_props.memoryTypes[i].propertyFlags & flags) {
                return i;
            }
        }

        throw std::runtime_error("Memory type not found in vulkan device.");
    };

    m_gpu_local_memory_type_index = get_memory_type_index(vk::MemoryPropertyFlagBits::eDeviceLocal);
    m_gpu_local_mappable_memory_type_index = get_memory_type_index(vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible);
    m_host_uncached_memory_type_index = get_memory_type_index(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
}

gpu_memory_pool::~gpu_memory_pool() {
#if _DEBUG
    auto assert_all_suballocs_freed = [](const std::deque<memory_block>& blocks) {
        for (const auto& block : blocks) {
            assert(block.suballocations.size() == 1);
            assert(block.suballocations.front().is_free);
        }
    };

    assert_all_suballocs_freed(*m_gpu_local_blocks);
    assert_all_suballocs_freed(*m_gpu_local_mappable_blocks);
    assert_all_suballocs_freed(*m_host_uncached_blocks);
#endif
}

gpu_memory gpu_memory_pool::allocate_gpu_local(const vk::MemoryRequirements& requirements) {
    if (!(requirements.memoryTypeBits & (1 << m_gpu_local_memory_type_index))) {
        throw std::runtime_error("Unsupported GPU memory type.");
    }

    memory_block::suballocation* suballocation = nullptr;

    auto gpu_local_blocks = m_gpu_local_blocks.synchronize();
    for (auto& memory_block : *gpu_local_blocks) {
        suballocation = try_allocate(memory_block, requirements);
        if (suballocation != nullptr) break;
    }

    if (!suballocation) {
        auto new_memory_block = try_allocate_memory_block(m_gpu_local_memory_type_index, *gpu_local_blocks, gpu_local_max_block_size, requirements.size);
        if (!new_memory_block) {
            if (!(requirements.memoryTypeBits & (1 << m_host_uncached_memory_type_index))) {
                throw std::runtime_error("Out of GPU memory.");
            }

            new_memory_block = try_allocate_memory_block(m_host_uncached_memory_type_index, *m_host_uncached_blocks, host_uncached_max_block_size, requirements.size);

            if(!new_memory_block) {
                throw std::runtime_error("Out of GPU memory.");
            }
        }

        suballocation = try_allocate(*new_memory_block, requirements);
    }

    assert(suballocation != nullptr);

    return { suballocation };
}

gpu_memory gpu_memory_pool::allocate_gpu_local_mappable(const vk::MemoryRequirements& requirements) {
    if (!(requirements.memoryTypeBits & (1 << m_gpu_local_mappable_memory_type_index))) {
        throw std::runtime_error("Unsupported GPU memory type.");
    }

    memory_block::suballocation* suballocation = nullptr;

    auto gpu_local_mappable_blocks = m_gpu_local_mappable_blocks.synchronize();
    for (auto& memory_block : *gpu_local_mappable_blocks) {
        suballocation = try_allocate(memory_block, requirements);
        if (suballocation != nullptr) break;
    }

    if (!suballocation) {
        auto new_memory_block = try_allocate_memory_block(m_gpu_local_mappable_memory_type_index, *gpu_local_mappable_blocks, gpu_local_mappable_max_block_size, requirements.size);

        if (!new_memory_block) {
            throw std::runtime_error("Out of GPU memory.");
        }

        suballocation = try_allocate(*new_memory_block, requirements);
    }

    assert(suballocation != nullptr);

    return { suballocation };
}

gpu_memory_pool::memory_block* gpu_memory_pool::try_allocate_memory_block(std::uint32_t memory_type_index, std::deque<memory_block>& storage, vk::DeviceSize max_size, vk::DeviceSize min_size) {
    vk::MemoryAllocateInfo memory_alloc_info;
    memory_alloc_info
        .setAllocationSize(max_size)
        .setMemoryTypeIndex(memory_type_index);

    while (true) {
        try {
            auto& block = storage.emplace_back();
            block.block = m_device->allocateMemoryUnique(memory_alloc_info);
            block.size = memory_alloc_info.allocationSize;

            auto deleter = [this](memory_block::suballocation* x) { m_suballocation_pool->destroy(x); };
            auto free_suballocation = std::unique_ptr<memory_block::suballocation, decltype(deleter)>(m_suballocation_pool->construct(), deleter);
            free_suballocation->block = &block;
            free_suballocation->offset = 0;
            free_suballocation->size = block.size;
            free_suballocation->is_free = true;

            block.suballocations.push_back(*free_suballocation);
            block.free_suballocations.push_back(*free_suballocation);
            block.max_free_suballocation = free_suballocation.get();

            free_suballocation.release();

            return &block;
        }
        catch (vk::OutOfDeviceMemoryError) {
            memory_alloc_info.allocationSize /= 2;
            if (memory_alloc_info.allocationSize < min_size) break;
        }
    }

    return nullptr;
}

gpu_memory_pool::memory_block::suballocation* gpu_memory_pool::try_allocate(memory_block& block, const vk::MemoryRequirements& requirements) {
    std::unique_lock lock(block.mutex);

    if (block.max_free_suballocation == nullptr || (block.max_free_suballocation->size - (block.max_free_suballocation->offset % requirements.alignment)) < requirements.size) {
        return nullptr;
    }

    memory_block::suballocation* free_suballocation_pick = nullptr;
    memory_block::suballocation* second_max_free_suballocation = nullptr;
    for (auto& free_suballocation : block.free_suballocations) {
        if ((free_suballocation.size - (free_suballocation.offset % requirements.alignment)) < requirements.size) continue;

        if (free_suballocation_pick == nullptr) {
            free_suballocation_pick = &free_suballocation;
        }
        else if (free_suballocation.size < free_suballocation_pick->size) {
            free_suballocation_pick = &free_suballocation;
        }

        if (free_suballocation_pick != block.max_free_suballocation) {
            if (second_max_free_suballocation == nullptr || free_suballocation_pick->size > second_max_free_suballocation->size) {
                second_max_free_suballocation = free_suballocation_pick;
            }
        }
    }

    assert(free_suballocation_pick != nullptr);

    memory_block::suballocation* new_suballocation = nullptr;

    auto new_suballocation_offset = free_suballocation_pick->offset + requirements.alignment - (free_suballocation_pick->offset % requirements.alignment);

    if (new_suballocation_offset == free_suballocation_pick->offset && requirements.size == free_suballocation_pick->size) {
        free_suballocation_pick->is_free = false;
        block.free_suballocations.erase(block.free_suballocations.iterator_to(*free_suballocation_pick));
        new_suballocation = free_suballocation_pick;
    }
    else {
        auto deleter = [this](memory_block::suballocation* x) { m_suballocation_pool->destroy(x); };
        auto new_suballocation_safe = std::unique_ptr<memory_block::suballocation, decltype(deleter)>(m_suballocation_pool->construct(), deleter);
        new_suballocation_safe->block = &block;
        new_suballocation_safe->offset = new_suballocation_offset;
        new_suballocation_safe->size = requirements.size;

        free_suballocation_pick->size = free_suballocation_pick->size - ((new_suballocation_safe->offset - free_suballocation_pick->offset) + new_suballocation->size);
        free_suballocation_pick->offset = new_suballocation_safe->offset + new_suballocation_safe->size;

        new_suballocation = new_suballocation_safe.release();
        block.suballocations.insert(block.suballocations.iterator_to(*free_suballocation_pick), *new_suballocation);
    }

    if (free_suballocation_pick == block.max_free_suballocation) {
        if (free_suballocation_pick->size == 0) {
            block.max_free_suballocation = second_max_free_suballocation;
        }
        else if (second_max_free_suballocation != nullptr && second_max_free_suballocation->size > free_suballocation_pick->size) {
            block.max_free_suballocation = second_max_free_suballocation;
        }
    }

    if (free_suballocation_pick->size == 0) {
        block.suballocations.erase(block.suballocations.iterator_to(*free_suballocation_pick));
        block.free_suballocations.erase(block.free_suballocations.iterator_to(*free_suballocation_pick));
        m_suballocation_pool->destroy(free_suballocation_pick);
        free_suballocation_pick = nullptr;
    }

    return new_suballocation;
}

gpu_memory::~gpu_memory() {
    if (m_suballocation == nullptr) return;

    auto& block = *m_suballocation->block;
    std::lock_guard lock(block.mutex);
        
    auto suballoc_iter = block.suballocations.iterator_to(*m_suballocation);
    decltype(suballoc_iter) merge_free_first;
    decltype(suballoc_iter) merge_free_last;
        
    if (suballoc_iter != block.suballocations.begin() && std::prev(suballoc_iter)->is_free) {
        merge_free_first = std::prev(suballoc_iter);
    }
    else {
        merge_free_first = suballoc_iter;
    }

    if (std::next(suballoc_iter) != block.suballocations.end() && std::next(suballoc_iter)->is_free) {
        merge_free_last = std::next(suballoc_iter);
    }
    else {
        merge_free_last = suballoc_iter;
    }

    vk::DeviceSize new_free_offset;

    if (merge_free_first != block.suballocations.begin()) {
        auto prev = std::prev(merge_free_first);
        assert(!prev->is_free);
        new_free_offset = prev->offset + prev->size;
    }
    else {
        assert(merge_free_first->offset == 0);
        new_free_offset = 0;
    }

    vk::DeviceSize new_free_size = (merge_free_last->offset + merge_free_last->size) - new_free_offset;

    auto& new_free_allocation = *merge_free_first;
    new_free_allocation.offset = new_free_offset;
    new_free_allocation.size = new_free_size;

    if (new_free_allocation.is_free != true) {
        block.free_suballocations.push_back(new_free_allocation);
        new_free_allocation.is_free = true;
    }

    if (new_free_allocation.size >= block.max_free_suballocation->size) {
        block.max_free_suballocation = &new_free_allocation;
    }

    for (auto iter = std::next(merge_free_first); iter != std::next(merge_free_last); ++iter) {
        if (iter->is_free) {
            block.free_suballocations.erase(block.free_suballocations.iterator_to(*iter));
        }

        block.suballocations.erase(iter);
        block.suballocation_pool->synchronize()->destroy(&*iter);
    }
}

gpu_memory::gpu_memory(gpu_memory&& rhs)
    : m_handle(rhs.m_handle), m_offset(rhs.m_offset), m_size(rhs.m_size), m_suballocation(rhs.m_suballocation) {
    rhs.m_handle = nullptr;
    rhs.m_suballocation = nullptr;
}

gpu_memory& gpu_memory::operator=(gpu_memory && rhs) {
    m_handle = rhs.m_handle;
    m_offset = rhs.m_offset;
    m_size = rhs.m_size;
    m_suballocation = rhs.m_suballocation;

    rhs.m_handle = nullptr;
    rhs.m_suballocation = nullptr;

    return *this;
}

}