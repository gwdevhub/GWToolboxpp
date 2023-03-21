include_guard()

add_library(nativefiledialog INTERFACE)
target_include_directories(nativefiledialog INTERFACE "${PROJECT_SOURCE_DIR}/dependencies/nativefiledialog")