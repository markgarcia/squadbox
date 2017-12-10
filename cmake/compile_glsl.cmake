
function(compile_glsl)
    set(PARSE_OPTIONS)
    set(ONE_VALUE_ARGS TARGET OUTPUT_DIR)
    set(MULTI_VALUE_ARGS FILES)
    cmake_parse_arguments(ARG "${PARSE_OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})

    find_package(Vulkan REQUIRED)
    get_filename_component(VULKAN_DIR ${Vulkan_INCLUDE_DIR} DIRECTORY)

    get_filename_component(OUTPUT_DIR_ABSOLUTE ${ARG_OUTPUT_DIR} ABSOLUTE)

    foreach(GLSL_FILE ${ARG_FILES})
        get_filename_component(GLSL_FILE_PATH_ABSOLUTE ${GLSL_FILE} ABSOLUTE)
        get_filename_component(GLSL_FILE_NAME ${GLSL_FILE} NAME)

        set(OUTPUT_FILE_PATH_ABSOLUTE "${OUTPUT_DIR_ABSOLUTE}/${GLSL_FILE_NAME}.spv.c")

        add_custom_command(
            OUTPUT ${OUTPUT_FILE_PATH_ABSOLUTE}
            COMMAND "${VULKAN_DIR}/Bin/glslangValidator" ARGS "-V" "-x" "-o \"${OUTPUT_FILE_PATH_ABSOLUTE}\"" "${GLSL_FILE_PATH_ABSOLUTE}"
            MAIN_DEPENDENCY ${GLSL_FILE_PATH_ABSOLUTE}
        )

        set(OUTPUT_FILES ${OUTPUT_FILES};${OUTPUT_FILE_PATH_ABSOLUTE})
    endforeach(GLSL_FILE)

    add_custom_target("compile_glsl_${ARG_TARGET}" DEPENDS ${OUTPUT_FILES})

    add_dependencies(${ARG_TARGET} "compile_glsl_${ARG_TARGET}")
endfunction()