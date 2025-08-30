add_library(
    ui
    dialog.c
)

target_include_directories(
    ui PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(ui PUBLIC 
    data_structures
    logger
    utils
    renderer
)
