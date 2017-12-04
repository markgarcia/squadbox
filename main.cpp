#include "vulkan_manager.hpp"
#include "glfw_raii.hpp"

namespace squadbox::globals {
    squadbox::vulkan_manager* vulkan_manager = nullptr;
}

int main() {
    using namespace squadbox;

    auto& glfw = init_glfw();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwWindowRaii { glfwCreateWindow(800, 600, "squadbox", nullptr, nullptr) };

    auto vulkan_manager = init_vulkan(window.get());
    globals::vulkan_manager = &vulkan_manager;

    glfwSetFramebufferSizeCallback(window.get(), [](GLFWwindow*, int width, int height) {
        globals::vulkan_manager->resize_framebuffer(width, height);
    });

    while (!glfwWindowShouldClose(window.get()))
    {
        glfwSwapBuffers(window.get());
        glfwPollEvents();
    }
}