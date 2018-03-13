#ifndef SQUADBOX_GFX_CAMERA_HPP
#define SQUADBOX_GFX_CAMERA_HPP

#include <glm/glm.hpp>

namespace squadbox::gfx {

class camera {
public:
    void orient(const glm::vec3& camera_position, const glm::vec3& look_position,
                const glm::vec3& up_direction = { 0, 1, 0 });
    void set_perspective(float fov_degrees, float aspect_ratio);

    const glm::mat4& view_matrix() const;
    const glm::mat4& projection_matrix() const;
    glm::vec3 position() const;

private:
    glm::mat4 m_view_matrix;
    glm::mat4 m_projection_matrix;
};

}

#endif