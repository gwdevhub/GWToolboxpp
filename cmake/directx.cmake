include_guard()

set(DIRECTX_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/DirectX/build/native/")

add_library(directx INTERFACE)
target_include_directories(directx INTERFACE "${DIRECTX_FOLDER}/include")
target_link_libraries(directx INTERFACE
    "${DIRECTX_FOLDER}/release/lib/x86/d3dx9.lib")

