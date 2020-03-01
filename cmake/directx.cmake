include_guard()

set(DIRECTX_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/DirectX/")

add_library(directx INTERFACE)
target_include_directories(directx INTERFACE "${DIRECTX_FOLDER}/include")
target_link_libraries(directx INTERFACE
    "${DIRECTX_FOLDER}/lib/x86/d3d9.lib"
    "${DIRECTX_FOLDER}/lib/x86/d3dx9.lib")

