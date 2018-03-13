#include "mesh.hpp"

namespace squadbox::gfx {

void calculate_normals(gsl::span<const glm::vec3> positions, gsl::span<const std::uint32_t> triangle_list_indices, const gsl::span<glm::vec3>& output_normals) {
    assert(triangle_list_indices.size() % 3 == 0);

    std::vector<std::tuple<std::uint32_t /* a */, std::uint32_t /* b */, std::uint32_t /* c */, glm::vec3 /* normal */, float /* area */>> triangles;
    triangles.reserve(triangle_list_indices.size() / 3);

    for (std::ptrdiff_t i = 0; i < triangle_list_indices.size(); i += 3) {
        const auto& a = positions[triangle_list_indices[i + 0]];
        const auto& b = positions[triangle_list_indices[i + 1]];
        const auto& c = positions[triangle_list_indices[i + 2]];
        const auto cross_product = glm::cross(c - a, b - a);

        triangles.emplace_back(
            triangle_list_indices[i + 0],
            triangle_list_indices[i + 1],
            triangle_list_indices[i + 2],
            glm::normalize(cross_product),
            glm::length(cross_product) / 2.0f
        );
    }

    for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(positions.size()); ++index) {
        const auto& position = positions[index];
        glm::vec3 vert_normal {};

        for (std::ptrdiff_t n = 0; n < triangle_list_indices.size(); ++n) {
            if (index != triangle_list_indices[n]) break;
            
            const auto triangle_idx = n / 3;
            const auto vert_tri_idx = n - triangle_idx;            
            const auto& triangle = triangles[triangle_idx];

            const auto& face_normal = std::get<3>(triangle);
            const auto& area = std::get<4>(triangle);

            const auto a = positions[vert_tri_idx == 0 ? std::get<1>(triangle) : vert_tri_idx == 1 ? std::get<2>(triangle) : std::get<0>(triangle)] - position;
            const auto b = positions[vert_tri_idx == 0 ? std::get<2>(triangle) : vert_tri_idx == 1 ? std::get<0>(triangle) : std::get<1>(triangle)] - position;
            const auto angle = glm::acos(glm::dot(a, b));

            vert_normal += face_normal * area * angle;
        }

        output_normals[index] = glm::normalize(vert_normal);
    }
}

std::vector<glm::vec3> calculate_normals(gsl::span<const glm::vec3> positions, gsl::span<const std::uint32_t> triangle_list_indices) {
    std::vector<glm::vec3> normals;
    normals.reserve(positions.size());
    
    calculate_normals(positions, triangle_list_indices, normals);

    return normals;
}

}