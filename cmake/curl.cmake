include_guard()

set(CURL_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/curl")

add_library(curl INTERFACE)
target_include_directories(curl INTERFACE "${CURL_FOLDER}")
target_link_libraries(curl INTERFACE
    "${CURL_FOLDER}/libcurl.lib")
target_link_libraries(curl INTERFACE 
    "${CURL_FOLDER}/libcurl.lib"
    "${CURL_FOLDER}/zlibstatic.lib"
    "${CURL_FOLDER}/mbedcrypto.lib"
    "${CURL_FOLDER}/mbedtls.lib"
    "${CURL_FOLDER}/mbedx509.lib"
    )
