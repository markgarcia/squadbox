#ifndef SQUADBOX_GFX_MESH_HPP
#define SQUADBOX_GFX_MESH_HPP

#pragma once

#include <glm/glm.hpp>
#include <gsl/gsl>
#include <boost/preprocessor.hpp>

#include <vector>
#include <algorithm>
#include <type_traits>

namespace squadbox::gfx {

namespace mesh_features {
    struct position {};
    struct normal {};
    struct tex_2d_coord {};
    struct color {};
}

namespace internal {
    template<bool enable>
    class mesh_base_positions {};

    template<>
    class mesh_base_positions<true> {
    public:
        gsl::span<glm::vec3> positions() { return m_positions; }
        gsl::span<const glm::vec3> positions() const { return m_positions; }
        
        void set_positions(gsl::span<const glm::vec3> positions) {
            assert(m_positions.size() == static_cast<std::size_t>(positions.size()));
            m_positions.assign(positions.begin(), positions.end());
        }

        void set_positions(std::vector<glm::vec3>&& positions) {
            assert(m_positions.size() == positions.size());
            m_positions = std::move(positions);
        }

    protected:
        std::vector<glm::vec3> m_positions;
    };

    template<bool enable>
    class mesh_base_normals {};

    template<>
    class mesh_base_normals<true> {
    public:
        gsl::span<glm::vec3> normals() { return m_normals; }
        gsl::span<const glm::vec3> normals() const { return m_normals; }

        void set_normals(gsl::span<const glm::vec3> normals) {
            assert(m_normals.size() == static_cast<std::size_t>(normals.size()));
            m_normals.assign(normals.begin(), normals.end());
        }

        void set_normals(std::vector<glm::vec3>&& normals) {
            assert(m_normals.size() == normals.size());
            m_normals = std::move(normals);
        }

    protected:
        std::vector<glm::vec3> m_normals;
    };

    template<bool enable>
    class mesh_base_tex_2d_coords {};

    template<>
    class mesh_base_tex_2d_coords<true> {
    public:
        gsl::span<glm::vec2> tex_2d_coords() { return m_tex_2d_coords; }
        gsl::span<const glm::vec2> tex_2d_coords() const { return m_tex_2d_coords; }

        void set_tex_2d_coords(gsl::span<const glm::vec2> tex_2d_coords) {
            assert(m_tex_2d_coords.size() == static_cast<std::size_t>(tex_2d_coords.size()));
            m_tex_2d_coords.assign(tex_2d_coords.begin(), tex_2d_coords.end());
        }

        void set_tex_2d_coords(std::vector<glm::vec2>&& tex_2d_coords) {
            assert(m_tex_2d_coords.size() == tex_2d_coords.size());
            m_tex_2d_coords = std::move(tex_2d_coords);
        }

    protected:
        std::vector<glm::vec2> m_tex_2d_coords;
    };

    template<bool enable>
    class mesh_base_colors {};

    template<>
    class mesh_base_colors<true> {
    public:
        gsl::span<glm::vec4> colors() { return m_colors; }
        gsl::span<const glm::vec4> colors() const { return m_colors; }

        void set_colors(gsl::span<const glm::vec4> colors) {
            assert(m_colors.size() == static_cast<std::size_t>(colors.size()));
            m_colors.assign(colors.begin(), colors.end());
        }

        void set_colors(std::vector<glm::vec4>&& colors) {
            assert(m_colors.size() == colors.size());
            m_colors = std::move(colors);
        }

    protected:
        std::vector<glm::vec4> m_colors;
    };
}

template<typename... features>
class mesh :
    public internal::mesh_base_positions<std::disjunction_v<std::is_same<features, mesh_features::position>...>>,
    public internal::mesh_base_normals<std::disjunction_v<std::is_same<features, mesh_features::normal>...>>,
    public internal::mesh_base_tex_2d_coords<std::disjunction_v<std::is_same<features, mesh_features::tex_2d_coord>...>>,
    public internal::mesh_base_colors<std::disjunction_v<std::is_same<features, mesh_features::color>...>> {
public:
    using index_type = std::uint32_t;

    static const bool has_positions = std::disjunction_v<std::is_same<features, mesh_features::position>...>;
    static const bool has_normals = std::disjunction_v<std::is_same<features, mesh_features::normal>...>;
    static const bool has_tex_2d_coords = std::disjunction_v<std::is_same<features, mesh_features::tex_2d_coord>...>;
    static const bool has_colors = std::disjunction_v<std::is_same<features, mesh_features::color>...>;

    mesh() {}

    mesh(std::uint32_t num_vertices) {
        apply_to_features([num_vertices](auto& v) { v.resize(num_vertices); });
    }

    void set_triangle_list_indices(gsl::span<const index_type> indices) {
        assert(indices.size() % 3 == 0);
        m_indices.assign(indices.begin(), indices.end());
    }

    void set_triangle_list_indices(std::vector<index_type>&& indices) {
        assert(indices.size() % 3 == 0);
        m_indices = std::move(indices);
    }

private:
    template<typename func_type>
    void apply_to_features(func_type&& func) {
        if constexpr(has_positions) {
            std::invoke(std::forward<func_type>(func), m_positions);
        }

        if constexpr(has_normals) {
            std::invoke(std::forward<func_type>(func), m_normals);
        }

        if constexpr(has_tex_2d_coords) {
            std::invoke(std::forward<func_type>(func), m_tex_2d_coords);
        }

        if constexpr(has_colors) {
            std::invoke(std::forward<func_type>(func), m_colors);
        }
    }

    std::vector<index_type> m_indices;
};


void calculate_normals(gsl::span<const glm::vec3> positions, gsl::span<const std::uint32_t> triangle_list_indices, const gsl::span<glm::vec3>& output_normals);
std::vector<glm::vec3> calculate_normals(gsl::span<const glm::vec3> positions, gsl::span<const std::uint32_t> triangle_list_indices);


}

#endif