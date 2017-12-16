#ifndef SQUADBOX_GFX_RENDER_JOB_HPP
#define SQUADBOX_GFX_RENDER_JOB_HPP

#pragma once

#include <boost/container/small_vector.hpp>
#include <vulkan/vulkan.hpp>

#include <memory>
#include <tuple>

namespace squadbox::gfx {

struct render_job_command_buffer_base {
    vk::UniqueCommandBuffer command_buffer;
};

class render_job {
public:
    render_job() = default;

    render_job(vk::UniqueCommandBuffer&& command_buffer)
        : m_command_buffer(command_buffer.get()),
          m_data(std::make_shared<vk::UniqueCommandBuffer>(std::move(command_buffer))) {
    }

    template<typename data_type, typename = std::enable_if_t<!std::is_base_of_v<render_job_command_buffer_base, data_type>>>
    render_job(vk::UniqueCommandBuffer&& command_buffer, data_type&& data) 
        : m_command_buffer(command_buffer.get()),
          m_data(std::make_shared<std::tuple<vk::UniqueCommandBuffer, std::decay_t<data_type>>>(std::move(command_buffer), std::forward<data_type>(data))) {
    }

    template<typename data_type, typename = std::enable_if_t<std::is_base_of_v<render_job_command_buffer_base, data_type>>>
    render_job(data_type&& data)
        : m_command_buffer(static_cast<render_job_command_buffer_base&>(data).command_buffer),
          m_data(std::make_shared<std::decay_t<data_type>>(std::forward<data_type>(data))) {
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
    std::shared_ptr<void> m_data;
};

template<typename data_type>
class typed_render_job : public render_job {
public:
    typed_render_job() = default;

    typed_render_job(vk::UniqueCommandBuffer&& command_buffer)
        : render_job() {
        if constexpr(std::is_base_of_v<render_job_command_buffer_base, data_type>) {
            m_command_buffer = command_buffer.get();

            auto data = std::make_shared<data_type>();
            static_cast<render_job_command_buffer_base*>(data.get())->command_buffer = std::move(command_buffer);
            m_data = std::move(data);
        }
        else {
            m_command_buffer = command_buffer.get();
            m_data = std::make_shared<std::tuple<vk::UniqueCommandBuffer, data_type>>(std::move(command_buffer), data_type());
        }
    }

    template<typename = std::enable_if_t<!std::is_base_of_v<render_job_command_buffer_base, data_type>>>
    typed_render_job(vk::UniqueCommandBuffer&& command_buffer, const data_type& data)
        : render_job(std::move(command_buffer), data) {
    }

    template<typename = std::enable_if_t<!std::is_base_of_v<render_job_command_buffer_base, data_type>>>
    typed_render_job(vk::UniqueCommandBuffer&& command_buffer, data_type&& data)
        : render_job(std::move(command_buffer), std::move(data)) {
    }

    template<typename = std::enable_if_t<std::is_base_of_v<render_job_command_buffer_base, data_type>>>
    typed_render_job(const data_type& data)
        : render_job(data) {
    }

    template<typename = std::enable_if_t<std::is_base_of_v<render_job_command_buffer_base, data_type>>>
    typed_render_job(data_type&& data)
        : m_command_buffer(std::move(data)) {
    }

    data_type& data() {
        if constexpr(std::is_base_of_v<render_job_command_buffer_base, data_type>) {
            return *static_cast<data_type*>(m_data.get());
        }
        else {
            return std::get<data_type>(*static_cast<std::tuple<vk::UniqueCommandBuffer, data_type>*>(m_data.get()));
        }
    }
};

template<typename data_type, std::size_t typical_workload>
class render_job_pool {
public:
    typed_render_job<data_type> create(const vk::Device& device, const vk::CommandBufferAllocateInfo& command_buffer_alloc_info) {
        auto job = [this, &device, &command_buffer_alloc_info]() {
            for (auto& job : m_jobs) {
                if (job.use_count() == 1) {
                    job = { std::move(device.allocateCommandBuffersUnique(command_buffer_alloc_info)[0]), std::move(job.data()) };
                    return job;
                }
            }

            return m_jobs.emplace_back(std::move(device.allocateCommandBuffersUnique(command_buffer_alloc_info)[0]));
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

private:
    boost::container::small_vector<typed_render_job<data_type>, typical_workload> m_jobs;
};

}

#endif