cmake_minimum_required(VERSION 3.8)
project(squadbox)

set(SQUADBOX_SRC
	main.cpp
    glfw_raii.hpp
    glfw_raii.cpp
    vulkan_manager.hpp
    vulkan_manager.cpp)

add_executable(squadbox ${SQUADBOX_SRC})

if(MSVC)
	target_compile_options(squadbox PRIVATE /std:c++latest /permissive-)
	if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else()
	    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
	endif()
endif()

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
find_package(boost REQUIRED)

find_package(assimp REQUIRED)
target_include_directories(squadbox PRIVATE ${ASSIMP_INCLUDE_DIRS})
#target_compile_options(squadbox PRIVATE ${ASSIMP_CXX_FLAGS})

add_library(imgui SHARED IMPORTED)

if(WIN32)
    find_library(IMGUI_DEBUG_LIBRARY imguid)
    find_file(IMGUI_DEBUG_DLL bin/imguid.dll)
    
    set_target_properties(imgui PROPERTIES IMPORTED_IMPLIB_DEBUG ${IMGUI_DEBUG_LIBRARY})
    set_target_properties(imgui PROPERTIES IMPORTED_LOCATION_DEBUG ${IMGUI_DEBUG_DLL})

    find_library(IMGUI_RELEASE_LIBRARY imgui)
    find_file(IMGUI_RELEASE_DLL bin/imgui.dll)
    set_target_properties(imgui PROPERTIES IMPORTED_IMPLIB_RELEASE ${IMGUI_RELEASE_LIBRARY})
    set_target_properties(imgui PROPERTIES IMPORTED_LOCATION_RELEASE ${IMGUI_RELEASE_DLL})
else()
    find_library(IMGUI_LIBRARY imgui)
    set_target_properties(imgui PROPERTIES IMPORTED_IMPLIB ${IMGUI_LIBRARY})
endif()

target_link_libraries(squadbox
    Vulkan::Vulkan
    glfw GLEW::GLEW glm
    ${ASSIMP_LIBRARIES}
    imgui
    Boost::boost Boost::dynamic_linking)

include(./cmake/compile_glsl.cmake)

compile_glsl(TARGET squadbox
             OUTPUT_DIR ./shaders/compiled/
             FILES
             ./shaders/imgui.vert
             ./shaders/imgui.frag)