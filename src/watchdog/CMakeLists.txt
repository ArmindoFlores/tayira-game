add_library(watchdog watchdog.c)

target_include_directories(watchdog PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(watchdog PUBLIC 
    data_structures
    logger
    Threads::Threads
)
