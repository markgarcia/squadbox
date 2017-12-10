# squadbox

A gaem for portfolio and for learning Vulkan.

Currently being built and tested in Windows with Visual Studio 2017.

Instructions:
* Install [LunarG Vulkan SDK](https://vulkan.lunarg.com/).
* Install [vcpkg](https://github.com/Microsoft/vcpkg) and the following:
  * GLFW
  * glm
  * assimp
  * imgui
  * boost
* I recommend using Visual Studio and directly opening this as a CMake project. **IMPORTANT:** If you do this please properly set `CMAKE_TOOLCHAIN_FILE` in CMakeSettings.json to vcpkg's.
* Run.
