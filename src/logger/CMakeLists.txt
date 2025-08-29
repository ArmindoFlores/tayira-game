add_library(logger logger.c)

target_compile_definitions(logger PRIVATE
    LOG_LEVEL=LOG_LEVEL_DEBUG
)

target_include_directories(logger PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(logger PUBLIC 
    data_structures
)
