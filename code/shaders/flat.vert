#version 450

layout(binding = 0) uniform ubo_t {
    mat4 model_view;
    mat4 projection;
    vec4 model_color;
    vec4 ambient_color;
} ubo;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location = 0) /*smooth*/ out vec4 out_color;


void main() {
    gl_Position = ubo.projection * ubo.model_view * vec4(in_pos, 1.0);

    vec3 normal_mv = normalize(vec3(ubo.model_view * vec4(in_normal, 0.0)));
    float angle_of_incidence = clamp(dot(normal_mv, vec3(0, 0, 1)), 0, 1);

    out_color = (ubo.model_color * angle_of_incidence) + (ubo.model_color * ubo.ambient_color);
}