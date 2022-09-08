include_guard()

set(CURL_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/curl")

add_library(curl INTERFACE)
target_include_directories(curl INTERFACE "${CURL_FOLDER}/include")
target_link_libraries(curl INTERFACE
    "${CURL_FOLDER}/lib/libcurl_a.lib")