# Chainloaded by x86-windows-mixed.cmake below when cross-building from Linux. vcpkg's stock
# windows triplet logic assumes a native Visual Studio Developer Prompt (vcvarsall.bat), which
# doesn't exist here - point it directly at the wine-wrapped MSVC tools on PATH instead
# (see scripts/wine-msvc/Dockerfile).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)
set(CMAKE_C_COMPILER cl)
set(CMAKE_CXX_COMPILER cl)
set(CMAKE_RC_COMPILER rc)

# Using VCPKG_CHAINLOAD_TOOLCHAIN_FILE replaces vcpkg's own scripts/toolchains/windows.cmake
# entirely, including the bits that enforce VCPKG_CRT_LINKAGE - without this, ports built here
# default to /MD (dynamic) and fail to link against the rest of the project, which is always
# built /MT (static; see CMakeLists.txt's CMAKE_MSVC_RUNTIME_LIBRARY setting) via
# VCPKG_CRT_LINKAGE=dynamic|static per PORT in x86-windows-mixed.cmake. Mirror both mechanisms
# vcpkg's own toolchain uses: CMAKE_MSVC_RUNTIME_LIBRARY (gated behind CMake's CMP0091 policy,
# so some ports' own older cmake_minimum_required ignore it) and explicit /MT or /MD in
# CMAKE_C/CXX_FLAGS_DEBUG|RELEASE (unconditional, and what actually fixed recastnavigation).
if(DEFINED VCPKG_CRT_LINKAGE)
    if(VCPKG_CRT_LINKAGE STREQUAL "dynamic")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        set(_CRT_FLAG_PREFIX "/MD")
    elseif(VCPKG_CRT_LINKAGE STREQUAL "static")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set(_CRT_FLAG_PREFIX "/MT")
    else()
        message(FATAL_ERROR "Invalid setting for VCPKG_CRT_LINKAGE: \"${VCPKG_CRT_LINKAGE}\". It must be \"static\" or \"dynamic\"")
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG "${_CRT_FLAG_PREFIX}d /Z7 /Ob0 /Od /RTC1" CACHE STRING "")
    set(CMAKE_C_FLAGS_DEBUG "${_CRT_FLAG_PREFIX}d /Z7 /Ob0 /Od /RTC1" CACHE STRING "")
    set(CMAKE_CXX_FLAGS_RELEASE "${_CRT_FLAG_PREFIX} /O2 /Oi /Gy /DNDEBUG /Z7" CACHE STRING "")
    set(CMAKE_C_FLAGS_RELEASE "${_CRT_FLAG_PREFIX} /O2 /Oi /Gy /DNDEBUG /Z7" CACHE STRING "")
    unset(_CRT_FLAG_PREFIX)
endif()
