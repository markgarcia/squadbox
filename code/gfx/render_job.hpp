#ifndef SQUADBOX_GFX_RENDER_JOB_HPP
#define SQUADBOX_GFX_RENDER_JOB_HPP

#pragma once

#include <boost/container/small_vector.hpp>
#include <vulkan/vulkan.hpp>

#include <condition_variable>
#include <shared_mutex>
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
        : any_persistent_render_data(std::make_shared<std::unique_ptr<storage_type>>(nullptr)),
          m_local_data(std::move(data)) {}

    persistent_render_data(persistent_render_data&&) = default;

    ~persistent_render_data() {
        if (m_persistent_data_after_destruction.use_count() > 1) {
            *static_cast<std::unique_ptr<storage_type>*>(m_persistent_data_after_destruction.get())
                = std::make_unique<storage_type>(std::move(m_local_data));
        }
    }

    persistent_render_data& operator=(persistent_render_data&& rhs) {
        if (m_persistent_data_after_destruction.use_count() > 1) {
            *static_cast<std::unique_ptr<storage_type>*>(m_persistent_data_after_destruction.get())
                = std::make_unique<storage_type>(std::move(m_local_data));
        }

        m_persistent_data_after_destruction = std::move(rhs.m_persistent_data_after_destruction);
        m_local_data = std::move(rhs.m_local_data);

        return *this;
    }

    storage_type& data() { return m_local_data; }
    const storage_type& data() const { return m_local_data; }

    storage_type* operator->() { return &m_local_data; }
    const storage_type* operator->() const { return &m_local_data; }

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
    struct finish_flag {
        std::mutex mutex;
        std::condition_variable condition_variable;
        bool is_finished;
    };

    template<typename storage_type>
    struct without_render_job_command_buffer_base : render_job_command_buffer_base {
        storage_type data;

        template<typename arg_storage_type>
        without_render_job_command_buffer_base(vk::UniqueCommandBuffer&& command_buffer, arg_storage_type&& arg_data)
            : render_job_command_buffer_base { std::move(command_buffer) }, data(std::forward<arg_storage_type>(arg_data)) {
        }
    };

public:
    render_job() = default;

    render_job(vk::UniqueCommandBuffer&& command_buffer, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : m_command_buffer(command_buffer.get()),
          m_persistent_data(persistent_data.m_persistent_data_after_destruction),
          m_data(std::make_shared<render_job_command_buffer_base>(render_job_command_buffer_base { std::move(command_buffer) })) {
    }

    template<typename storage_type, typename = std::enable_if_t<std::is_base_of_v<render_job_command_buffer_base, storage_type>>>
    render_job(storage_type&& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : m_command_buffer(static_cast<render_job_command_buffer_base&>(data).command_buffer.get()),
          m_persistent_data(persistent_data.m_persistent_data_after_destruction),
          m_data(std::make_shared<std::decay_t<storage_type>>(std::forward<storage_type>(data))) {
    }

    template<typename storage_type, typename = std::enable_if_t<!std::is_base_of_v<render_job_command_buffer_base, storage_type>>>
    render_job(vk::UniqueCommandBuffer&& command_buffer, storage_type&& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : m_command_buffer(command_buffer.get()),
          m_persistent_data(persistent_data.m_persistent_data_after_destruction),
          m_data(std::make_shared<without_render_job_command_buffer_base<std::decay_t<storage_type>>>(std::move(command_buffer), std::forward<storage_type>(data))) {
    }

    render_job(const std::shared_ptr<render_job_command_buffer_base>& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : m_command_buffer(data->command_buffer.get()),
          m_persistent_data(persistent_data.m_persistent_data_after_destruction),
          m_data(data) {
    }

    const vk::CommandBuffer& command_buffer() const { return m_command_buffer; }
    
    bool is_valid() const { return m_data != nullptr; }
    auto use_count() const { return m_data.use_count(); }

    void finish() {
        if (!is_valid()) return;

        std::unique_lock<std::mutex> lock(m_finish_flag->mutex);
        m_finish_flag->is_finished = true;
        lock.unlock();
        m_finish_flag->condition_variable.notify_all();
    }

    void free() {
        m_command_buffer = nullptr;
        m_data.reset();
        m_persistent_data.reset();
    }

    void wait_finish() {
        if (!is_valid() || use_count() == 1) return;
        
        std::unique_lock<std::mutex> lock(m_finish_flag->mutex);
        m_finish_flag->condition_variable.wait(lock, [this] { return m_finish_flag->is_finished; });
    }

protected:
    vk::CommandBuffer m_command_buffer;
    std::shared_ptr<void> m_persistent_data;
    std::shared_ptr<render_job_command_buffer_base> m_data;
    std::shared_ptr<finish_flag> m_finish_flag = std::make_shared<finish_flag>();
};


template<typename storage_type>
class typed_render_job : public render_job {
    using tuple_storage_type = std::tuple<vk::UniqueCommandBuffer, storage_type>;

public:
    static_assert(std::is_base_of_v<render_job_command_buffer_base, storage_type>);

    typed_render_job() = default;

    typed_render_job(vk::UniqueCommandBuffer&& command_buffer, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : render_job() {
        m_data = std::make_shared<storage_type>();
        static_cast<render_job_command_buffer_base*>(m_data.get())->command_buffer = std::move(command_buffer);

        m_persistent_data = persistent_data.m_persistent_data_after_destruction;
    }

    typed_render_job(const storage_type& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : render_job(data, persistent_data) {
    }

    typed_render_job(storage_type&& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : render_job(std::move(data), persistent_data) {
    }

    typed_render_job(const std::shared_ptr<storage_type>& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {})
        : render_job(data, persistent_data) {
    }

    void emplace(vk::UniqueCommandBuffer&& command_buffer, const storage_type& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {}) {
        static_cast<render_job_command_buffer_base*>(m_data.get())->command_buffer = std::move(command_buffer);
        *static_cast<storage_type*>(m_data.get()) = data;

        m_persistent_data = persistent_data.m_persistent_data_after_destruction;
    }

    void emplace(vk::UniqueCommandBuffer&& command_buffer, storage_type&& data, const any_persistent_render_data& persistent_data = no_persistent_render_data {}) {
        static_cast<render_job_command_buffer_base*>(m_data.get())->command_buffer = std::move(command_buffer);
        *static_cast<storage_type*>(m_data.get()) = std::move(data);

        m_persistent_data = persistent_data.m_persistent_data_after_destruction;
    }

    storage_type& data() {
        return const_cast<storage_type&>(const_cast<const typed_render_job*>(this)->data());
    }

    const storage_type& data() const {
        return *static_cast<storage_type*>(m_data.get());
    }

    storage_type* operator->() { return &data(); }
    const storage_type* operator->() const { return &data(); }
};


template<typename storage_type, std::size_t typical_workload>
class render_job_pool {
public:
    typed_render_job<storage_type> create(vk::UniqueCommandBuffer&& command_buffer,
                                          const any_persistent_render_data& persistent_data = no_persistent_render_data {}) {
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
                                          const any_persistent_render_data& persistent_data = no_persistent_render_data {}) {
        return create(std::move(device.allocateCommandBuffersUnique(command_buffer_alloc_info)[0]), persistent_data);
    }

private:
    boost::container::small_vector<typed_render_job<storage_type>, typical_workload> m_jobs;
};

}

#endif