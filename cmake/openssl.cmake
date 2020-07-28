include_guard()

set(OPENSSL_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/")

file(GLOB SOURCES "${OPENSSL_FOLDER}/openssl/*.c")

add_library(openssl)
target_sources(openssl PRIVATE ${SOURCES})
target_compile_definitions(openssl PRIVATE "_CRT_SECURE_NO_WARNINGS")
target_include_directories(openssl PUBLIC "${OPENSSL_FOLDER}")
target_link_libraries(openssl PRIVATE
    "${OPENSSL_FOLDER}/openssl/lib/libeay32.lib"
    "${OPENSSL_FOLDER}/openssl/lib/ssleay32.lib")

set_target_properties(openssl PROPERTIES FOLDER "Dependencies/")
