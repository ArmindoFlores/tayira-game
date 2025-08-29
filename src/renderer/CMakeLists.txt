add_library(renderer assets.c renderer.c)

target_include_directories(renderer PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(renderer PUBLIC 
    data_structures
    logger
    OpenGL::GL
    ${GLFW_TARGET}
    GLEW::GLEW
)
