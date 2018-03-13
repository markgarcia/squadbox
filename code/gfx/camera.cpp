#include "camera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace squadbox::gfx {

void camera::orient(const glm::vec3& camera_position, const glm::vec3& look_position, const glm::vec3& up_direction) {
    m_view_matrix = glm::lookAt(camera_position, look_position, up_direction);
}

void camera::set_perspective(float fov_degrees, float aspect_ratio) {
    m_projection_matrix = glm::perspective(glm::radians(fov_degrees), aspect_ratio, 0.1f, 10.0f);
    m_projection_matrix[1][1] *= -1;    // because vulkan
}

const glm::mat4& camera::view_matrix() const {
    return m_view_matrix;
}

const glm::mat4& camera::projection_matrix() const {
    return m_projection_matrix;
}


glm::vec3 camera::position() const {
    return m_view_matrix[3];
}

}