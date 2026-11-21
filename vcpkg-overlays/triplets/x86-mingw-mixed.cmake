set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# Only DirectXTex gets the MinGW system name: it needs it to pull in directx-headers, but it breaks other ports' "windows" match.
if(PORT STREQUAL "directxtex")
    set(VCPKG_CMAKE_SYSTEM_NAME MinGW)
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DBC_USE_OPENMP=OFF;-DBUILD_DX11=OFF")
endif()

set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../../cmake/toolchain.cmake")
