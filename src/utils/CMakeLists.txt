add_library(utils utils.c)

target_include_directories(utils PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(utils PUBLIC 
    logger
    cJSON
)
