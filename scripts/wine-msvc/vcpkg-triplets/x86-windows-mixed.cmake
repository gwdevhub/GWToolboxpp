# Cross-building x86-windows-mixed from Linux via wine (see scripts/build-wine-prefix.sh):
# mirrors vcpkg-overlays/triplets/x86-windows-mixed.cmake (kept in sync manually - vcpkg's
# own triplet-file parser doesn't support include(), so this can't just include() that file)
# plus the settings needed to point vcpkg at the wine-wrapped MSVC toolchain instead of
# trying to auto-detect a native Visual Studio Developer Prompt.
set(VCPKG_TARGET_ARCHITECTURE x86)

if(PORT MATCHES "discord-game-sdk|curl")
    set(VCPKG_CRT_LINKAGE dynamic)
    set(VCPKG_LIBRARY_LINKAGE dynamic)
else()
    set(VCPKG_CRT_LINKAGE static)
    set(VCPKG_LIBRARY_LINKAGE static)
endif()

# we never run BC6H/BC7 software compression. Also disable the "dx11" default feature here
# (wine-build only, see wine-msvc-toolchain.cmake): it default-compiles GPU compute shaders
# via a CompileShaders.cmd batch file invoked directly by the port's CMakeLists.txt, which
# isn't runnable on Linux (no cmd.exe interpreter outside of an explicit `wine` invocation).
# The codebase only calls directxtex's core Compress/Decompress/Convert functions - actual
# DDS/WIC texture loading goes through the separately-fetched D3D9 DDSTextureLoader9 (see
# cmake/directxtexloader.cmake), never through directxtex's own DX11 loader.
if(PORT STREQUAL "directxtex")
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DBC_USE_OPENMP=OFF;-DBUILD_DX11=OFF")
endif()

# Skip vcpkg's native-Windows Visual Studio Developer Prompt auto-detection (which doesn't
# exist on Linux) and point it at the wine-wrapped MSVC toolchain instead. Deliberately NOT
# setting VCPKG_CMAKE_SYSTEM_NAME here: vcpkg's "windows" platform-expression match (used by
# ports' "supports" checks, e.g. directxtex/minhook/discord-game-sdk) only recognizes it as
# Windows-family when left unset (empty) or "WindowsStore"/"MinGW" - an explicit "Windows"
# value doesn't match and makes every Windows-only port look unsupported.
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/wine-msvc-toolchain.cmake")
