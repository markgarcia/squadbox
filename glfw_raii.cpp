#include "glfw_raii.hpp"

namespace squadbox {

std::mutex glfw_raii::mutex;
std::string glfw_raii::last_glfw_error;

}