#include "vulkan_manager.hpp"
#include "glfw_raii.hpp"


int main() {
    using namespace squadbox;

    auto& glfw = init_glfw();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwWindowRaii { glfwCreateWindow(800, 600, "squadbox", nullptr, nullptr) };

    auto vulkan_manager = init_vulkan(window.get());

    while (!glfwWindowShouldClose(window.get()))
    {
        glfwSwapBuffers(window.get());
        glfwPollEvents();
    }
}