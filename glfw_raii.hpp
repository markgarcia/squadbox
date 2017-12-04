#ifndef SQUADBOX_GLFW_RAII_HPP
#define SQUADBOX_GLFW_RAII_HPP

#pragma once

#include <GLFW/glfw3.h>

#include <mutex>
#include <string>

namespace squadbox {

class glfw_raii {
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
        glfwSetErrorCallback([]([[maybe_unused]] int error, const char* desc) noexcept {
            std::lock_guard<std::mutex> lock { mutex };
            last_glfw_error = desc;
        });

        if (!glfwInit()) throw std::runtime_error(last_error());
    }
};


[[nodiscard]]
inline glfw_raii& init_glfw() {
    static glfw_raii glfw;
    return glfw;
}


class glfwWindowRaii : public std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)> {
public:
    glfwWindowRaii(GLFWwindow* window) : std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)>(window, &glfwDestroyWindow) {}
};


}

#endif