#include "glfw_wrappers.hpp"

namespace squadbox::gfx {

std::mutex glfw_manager::mutex;
std::string glfw_manager::last_glfw_error;

}