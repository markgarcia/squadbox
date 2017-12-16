#ifndef SQUADBOX_GFX_GLFW_WRAPPERS_HPP
#define SQUADBOX_GFX_GLFW_WRAPPERS_HPP

#pragma once

#include <GLFW/glfw3.h>
#include <gsl/gsl>

#include <mutex>
#include <string>

namespace squadbox {

class [[nodiscard]] glfw_raii {
    static std::mutex mutex;
    static std::string last_glfw_error;

public:
    friend glfw_raii& init_glfw();

    glfw_raii(const glfw_raii&) = delete;

    ~glfw_raii() {
        glfwTerminate();
    }

    std::string last_error() const {
        std::lock_guard<std::mutex> lock { mutex };
        return last_glfw_error;
    }

private:
    glfw_raii() {
        glfwSetErrorCallback([](int /*error*/, const char* desc) noexcept {
            std::lock_guard<std::mutex> lock { mutex };
            last_glfw_error = desc;
        });

        if (!glfwInit()) throw std::runtime_error(last_error());
    }
};

inline glfw_raii& init_glfw() {
    static glfw_raii glfw;
    return glfw;
}


class glfw_window;
class glfw_callback_base;
class imgui_glue;

struct glfw_data {
    glfw_callback_base* key_callback;
    glfw_callback_base* framebuffer_resize_callback;
    glfw_callback_base* mouse_button_callback;
    glfw_callback_base* scroll_callback;
    glfw_callback_base* char_callback;
};

class glfw_callback_base {};

namespace {
template <typename> struct glfw_callback_setter_func_type_info;

template <typename glfw_callback_func_type>
struct glfw_callback_setter_func_type_info<glfw_callback_func_type(GLFWwindow*, glfw_callback_func_type)> {
    using callback_func_type = glfw_callback_func_type;
};
}

template <
    typename glfw_callback_setter_func_type,
    glfw_callback_setter_func_type glfw_callback_setter_func,
    glfw_callback_base* glfw_data::*data_member,
    typename callback_type
>
class [[nodiscard]] glfw_callback : public glfw_callback_base {
    using glfw_callback_func_type = typename glfw_callback_setter_func_type_info<glfw_callback_setter_func_type>::callback_func_type;

public:
    template<typename callback_arg_type>
    glfw_callback(gsl::not_null<GLFWwindow*> window, callback_arg_type&& callback)
        : m_callback { std::forward<callback_arg_type>(callback) } {
        auto user_ptr = glfwGetWindowUserPointer(window);
        assert(user_ptr != nullptr);
        auto& data = *static_cast<glfw_data*>(user_ptr);

        (data.*data_member) = this;

        glfw_callback_setter_func(window, [](auto... args) {
            auto window_ptr = std::get<0>(std::tie(args...));

            auto user_ptr = glfwGetWindowUserPointer(window_ptr);
            assert(user_ptr != nullptr);
            auto& data = *static_cast<glfw_data*>(user_ptr);

            auto& callback = *static_cast<glfw_callback*>(data.*data_member);
            callback.m_callback(args...);
        });
    }

private:
    callback_type m_callback;
};


#define SQUADBOX_GLFW_CALLBACK(name, callback_setter) \
template <typename callback_type>                                                                         \
auto glfw_##name##_callback(gsl::not_null<GLFWwindow*> window, callback_type&& callback) {  \
    return glfw_callback<                                                                                 \
        decltype(callback_setter),                                                                        \
        callback_setter,                                                                                  \
        &glfw_data::name##_callback,                                                                      \
        std::decay_t<callback_type>                                                                       \
    > { window, std::forward<callback_type>(callback) };                                                  \
}                                                                                                         \

SQUADBOX_GLFW_CALLBACK(key, glfwSetKeyCallback)
SQUADBOX_GLFW_CALLBACK(framebuffer_resize, glfwSetFramebufferSizeCallback)
SQUADBOX_GLFW_CALLBACK(mouse_button, glfwSetMouseButtonCallback)
SQUADBOX_GLFW_CALLBACK(scroll, glfwSetScrollCallback)
SQUADBOX_GLFW_CALLBACK(char, glfwSetCharCallback)


class glfw_window : public std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)> {
public:
    glfw_window(GLFWwindow* window)
        : std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)>(window, &glfwDestroyWindow) {
        glfwSetWindowUserPointer(get(), &m_data);
    }

    const glfw_data& data() const {
        return m_data;
    }

private:
    glfw_data& mut_data() {
        return m_data;
    }

    glfw_data m_data;
};


}

#endif