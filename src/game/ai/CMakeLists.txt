add_library(
    ai
    pathfinding.c
)

target_include_directories(
    ai PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(ai PUBLIC 
    data_structures
    logger
    utils
)
