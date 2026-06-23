set(VCPKG_TARGET_ARCHITECTURE x86)

if(PORT MATCHES "discord-game-sdk|curl")
    set(VCPKG_CRT_LINKAGE dynamic)
    set(VCPKG_LIBRARY_LINKAGE dynamic)
else()
    set(VCPKG_CRT_LINKAGE static)
    set(VCPKG_LIBRARY_LINKAGE static)
endif()

# we never run BC6H/BC7 software compression
if(PORT STREQUAL "directxtex")
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DBC_USE_OPENMP=OFF")
endif()
