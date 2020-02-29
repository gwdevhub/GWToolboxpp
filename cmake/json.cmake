include_guard()

add_library(json INTERFACE)
target_include_directories(json INTERFACE "${PROJECT_SOURCE_DIR}/dependencies/")

