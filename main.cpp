#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <gsl/span>

#include <exception>
#include <memory>
#include <mutex>


class glfw {
    static std::mutex mutex;
    static std::string last_glfw_error;

public:
    friend glfw& init_glfw();

    glfw(const glfw&) = delete;

    ~glfw() {
        glfwTerminate();
    }

    std::string last_error() const {
        std::lock_guard<std::mutex> lock{ mutex };
        return last_glfw_error;
    }

private:
    glfw() {
        glfwSetErrorCallback([](int error, const char* desc) noexcept {
            std::lock_guard<std::mutex> lock { mutex };
            last_glfw_error = desc;
        });

        if (!glfwInit()) throw std::runtime_error(last_error());
    }
};

std::mutex glfw::mutex;
std::string glfw::last_glfw_error;


glfw& init_glfw() {
    static glfw glfw;
    return glfw;
}


auto glfw_window_destroyer = [](GLFWwindow* window) { glfwDestroyWindow(window); };
using glfwWindowRaii = std::unique_ptr<GLFWwindow, decltype(glfw_window_destroyer)>;


int main() {
    auto& glfw = init_glfw();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwWindowRaii { glfwCreateWindow(800, 600, "squadbox", nullptr, nullptr), glfw_window_destroyer };

    auto extensions = []() {
        std::uint32_t extensions_count;
        auto extensions_arr = glfwGetRequiredInstanceExtensions(&extensions_count);
        return gsl::make_span(extensions_arr, extensions_count);
    }();

    auto vk_instance = [&]() {
        vk::InstanceCreateInfo instance_create_info;
        instance_create_info
            .setPpEnabledExtensionNames(extensions.data())
            .setEnabledExtensionCount(extensions.size());

        return vk::UniqueInstance{ vk::createInstance(instance_create_info) };
    }();

    auto vk_surface = [&]() {
        VkSurfaceKHR vk_surface_khr_raw;
        auto result = vk::Result { glfwCreateWindowSurface(static_cast<VkInstance>(vk_instance.get()), window.get(), nullptr, &vk_surface_khr_raw) };
        if (result != vk::Result::eSuccess) throw std::runtime_error(vk::to_string(result));

        return vk::SurfaceKHR { vk_surface_khr_raw };
    }();

    while (!glfwWindowShouldClose(window.get()))
    {
        glfwSwapBuffers(window.get());
        glfwPollEvents();
    }
}