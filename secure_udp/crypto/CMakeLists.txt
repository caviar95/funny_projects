add_library(crypto SHARED aes_gcm.cpp)
target_link_libraries(crypto PUBLIC sodium)

target_include_directories(crypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

