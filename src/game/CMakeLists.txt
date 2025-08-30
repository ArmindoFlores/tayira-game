add_library(
    game
    animation.c
    asset_manager.c
    drawable.c
    entity_manager.c
    font.c
    game.c
    level_manager.c
    map.c
)

add_subdirectory(ui)

target_include_directories(
    game PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(game PUBLIC 
    data_structures
    ui
    logger
    utils
    watchdog
    Threads::Threads
)
