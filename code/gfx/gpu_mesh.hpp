#ifndef SQUADBOX_GFX_GPU_MESH_HPP
#define SQUADBOX_GFX_GPU_MESH_HPP

#include "mesh.hpp"

#include <vulkan/vulkan.hpp>

namespace squadbox::gfx {

namespace internal_gpu_mesh {
    using vertex_position_t = glm::vec3;
    using vertex_normal_t = glm::vec3;
    using vertex_tex_2d_coord_t = glm::vec2;
    using vertex_color_t = glm::vec4;

    // :( can't use mixins / multiple base classes as it makes the type non-standard layout
    template<bool has_position, bool has_normal, bool has_tex_2d_coord, bool has_color>
    struct interleaved_vertex_base;

    #define SQUADBOX_GFX_INTERNAL_GEN_INTERLEAVED_VERTEX_BASE_4(P1, P2, P3, P4)         \
        template<>                                                                      \
        struct interleaved_vertex_base<P1, P2, P3, P4> {                                    \
            BOOST_PP_IF(BOOST_PP_EQUAL(P1, 1), vertex_position_t position;, )           \
            BOOST_PP_IF(BOOST_PP_EQUAL(P2, 1), vertex_normal_t normal;, )               \
            BOOST_PP_IF(BOOST_PP_EQUAL(P3, 1), vertex_tex_2d_coord_t tex_2d_coord;, )   \
            BOOST_PP_IF(BOOST_PP_EQUAL(P4, 1), vertex_color_t color;, )                 \
        };

    #define SQUADBOX_GFX_INTERNAL_GEN_INTERLEAVED_VERTEX_BASE_3(P1, P2, P3) \
        SQUADBOX_GFX_INTERNAL_GEN_INTERLEAVED_VERTEX_BASE_4(P1, P2, P3, 1) \
        SQUADBOX_GFX_INTERNAL_GEN_INTERLEAVED_VERTEX_BASE_4(P1, P2, P3, 0)

    #define SQUADBOX_GFX_INTERNAL_GEN_INTERLEAVED_VERTEX_BASE_2(P1, P2) \
        SQUADBOX_GFX_INTERNAL_GEN_INTERLEAVED_VERTEX_BASE_3(P1, P2, 1) \
        SQUADBOX_GFX_INTERNAL_GEN_INTERLEAVED_VERTEX_BASE_3(P1, P2, 0)

    #define SQUADBOX_GFX_INTERNAL_GEN_INTERLEAVED_VERTEX_BASE_1(P1) \
        SQUADBOX_GFX_INTERNAL_GEN_INTERLEAVED_VERTEX_BASE_2(P1, 1) \
        SQUADBOX_GFX_INTERNAL_GEN_INTERLEAVED_VERTEX_BASE_2(P1, 0)

    #define SQUADBOX_GFX_INTERNAL_GEN_INTERLEAVED_VERTEX_BASE() \
        SQUADBOX_GFX_INTERNAL_GEN_INTERLEAVED_VERTEX_BASE_1(1) \
        SQUADBOX_GFX_INTERNAL_GEN_INTERLEAVED_VERTEX_BASE_1(0)

    SQUADBOX_GFX_INTERNAL_GEN_INTERLEAVED_VERTEX_BASE()

    template<typename... features>
    struct interleaved_vertex
        : interleaved_vertex_base<
            std::disjunction_v<std::is_same<features, mesh_features::position>...>,
            std::disjunction_v<std::is_same<features, mesh_features::normal>...>,
            std::disjunction_v<std::is_same<features, mesh_features::tex_2d_coord>...>,
            std::disjunction_v<std::is_same<features, mesh_features::color>...>
        > {
        interleaved_vertex() = default;
        interleaved_vertex(const interleaved_vertex&) = default;

        static const bool has_position = std::disjunction_v<std::is_same<features, mesh_features::position>...>;
        static const bool has_normal = std::disjunction_v<std::is_same<features, mesh_features::normal>...>;
        static const bool has_tex_2d_coord = std::disjunction_v<std::is_same<features, mesh_features::tex_2d_coord>...>;
        static const bool has_color = std::disjunction_v<std::is_same<features, mesh_features::color>...>;

        template<typename... other_vertex_features, typename = std::enable_if_t<!std::is_same_v<interleaved_vertex, interleaved_vertex<other_vertex_features...>>>>
        operator interleaved_vertex<other_vertex_features...>() const {
            using this_vertex_t = interleaved_vertex;
            using other_vertex_t = interleaved_vertex<other_vertex_features...>;

            other_vertex_t other;
            other.position = this->position;

            if constexpr(this_vertex_t::has_normal) {
                static_assert(other_vertex_t::has_normal);
                other.normal = this->normal;
            }

            if constexpr(this_vertex_t::has_tex_2d_coord) {
                static_assert(other_vertex_t::has_tex_2d_coord);
                other.tex_2d_coord = this->tex_2d_coord;
            }

            if constexpr(this_vertex_t::has_color) {
                static_assert(other_vertex_t::has_color);
                other.color = this->color;
            }
        }

        template<typename feature_t>
        static std::size_t offset_of() {
            static_assert(std::is_standard_layout_v<interleaved_vertex>);

            if constexpr(std::is_same_v<feature_t, mesh_features::position>) {
                return offsetof(interleaved_vertex, position);
            }
            else if constexpr(std::is_same_v<feature_t, mesh_features::normal>) {
                return offsetof(interleaved_vertex, normal);
            }
            else if constexpr(std::is_same_v<feature_t, mesh_features::tex_2d_coord>) {
                return offsetof(interleaved_vertex, tex_2d_coord);
            }
            else if constexpr(std::is_same_v<feature_t, mesh_features::color>) {
                return offsetof(interleaved_vertex, color);
            }
        }
    };
}


struct gpu_mesh_usage {
    enum bits {
        vertex = 1 << 0,
        fragment = 1 << 1
    };
};

#define SQUADBOX_GFX_GEN_MESH_FEATURE(feature_name, default_vulkan_format) \
template<unsigned long long arg_usage, std::uint32_t arg_glsl_location = std::numeric_limits<std::uint32_t>::max(), vk::Format arg_vulkan_format = default_vulkan_format> \
struct gpu_mesh_##feature_name { \
    static constexpr auto usage = arg_usage; \
    static constexpr auto glsl_location = arg_glsl_location; \
    static constexpr auto vulkan_format = arg_vulkan_format; \
    \
    static_assert(((usage | gpu_mesh_usage::vertex | gpu_mesh_usage::fragment) ^ (gpu_mesh_usage::vertex | gpu_mesh_usage::fragment)) == 0, \
                  "Invalid mesh feature usage value. Please only use values from gpu_mesh_usage."); \
    static_assert(usage != 0, \
                  "Invalid mesh feature usage value."); \
    \
    using mesh_feature = mesh_features::feature_name; \
};

SQUADBOX_GFX_GEN_MESH_FEATURE(position    , vk::Format::eR32G32B32Sfloat)
SQUADBOX_GFX_GEN_MESH_FEATURE(normal      , vk::Format::eR32G32B32Sfloat)
SQUADBOX_GFX_GEN_MESH_FEATURE(tex_2d_coord, vk::Format::eR32G32Sfloat)
SQUADBOX_GFX_GEN_MESH_FEATURE(color       , vk::Format::eR32G32B32A32Sfloat)


namespace internal_gpu_mesh {
    template<typename... Ts>
    struct types {};


    template<typename T>
    constexpr bool has_duplicates_impl = false;

    template<typename T, typename... Ts>
    constexpr bool has_duplicates_impl<types<T, Ts...>> = (std::is_same_v<T, Ts> || ...) || has_duplicates_impl<types<Ts...>>;

    template<typename... Ts>
    constexpr bool has_duplicates = has_duplicates_impl<types<Ts...>>;


    template<typename T, unsigned long long new_usage>
    struct respecialize {};

    template<template <unsigned long long, std::uint32_t, vk::Format> typename T, unsigned long long usage, unsigned long long new_usage, std::uint32_t glsl_location, vk::Format format>
    struct respecialize<T<usage, glsl_location, format>, new_usage> {
        using type = T<new_usage, glsl_location, format>;
    };


    template<typename T>
    struct is_gpu_mesh_feature : std::false_type {};

    template<template <unsigned long long, std::uint32_t, vk::Format> typename T, unsigned long long usage, std::uint32_t glsl_location, vk::Format format>
    struct is_gpu_mesh_feature<T<usage, glsl_location, format>> {
        static constexpr bool value
            =  std::is_same_v<T<gpu_mesh_usage::vertex, glsl_location, format>, gpu_mesh_position    <gpu_mesh_usage::vertex, glsl_location, format>>
            || std::is_same_v<T<gpu_mesh_usage::vertex, glsl_location, format>, gpu_mesh_normal      <gpu_mesh_usage::vertex, glsl_location, format>>
            || std::is_same_v<T<gpu_mesh_usage::vertex, glsl_location, format>, gpu_mesh_tex_2d_coord<gpu_mesh_usage::vertex, glsl_location, format>>
            || std::is_same_v<T<gpu_mesh_usage::vertex, glsl_location, format>, gpu_mesh_color       <gpu_mesh_usage::vertex, glsl_location, format>>;
    };


    template<unsigned long long usage, typename T>
    constexpr int count_usage_impl = 0;

    template<unsigned long long usage, typename feature, typename... features>
    constexpr int count_usage_impl<usage, types<feature, features...>>
        = static_cast<int>(feature::usage == usage) + count_usage_impl<usage, types<features...>>;

    template<unsigned long long usage, typename... features>
    constexpr int count_usage = count_usage_impl<usage, types<features...>>;


    template<unsigned long long usage_needed, typename feature>
    struct get_mesh_feature_for_interleaved_vertex {
        using type = void;
    };

    template<unsigned long long usage_needed, template <unsigned long long, std::uint32_t, vk::Format> typename feature, std::uint32_t glsl_location, vk::Format format>
    struct get_mesh_feature_for_interleaved_vertex<usage_needed, feature<usage_needed, glsl_location, format>> {
        using type = typename feature<usage_needed, glsl_location, format>::mesh_feature;
    };


    template<typename feature, typename search_features, typename orig_features>
    struct get_glsl_location_impl;

    template<typename feature, typename... rfeatures, typename... orig_features>
    struct get_glsl_location_impl<feature, types<feature, rfeatures...>, types<orig_features...>> {
        static constexpr auto glsl_location
            = feature::glsl_location == std::numeric_limits<std::uint32_t>::max()
                ? 0
                : feature::glsl_location;
    };

    template<typename feature, typename lfeature, typename... rfeatures, typename... orig_features>
    struct get_glsl_location_impl<feature, types<lfeature, feature, rfeatures...>, types<orig_features...>> {
        static constexpr auto glsl_location
            = feature::glsl_location == std::numeric_limits<std::uint32_t>::max()
                ? get_glsl_location_impl<lfeature, types<orig_features...>, types<orig_features...>>::glsl_location + 1
                : feature::glsl_location;
    };

    template<typename feature, typename lfeature, typename... rfeatures, typename... orig_features>
    struct get_glsl_location_impl<feature, types<lfeature, rfeatures...>, types<orig_features...>> {
        static constexpr auto glsl_location = get_glsl_location_impl<feature, types<rfeatures...>, types<orig_features...>>::glsl_location;
    };


    template<typename feature, typename... features>
    struct get_glsl_location {
        static constexpr auto glsl_location
            = feature::glsl_location == std::numeric_limits<std::uint32_t>::max()
                ? get_glsl_location_impl<feature, types<features...>, types<features...>>::glsl_location
                : feature::glsl_location;
    };
}


template<typename... features>
class gpu_mesh {
public:
    static_assert(sizeof...(features) != 0);
    static_assert(std::conjunction_v<internal_gpu_mesh::is_gpu_mesh_feature<features>...>);
    static_assert(!internal_gpu_mesh::has_duplicates<typename internal_gpu_mesh::respecialize<features, gpu_mesh_usage::vertex>::type...>);

    static constexpr int num_vertex_shader_only_features = internal_gpu_mesh::count_usage<gpu_mesh_usage::vertex, features...>;
    static constexpr int num_fragment_shader_only_features = internal_gpu_mesh::count_usage<gpu_mesh_usage::fragment, features...>;

private:
    template<bool enable>
    struct common_buffer_base {};

    template<>
    struct common_buffer_base<true> {
        using common_buffer_element_type = internal_gpu_mesh::interleaved_vertex<
            typename internal_gpu_mesh::get_mesh_feature_for_interleaved_vertex<gpu_mesh_usage::vertex | gpu_mesh_usage::fragment, features>::type...
        >;

        vk::UniqueBuffer common_buffer;
    };

    template<bool enable>
    struct vertex_shader_only_buffer_base {};

    template<>
    struct vertex_shader_only_buffer_base<true> {
        using vertex_shader_only_buffer_element_type = internal_gpu_mesh::interleaved_vertex<
            typename internal_gpu_mesh::get_mesh_feature_for_interleaved_vertex<gpu_mesh_usage::vertex, features>::type...
        >;

        vk::UniqueBuffer vertex_shader_only_buffer;
    };

    template<bool enable>
    struct fragment_shader_only_buffer_base {};

    template<>
    struct fragment_shader_only_buffer_base<true> {
        using fragment_shader_only_buffer_element_type = internal_gpu_mesh::interleaved_vertex<
            typename internal_gpu_mesh::get_mesh_feature_for_interleaved_vertex<gpu_mesh_usage::fragment, features>::type...
        >;

        vk::UniqueBuffer fragment_shader_only_buffer;
    };

    struct vertex_buffers_storage
        : common_buffer_base<(sizeof...(features) - num_vertex_shader_only_features - num_fragment_shader_only_features) != 0>,
          vertex_shader_only_buffer_base<num_vertex_shader_only_features != 0>,
          fragment_shader_only_buffer_base<num_fragment_shader_only_features != 0> {
        static constexpr bool has_common_buffer = (sizeof...(features) - num_vertex_shader_only_features - num_fragment_shader_only_features) != 0;
        static constexpr bool has_vertex_shader_only_buffer = num_vertex_shader_only_features != 0;
        static constexpr bool has_fragment_shader_only_buffer = num_fragment_shader_only_features != 0;
    };

public:
    using index_type = std::uint32_t;

    /*template<typename... input_mesh_features>
    static gpu_mesh create(const mesh<input_mesh_features...>& mesh, const vk::Device& device,
                           const vk::PhysicalDeviceMemoryProperties& device_memory_props) {
        gpu_mesh gpu_mesh;

        gpu_mesh.m_vertex_buffer = [](const vk::Device& device, const vk::DeviceSize required_vertex_buffer_size) {
            vk::BufferCreateInfo vertex_buffer_ci;
            vertex_buffer_ci
                .setSize(required_vertex_buffer_size)
                .setUsage(vk::BufferUsageFlagBits::eVertexBuffer)
                .setSharingMode(vk::SharingMode::eExclusive);

            return device.createBufferUnique(vertex_buffer_ci);
        }(device, mesh.vertices().size_bytes());

        gpu_mesh.m_index_buffer = [](const vk::Device& device, const vk::DeviceSize required_index_buffer_size) {
            vk::BufferCreateInfo index_buffer_ci;
            index_buffer_ci
                .setSize(required_index_buffer_size)
                .setUsage(vk::BufferUsageFlagBits::eIndexBuffer)
                .setSharingMode(vk::SharingMode::eExclusive);

            return device.createBufferUnique(index_buffer_ci);
        }(device, mesh.triangle_list_indices().size_bytes());

        auto [ vertex_index_buffers_memory, vertex_index_buffers_memory_size,
               vertex_buffer_offset, index_buffer_offset ]
            = [](const vk::Device& device, const vk::PhysicalDeviceMemoryProperties& device_memory_props,
                 const vk::Buffer& vertex_buffer, const vk::Buffer& index_buffer) {
            const auto vertex_buffer_memory_reqs = device.getBufferMemoryRequirements(vertex_buffer);
            const auto index_buffer_memory_reqs = device.getBufferMemoryRequirements(index_buffer);

            const vk::DeviceSize vertex_buffer_offset = 0;
            const vk::DeviceSize index_buffer_offset =
                vertex_buffer_memory_reqs.size
                + index_buffer_memory_reqs.alignment
                - (vertex_buffer_memory_reqs.size % index_buffer_memory_reqs.alignment);

            const auto required_device_memory_size = vertex_buffer_offset + index_buffer_offset + index_buffer_memory_reqs.size;

            const auto vertex_buffer_memory_type_index = vk_utils::get_memory_type_index(device_memory_props, vertex_buffer_memory_reqs, vk::MemoryPropertyFlagBits::eHostVisible);
            const auto index_buffer_memory_type_index = vk_utils::get_memory_type_index(device_memory_props, index_buffer_memory_reqs, vk::MemoryPropertyFlagBits::eHostVisible);

            assert(vertex_buffer_memory_type_index == index_buffer_memory_type_index);

            vk::MemoryAllocateInfo memory_alloc_info;
            memory_alloc_info
                .setAllocationSize(required_device_memory_size)
                .setMemoryTypeIndex(vertex_buffer_memory_type_index);

            return std::make_tuple(device.allocateMemoryUnique(memory_alloc_info), required_device_memory_size,
                                    vertex_buffer_offset, index_buffer_offset);
        }(device, device_memory_props, gpu_mesh.vertex_buffer.get(), gpu_mesh.index_buffer.get());

        device.bindBufferMemory(gpu_mesh.m_vertex_buffer.get(), gpu_mesh.m_vertex_index_buffers_memory.get(), vertex_buffer_offset);
        device.bindBufferMemory(gpu_mesh.m_index_buffer.get(), gpu_mesh.m_vertex_index_buffers_memory.get(), index_buffer_offset);

        {
            const void* mapped_memory_ptr = device.mapMemory(gpu_mesh.m_vertex_index_buffers_memory.get(), 0, vertex_index_buffers_memory_size);
            auto vertex_dst = static_cast<typename gpu_mesh<gpu_mesh_features...>::vertex_t*>(mapped_memory_ptr + vertex_buffer_offset);
            auto index_dst = static_cast<typename gpu_mesh<gpu_mesh_features...>::index_t*>(mapped_memory_ptr + index_buffer_offset);

            std::copy_n(mesh.vertices().data(), mesh.vertices().size(), vertex_dst);
            std::copy_n(mesh.triangle_list_indices().data(), mesh.triangle_list_indices().size(), index_dst);

            vk::MappedMemoryRange mapped_memory;
            mapped_memory
                .setMemory(vertex_index_buffers_memory)
                .setOffset(0)
                .setSize(vertex_index_buffers_memory_size);

            device.flushMappedMemoryRanges({ mapped_memory });
            device.unmapMemory(gpu_mesh.m_vertex_index_buffers_memory);
        }

        return gpu_mesh;
    }*/


    static constexpr int num_vertex_buffers = vertex_buffers_storage::has_common_buffer + vertex_buffers_storage::has_vertex_shader_only_buffer + vertex_buffers_storage::has_fragment_shader_only_buffer;

    static constexpr auto vertex_input_binding_desc() {
        std::array<vk::VertexInputBindingDescription, num_vertex_buffers> vertex_input_binding_desc;
        std::uint32_t index = 0;

        if constexpr(vertex_buffers_storage::has_common_buffer) {
            vertex_input_binding_desc[index]
                .setBinding(index)
                .setStride(sizeof(vertex_buffers_storage::common_buffer_element_type))
                .setInputRate(vk::VertexInputRate::eVertex);

            ++index;
        }

        if constexpr(vertex_buffers_storage::has_vertex_shader_only_buffer) {
            vertex_input_binding_desc[index]
                .setBinding(index)
                .setStride(sizeof(vertex_buffers_storage::vertex_shader_only_buffer_element_type))
                .setInputRate(vk::VertexInputRate::eVertex);

            ++index;
        }

        if constexpr(vertex_buffers_storage::has_fragment_shader_only_buffer) {
            vertex_input_binding_desc[index]
                .setBinding(index)
                .setStride(sizeof(vertex_buffers_storage::fragment_shader_only_buffer_element_type))
                .setInputRate(vk::VertexInputRate::eVertex);

            ++index;
        }

        return vertex_input_binding_desc;
    }

    static auto vertex_input_attr_desc() {
        auto get_vert_input_attr_desc = [](auto feature) {
            using feature_t = std::decay_t<decltype(feature)>;
            vk::VertexInputAttributeDescription desc;

            if constexpr(feature_t::usage == (gpu_mesh_usage::vertex | gpu_mesh_usage::fragment)) {
                desc.setBinding(0);
                desc.setOffset(vertex_buffers_storage::common_buffer_element_type::template offset_of<typename feature_t::mesh_feature>());
            }
            else if constexpr(feature_t::usage == gpu_mesh_usage::vertex) {
                desc.setBinding(static_cast<std::uint32_t>(vertex_buffers_storage::has_common_buffer));
                desc.setOffset(vertex_buffers_storage::vertex_shader_only_buffer_element_type::template offset_of<typename feature_t::mesh_feature>());
            }
            else if constexpr(feature_t::usage == gpu_mesh_usage::fragment) {
                desc.setBinding(  static_cast<std::uint32_t>(vertex_buffers_storage::has_common_buffer)
                                + static_cast<std::uint32_t>(vertex_buffers_storage::has_vertex_shader_only_buffer));
                desc.setOffset(vertex_buffers_storage::fragment_shader_only_buffer_element_type::template offset_of<typename feature_t::mesh_feature>());
            }

            desc.setFormat(feature_t::vulkan_format);
            desc.setLocation(internal_gpu_mesh::get_glsl_location<feature_t, features...>::glsl_location);

            return desc;
        };

        std::array<vk::VertexInputAttributeDescription, sizeof...(features)> vertex_input_attr_desc = {
            get_vert_input_attr_desc(features {})...
        };

        return vertex_input_attr_desc;
    }

    std::array<vk::Buffer, num_vertex_buffers> vertex_buffers() const;
    std::array<vk::DeviceSize, num_vertex_buffers> vertex_buffer_offsets() const;

    const vk::Buffer& index_buffer() const { return m_index_buffer.get(); }

    index_type index_count() const { return m_index_count; }

private:
    vertex_buffers_storage m_vertex_buffers;
    vk::UniqueBuffer m_index_buffer;
    vk::UniqueDeviceMemory m_vertex_index_buffers_memory;
    index_type m_index_count;
};

}

#endif