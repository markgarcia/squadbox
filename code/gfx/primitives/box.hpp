#ifndef SQUADBOX_GFX_PRIMITIVES_BOX_HPP
#define SQUADBOX_GFX_PRIMITIVES_BOX_HPP

#pragma once

#include "../mesh.hpp"

#include <glm/glm.hpp>
#include <gsl/gsl>

#include <array>

namespace squadbox::gfx::primitives {

class box {
public:
    box() = default;

    box(const glm::vec3& min_corner, const glm::vec3& max_corner)
        : m_min_corner(min_corner), m_max_corner(max_corner) {}

    glm::vec3& min_corner() { return m_min_corner; }
    glm::vec3& max_corner() { return m_max_corner; }
    const glm::vec3& min_corner() const { return m_min_corner; }
    const glm::vec3& max_corner() const { return m_max_corner; }

    mesh<mesh_features::position, mesh_features::normal> create_mesh() const {
        std::array<glm::vec3, 8> positions;

                                                                             // [Front]
        positions[0] = { m_max_corner.x, m_max_corner.y, m_max_corner.z };   //       3---0
        positions[1] = { m_max_corner.x, m_min_corner.y, m_max_corner.z };   // left  |   |
        positions[2] = { m_min_corner.x, m_min_corner.y, m_max_corner.z };   //       |   |
        positions[3] = { m_min_corner.x, m_max_corner.y, m_max_corner.z };   //       2---1 
                                                                             //       bottom
                                                                                 
                                                                             // [Top]
        positions[4] = { m_min_corner.x, m_max_corner.y, m_min_corner.z };   //       4---5
        positions[5] = { m_max_corner.x, m_max_corner.y, m_min_corner.z };   // left  |   |
                                                                             //       |   |
                                                                             //       3---0
                                                                             //       front
                                                                                 
                                                                             // [Right]
        positions[6] = { m_max_corner.x, m_min_corner.y, m_min_corner.z };   //       0---5
                                                                             // front |   |
                                                                             //       |   |
                                                                             //       1---6
                                                                             //       bottom

                                                                             // [Bottom]
        positions[7] = { m_min_corner.x, m_min_corner.y, m_min_corner.z };   //       2---1
                                                                             // left  |   |
                                                                             //       |   |
                                                                             //       7---6
                                                                             //       back

                                                                             // [Left]
                                                                             //       4---3
                                                                             // back  |   |
                                                                             //       |   |
                                                                             //       7---2
                                                                             //       bottom

                                                                             // [Back]
                                                                             //       5---4
                                                                             // right |   |
                                                                             //       |   |
                                                                             //       6---7
                                                                             //       bottom

        constexpr std::array<mesh<>::index_type, 6 * 6> indices = {
            // Front
            0, 1, 2,
            0, 2, 3,

            // Top,
            0, 3, 4,
            0, 4, 5,

            // Right
            0, 5, 6,
            0, 6, 1,

            // Bottom
            1, 6, 7,
            1, 7, 2,

            // Left
            3, 2, 7,
            3, 7, 4,

            // Back
            4, 7, 6,
            4, 6, 5
        };

        mesh<mesh_features::position, mesh_features::normal> mesh;
        mesh.set_positions(positions);
        mesh.set_normals(calculate_normals(positions, indices));
        mesh.set_triangle_list_indices(indices);

        return mesh;
    }
    
private:
    glm::vec3 m_min_corner = {};
    glm::vec3 m_max_corner = {};
};

}

#endif