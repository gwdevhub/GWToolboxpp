include_guard()

set(wolfssl_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/wolfssl")

add_library(wolfssl INTERFACE)
target_include_directories(wolfssl INTERFACE "${wolfssl_FOLDER}/include")
target_link_libraries(wolfssl INTERFACE
    "${wolfssl_FOLDER}/lib/wolfssl.lib")
