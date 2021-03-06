
set(SQUADBOX_SRC
	main.cpp
    
    console_ui.hpp  console_ui.cpp
    
    gfx/camera.hpp              gfx/camera.cpp
    gfx/glfw_wrappers.hpp       gfx/glfw_wrappers.cpp
    gfx/gpu_memory_pool.hpp     gfx/gpu_memory_pool.cpp
    gfx/gpu_mesh.hpp            gfx/gpu_mesh.cpp
    gfx/imgui_glue.hpp          gfx/imgui_glue.cpp
    gfx/mesh.hpp                gfx/mesh.cpp
    gfx/render_job.hpp          gfx/render_job.cpp
    gfx/render_manager.hpp      gfx/render_manager.cpp
    gfx/vulkan_manager.hpp      gfx/vulkan_manager.cpp
    gfx/vulkan_utils.hpp        gfx/vulkan_utils.cpp

    gfx/primitives/box.hpp  gfx/primitives/box.cpp

    gfx/render_techniques/flat_shading.hpp  gfx/render_techniques/flat_shading.cpp
    
    test_scenes/cube.hpp    test_scenes/cube.cpp)

add_executable(squadbox ${SQUADBOX_SRC})

if(MSVC)
	target_compile_options(squadbox PRIVATE /std:c++latest /permissive-)

	if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else()
	    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
	endif()

    target_compile_definitions(squadbox PRIVATE
        UNICODE NOMINMAX _SCL_SECURE_NO_WARNINGS _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
endif()

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Boost REQUIRED COMPONENTS thread)

find_package(assimp REQUIRED)
add_library(assimp SHARED IMPORTED)
set_target_properties(assimp PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${ASSIMP_INCLUDE_DIRS}
    #INTERFACE_COMPILE_DEFINITIONS ${ASSIMP_CXX_FLAGS}
    )

if(WIN32)
    find_library(ASSIMP_DEBUG_LIBRARY assimp${ASSIMP_LIBRARY_SUFFIX}d)
    find_file(ASSIMP_DEBUG_DLL bin/assimp${ASSIMP_LIBRARY_SUFFIX}d.dll)
    set_target_properties(assimp PROPERTIES
                          IMPORTED_IMPLIB_DEBUG ${ASSIMP_DEBUG_LIBRARY}
                          IMPORTED_LOCATION_DEBUG ${ASSIMP_DEBUG_DLL})

    find_library(ASSIMP_RELEASE_LIBRARY assimp${ASSIMP_LIBRARY_SUFFIX}d)
    find_file(ASSIMP_RELEASE_DLL bin/assimp${ASSIMP_LIBRARY_SUFFIX}.dll)
    set_target_properties(assimp PROPERTIES
                          IMPORTED_IMPLIB_RELEASE ${ASSIMP_RELEASE_LIBRARY}
                          IMPORTED_LOCATION_RELEASE ${ASSIMP_RELEASE_DLL})
else()
    set_target_properties(assimp PROPERTIES IMPORTED_IMPLIB ${ASSIMP_LIBRARIES})
endif()

add_library(imgui SHARED IMPORTED)

if(WIN32)
    find_library(IMGUI_DEBUG_LIBRARY imguid)
    find_file(IMGUI_DEBUG_DLL bin/imguid.dll)
    set_target_properties(imgui PROPERTIES
                          IMPORTED_IMPLIB_DEBUG ${IMGUI_DEBUG_LIBRARY}
                          IMPORTED_LOCATION_DEBUG ${IMGUI_DEBUG_DLL})

    find_library(IMGUI_RELEASE_LIBRARY imgui)
    find_file(IMGUI_RELEASE_DLL bin/imgui.dll)
    set_target_properties(imgui PROPERTIES
                          IMPORTED_IMPLIB_RELEASE ${IMGUI_RELEASE_LIBRARY}
                          IMPORTED_LOCATION_RELEASE ${IMGUI_RELEASE_DLL})
else()
    find_library(IMGUI_LIBRARY imgui)
    set_target_properties(imgui PROPERTIES IMPORTED_IMPLIB ${IMGUI_LIBRARY})
endif()

target_compile_definitions(squadbox PRIVATE
    BOOST_THREAD_PROVIDES_EXECUTORS
    BOOST_THREAD_VERSION=4
    BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE)

target_link_libraries(squadbox
    Vulkan::Vulkan
    glfw glm
    assimp
    imgui
    ${Boost_LIBRARIES} Boost::dynamic_linking)

compile_glsl(TARGET squadbox
    OUTPUT_DIR ./shaders/compiled/
    FILES
    ./shaders/imgui.vert
    ./shaders/imgui.frag
    ./shaders/flat.vert
    ./shaders/flat.frag)