add_library(core SHARED sender.cpp reveiver.cpp)

target_link_libraries(core crypto)

target_include_libraries(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

