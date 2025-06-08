find_package(OpenSSL REQUIRED)

# 你的 crypto 库仍然保持不变
add_library(crypto STATIC
    crypto/aes_gcm.cpp
)

target_include_directories(crypto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto
)

# ✅ crypto 模块显式链接 OpenSSL
target_link_libraries(crypto PUBLIC OpenSSL::Crypto)

add_library(net STATIC
    net/sender.cpp
    net/receiver.cpp
)

target_include_directories(net PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/net
)

# ✅ net 链接 crypto
target_link_libraries(net PUBLIC crypto)

add_executable(main
    main.cpp
)

# ✅ main 链接 net 和 pthread
target_link_libraries(main PRIVATE net pthread)
