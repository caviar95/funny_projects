cmake_minimum_required(VERSION 3.24)

project(secure_udp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# find_package(PkgConfig REQUIRED)
# pkg_check_modules(SOLIUM REQUIRED libsodium)
# include_directories(${SODIUM_INCLUDE_DIRS})
# link_directories(${SODIUM_LIBRARY_DIRS})

add_subdirectory(external/libsodium EXCLUDE_FROM_ALL)

add_subdirectory(crypto)
add_subdirectory(core)
add_subdirectory(main)

