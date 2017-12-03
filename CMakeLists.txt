cmake_minimum_required(VERSION 3.8)
project(squadbox)

set(SQUADBOX_SRC
	main.cpp)

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

find_package(assimp REQUIRED)
target_include_directories(squadbox PRIVATE ${ASSIMP_INCLUDE_DIRS})
#target_compile_options(squadbox PRIVATE ${ASSIMP_CXX_FLAGS})

find_package(boost REQUIRED)

target_link_libraries(squadbox
    Vulkan::Vulkan
    glfw GLEW::GLEW glm
    ${ASSIMP_LIBRARIES}
    Boost::boost Boost::dynamic_linking)