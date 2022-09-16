include_guard()

set(OPENSSL_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/openssl")

add_library(openssl INTERFACE)
target_include_directories(openssl INTERFACE "${OPENSSL_FOLDER}/include")
target_link_libraries(openssl INTERFACE
    "${OPENSSL_FOLDER}/lib/libssl.lib"
    "${OPENSSL_FOLDER}/lib/libcrypto.lib")
