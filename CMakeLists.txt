cmake_minimum_required(VERSION 3.10)

# --- Project definition ---
project(tayira LANGUAGES C VERSION 0.1)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- External dependencies ---
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED) 
find_package(GLEW REQUIRED)
find_package(Threads REQUIRED)
find_package(Doxygen QUIET)

if (TARGET glfw)
    set(GLFW_TARGET glfw)
elseif (TARGET glfw3)
    set(GLFW_TARGET glfw3)
else()
    message(FATAL_ERROR "GLFW3 not found. Install it (e.g., libglfw3-dev) or provide a CMake package.")
endif()

if (DOXYGEN_FOUND)
    set(DOXYGEN_INPUT_DIRS
        "${CMAKE_SOURCE_DIR}/src"
    )
    list(JOIN DOXYGEN_INPUT_DIRS " " DOXYGEN_INPUT_DIRS_STR)

    set(DOXYGEN_FILE_PATTERNS "*.h")
    set(DOXYGEN_EXCLUDES "${CMAKE_BINARY_DIR}")
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/doxygen")

    configure_file(
        "${CMAKE_SOURCE_DIR}/Doxyfile.in"
        "${CMAKE_BINARY_DIR}/Doxyfile"
        @ONLY
    )

    add_custom_target(doc
        COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()

# --- Internal dependencies ---
add_subdirectory(src/cjson)
add_subdirectory(src/data_structures)
add_subdirectory(src/game)
add_subdirectory(src/logger)
add_subdirectory(src/renderer)
add_subdirectory(src/utils)
add_subdirectory(src/watchdog)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(tayira main/tayira.c)

target_link_libraries(tayira
    PRIVATE
    game
    logger
    renderer
    data_structures
    utils
    watchdog
)

if (UNIX AND NOT APPLE)
    target_link_libraries(tayira PRIVATE m dl)
endif()

add_custom_command(TARGET tayira POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
)
