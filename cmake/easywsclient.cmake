include_guard()

set(EASYWSCLIENT_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/easywsclient/")

set(SOURCES
    "${EASYWSCLIENT_FOLDER}/easywsclient.hpp"
    "${EASYWSCLIENT_FOLDER}/easywsclient.cpp")

add_library(easywsclient)
target_sources(easywsclient PRIVATE ${SOURCES})
target_include_directories(easywsclient PUBLIC "${EASYWSCLIENT_FOLDER}")

set_target_properties(easywsclient PROPERTIES FOLDER "Dependencies/")
find_package(wolfssl CONFIG REQUIRED)

target_link_libraries(easywsclient PUBLIC wolfssl::wolfssl)
