#ifndef SQUADBOX_GFX_RENDER_JOB_HPP
#define SQUADBOX_GFX_RENDER_JOB_HPP

#pragma once

#include <boost/container/small_vector.hpp>
#include <vulkan/vulkan.hpp>

#include <memory>
#include <tuple>

namespace squadbox::gfx {

class any_persistent_render_data {
public:
    friend class render_job;

    template<typename storage_type>
    friend class typed_render_job;

protected:
    any_persistent_render_data(std::shared_ptr<void>&& persistent_data_after_destruction)
        : m_persistent_data_after_destruction(std::move(persistent_data_after_destruction)) {
    }

    std::shared_ptr<void> m_persistent_data_after_destruction;
};


class no_persistent_render_data : public any_persistent_render_data {
public:
    no_persistent_render_data() : any_persistent_render_data(nullptr) {}
};


template<typename storage_type>
class persistent_render_data : public any_persistent_render_data {
public:
    persistent_render_data()
        : any_persistent_render_data(std::make_shared<std::unique_ptr<storage_type>>(nullptr)) {}

    persistent_render_data(storage_type&& data)
        : m_local_data(std::move(data)),
          any_persistent_render_data(std::make_shared<std::unique_ptr<storage_type>>(nullptr)) {}

    persistent_render_data(persistent_render_data&&) = default;

    ~persistent_render_data() {
        if (m_persistent_data_after_destruction.use_count() > 1) {
            *static_cast<std::unique_ptr<storage_type>*>(m_persistent_data_after_destruction.get())
                = std::make_unique<storage_type>(std::move(m_local_data));
        }
    }

    storage_type& data() { return m_local_data; }
    const storage_type& data() const { return m_local_data; }

    storage_type* operator->() { return &m_local_data; }
    const storage_type* operator->() const { return m_local_data; }

private:
    storage_type m_local_data;
};


template<>
class persistent_render_data<void> : public any_persistent_render_data {
public:
    persistent_render_data() : any_persistent_render_data(nullptr) {}
};


struct render_job_command_buffer_base {
    vk::UniqueCommandBuffer command_buffer;
};


class render_job {
public:
    render_job() = default;

    render_job(vk::UniqueCommandBuffer&& command_buffer, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : m_command_buffer(command_buffer.get()),
          m_persistent_data(persistent_data.m_persistent_data_after_destruction),
          m_data(std::make_shared<vk::UniqueCommandBuffer>(std::move(command_buffer))) {
    }

    template<typename storage_type, typename = std::enable_if_t<!std::is_base_of_v<render_job_command_buffer_base, storage_type>>>
    render_job(vk::UniqueCommandBuffer&& command_buffer, storage_type&& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : m_command_buffer(command_buffer.get()),
          m_persistent_data(persistent_data.m_persistent_data_after_destruction),
          m_data(std::make_shared<std::tuple<vk::UniqueCommandBuffer, std::decay_t<storage_type>>>(std::move(command_buffer), std::forward<storage_type>(data))) {
    }

    template<typename storage_type, typename = std::enable_if_t<std::is_base_of_v<render_job_command_buffer_base, storage_type>>>
    render_job(storage_type&& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : m_command_buffer(static_cast<render_job_command_buffer_base&>(data).command_buffer),
          m_persistent_data(persistent_data.m_persistent_data_after_destruction),
          m_data(std::make_shared<std::decay_t<storage_type>>(std::forward<storage_type>(data))) {
    }

    const vk::CommandBuffer& command_buffer() const { return m_command_buffer; }
    
    bool is_valid() const { return m_data != nullptr; }
    auto use_count() const { return m_data.use_count(); }

    void free() { 
        m_command_buffer = nullptr;
        m_data.reset();
    }

protected:
    vk::CommandBuffer m_command_buffer;
    std::shared_ptr<void> m_persistent_data;
    std::shared_ptr<void> m_data;
};


template<typename storage_type>
class typed_render_job : public render_job {
    using tuple_storage_type = std::tuple<vk::UniqueCommandBuffer, storage_type>;

public:
    typed_render_job() = default;

    typed_render_job(vk::UniqueCommandBuffer&& command_buffer, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : render_job() {
        if constexpr(std::is_base_of_v<render_job_command_buffer_base, storage_type>) {
            m_command_buffer = command_buffer.get();

            auto data = std::make_shared<storage_type>();
            static_cast<render_job_command_buffer_base*>(data.get())->command_buffer = std::move(command_buffer);
            m_data = std::move(data);
        }
        else {
            m_command_buffer = command_buffer.get();
            m_data = std::make_shared<tuple_storage_type>(std::move(command_buffer), storage_type());
        }

        m_persistent_data = persistent_data.m_persistent_data_after_destruction;
    }

    template<typename = std::enable_if_t<!std::is_base_of_v<render_job_command_buffer_base, storage_type>>>
    typed_render_job(vk::UniqueCommandBuffer&& command_buffer, const storage_type& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : render_job(std::move(command_buffer), data, persistent_data) {
    }

    template<typename = std::enable_if_t<!std::is_base_of_v<render_job_command_buffer_base, storage_type>>>
    typed_render_job(vk::UniqueCommandBuffer&& command_buffer, storage_type&& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : render_job(std::move(command_buffer), std::move(data), persistent_data) {
    }

    template<typename = std::enable_if_t<std::is_base_of_v<render_job_command_buffer_base, storage_type>>>
    typed_render_job(const storage_type& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : render_job(data, persistent_data) {
    }

    template<typename = std::enable_if_t<std::is_base_of_v<render_job_command_buffer_base, storage_type>>>
    typed_render_job(storage_type&& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : render_job(std::move(data), persistent_data) {
    }

    void emplace(vk::UniqueCommandBuffer&& command_buffer, const storage_type& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {}) {
        m_persistent_data = persistent_data.m_persistent_data_after_destruction;

        if constexpr(std::is_base_of_v<render_job_command_buffer_base, storage_type>) {
            m_command_buffer = command_buffer.get();

            static_cast<render_job_command_buffer_base*>(m_data.get())->command_buffer = std::move(command_buffer);
            *static_cast<storage_type*>(m_data.get()) = data;
        }
        else {
            m_command_buffer = command_buffer.get();
            *static_cast<tuple_storage_type*>(m_data.get()) = std::make_tuple(std::move(command_buffer), data);
        }
    }

    void emplace(vk::UniqueCommandBuffer&& command_buffer, storage_type&& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {}) {
        m_persistent_data = persistent_data.m_persistent_data_after_destruction;

        if constexpr(std::is_base_of_v<render_job_command_buffer_base, storage_type>) {
            m_command_buffer = command_buffer.get();

            static_cast<render_job_command_buffer_base*>(m_data.get())->command_buffer = std::move(command_buffer);
            *static_cast<storage_type*>(m_data.get()) = std::move(data);
        }
        else {
            m_command_buffer = command_buffer.get();
            *static_cast<tuple_storage_type*>(m_data.get()) = std::make_tuple(std::move(command_buffer), std::move(data));
        }
    }

    storage_type& data() {
        return const_cast<storage_type&>(const_cast<const typed_render_job*>(this)->data());
    }

    const storage_type& data() const {
        if constexpr(std::is_base_of_v<render_job_command_buffer_base, storage_type>) {
            return *static_cast<storage_type*>(m_data.get());
        }
        else {
            return std::get<storage_type>(*static_cast<std::tuple<vk::UniqueCommandBuffer, storage_type>*>(m_data.get()));
        }
    }

    storage_type* operator->() { return &data(); }
    const storage_type* operator->() const { return &data(); }
};


template<typename storage_type, std::size_t typical_workload, typename persistence_data_type = void>
class render_job_pool {
public:
    typed_render_job<storage_type> create(vk::UniqueCommandBuffer&& command_buffer,
                                          const persistent_render_data<persistence_data_type>& persistent_data = persistent_render_data<void> {}) {
        auto job = [&]() {
            for (auto& job : m_jobs) {
                if (job.use_count() == 1) {
                    job.emplace(std::move(command_buffer), std::move(job.data()), persistent_data);
                    return job;
                }
            }

            return m_jobs.emplace_back(std::move(command_buffer), persistent_data);
        }();

        // Cleanup excess jobs > typical_workload
        if (m_jobs.size() > typical_workload) {
            m_jobs.erase(
                std::remove_if(m_jobs.begin() + typical_workload, m_jobs.end(), [](const auto& job) {
                return job.use_count() == 1;
            }),
                m_jobs.end());

            m_jobs.shrink_to_fit();
        }

        return job;
    }

    typed_render_job<storage_type> create(const vk::Device& device, const vk::CommandBufferAllocateInfo& command_buffer_alloc_info,
                                          const persistent_render_data<persistence_data_type>& persistent_data = persistent_render_data<void> {}) {
        return create(std::move(device.allocateCommandBuffersUnique(command_buffer_alloc_info)[0]), persistent_data);
    }

private:
    boost::container::small_vector<typed_render_job<storage_type>, typical_workload> m_jobs;
};

}

#endif