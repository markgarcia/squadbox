#ifndef SQUADBOX_GFX_GLFW_WRAPPERS_HPP
#define SQUADBOX_GFX_GLFW_WRAPPERS_HPP

#pragma once

#include <GLFW/glfw3.h>
#include <gsl/gsl>

#include <mutex>
#include <string>

namespace squadbox::gfx {

class [[nodiscard]] glfw_manager {
public:
    glfw_manager() {
        glfwSetErrorCallback([](int /*error*/, const char* desc) noexcept {
            std::lock_guard<std::mutex> lock { mutex };
            last_glfw_error = desc;
        });

        if (!glfwInit()) throw std::runtime_error(last_error());

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }

    glfw_manager(const glfw_manager&) = delete;

    ~glfw_manager() {
        glfwTerminate();
    }

    std::string last_error() const {
        std::lock_guard<std::mutex> lock { mutex };
        return last_glfw_error;
    }

private:
    static std::mutex mutex;
    static std::string last_glfw_error;
};


class glfw_window;
class glfw_callback_base;
class imgui_glue;

struct glfw_data {
    void* key_callback;
    void* framebuffer_resize_callback;
    void* mouse_button_callback;
    void* scroll_callback;
    void* char_callback;
};


class glfw_window : public std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)> {
public:
    template<typename... arg_types>
    glfw_window(arg_types&&... args)
        : std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)>(glfwCreateWindow(std::forward<arg_types>(args)...), &glfwDestroyWindow) {
        glfwSetWindowUserPointer(get(), &m_data);
    }

    glfw_window(GLFWwindow* window)
        : std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)>(window, &glfwDestroyWindow) {
        glfwSetWindowUserPointer(get(), &m_data);
    }

    const glfw_data& data() const {
        return m_data;
    }

#define SQUADBOX_GLFW_WINDOW_CALLBACK(callback_name, callback_setter)                       \
    template <typename callback_type>                                                       \
    auto register_##callback_name##_callback(callback_type& callback) {                     \
        m_data.callback_name##_callback = &callback;                                        \
                                                                                            \
        callback_setter(get(), [](auto... args) {                                           \
            auto window_ptr = std::get<0>(std::tie(args...));                               \
                                                                                            \
            auto user_ptr = glfwGetWindowUserPointer(window_ptr);                           \
            assert(user_ptr != nullptr);                                                    \
            auto& data = *static_cast<glfw_data*>(user_ptr);                                \
                                                                                            \
            auto& callback = *static_cast<callback_type*>(data.callback_name##_callback);   \
            callback(args...);                                                              \
        })                                                                                  \
    }                                                                                       \

    SQUADBOX_GLFW_WINDOW_CALLBACK(key, glfwSetKeyCallback)
    SQUADBOX_GLFW_WINDOW_CALLBACK(framebuffer_resize, glfwSetFramebufferSizeCallback)
    SQUADBOX_GLFW_WINDOW_CALLBACK(mouse_button, glfwSetMouseButtonCallback)
    SQUADBOX_GLFW_WINDOW_CALLBACK(scroll, glfwSetScrollCallback)
    SQUADBOX_GLFW_WINDOW_CALLBACK(char, glfwSetCharCallback)

private:
    glfw_data& mut_data() {
        return m_data;
    }

    glfw_data m_data;
};


}

#endif