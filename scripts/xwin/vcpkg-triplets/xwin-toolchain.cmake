# clang-cl + lld-link against an xwin-supplied MSVC/Windows SDK: builds the x86 Windows
# target natively on Linux, with no wine anywhere in the compile or link path.
#
# Chainloaded by x86-windows-mixed.cmake for vcpkg's ports, and used directly as
# CMAKE_TOOLCHAIN_FILE by the "xwin" preset for the project itself. vcpkg's stock windows
# triplet logic assumes a native Visual Studio Developer Prompt (vcvarsall.bat), which does
# not exist here, so this replaces it outright.
#
# XWIN_SDK is the `xwin splat` output directory (see scripts/xwin/setup-toolchain.sh).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

if(NOT XWIN_SDK)
    set(XWIN_SDK "$ENV{XWIN_SDK}")
endif()
if(NOT XWIN_SDK)
    message(FATAL_ERROR "XWIN_SDK is not set: point it at the `xwin splat` output (scripts/xwin/setup-toolchain.sh)")
endif()

set(CMAKE_C_COMPILER   clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER       lld-link)
set(CMAKE_AR           llvm-lib)
set(CMAKE_RC_COMPILER  llvm-rc)
set(CMAKE_MT           llvm-mt)

# sdk/include/winrt carries the WRL headers (wrl/client.h), which DirectXTex needs.
set(XWIN_INCLUDE_FLAGS
    "/imsvc ${XWIN_SDK}/crt/include \
/imsvc ${XWIN_SDK}/sdk/include/ucrt \
/imsvc ${XWIN_SDK}/sdk/include/um \
/imsvc ${XWIN_SDK}/sdk/include/shared \
/imsvc ${XWIN_SDK}/sdk/include/winrt")

set(CMAKE_C_FLAGS_INIT   "--target=i686-pc-windows-msvc ${XWIN_INCLUDE_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "--target=i686-pc-windows-msvc ${XWIN_INCLUDE_FLAGS}")
set(CMAKE_RC_FLAGS_INIT  "-I ${XWIN_SDK}/sdk/include/um -I ${XWIN_SDK}/sdk/include/shared -I ${XWIN_SDK}/crt/include")

set(XWIN_LIBRARY_FLAGS
    "/libpath:${XWIN_SDK}/crt/lib/x86 \
/libpath:${XWIN_SDK}/sdk/lib/ucrt/x86 \
/libpath:${XWIN_SDK}/sdk/lib/um/x86")
foreach(TARGET_KIND EXE SHARED MODULE)
    set(CMAKE_${TARGET_KIND}_LINKER_FLAGS_INIT "${XWIN_LIBRARY_FLAGS}")
endforeach()

# BOTH, not ONLY: there is no sysroot to confine lookups to, and ONLY makes find_library
# fail for ports that locate their own prebuilt binaries (discord-game-sdk's SDK_LIB).
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BEFORE)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)

# Replacing vcpkg's scripts/toolchains/windows.cmake also drops its VCPKG_CRT_LINKAGE
# enforcement, so ports would default to /MD and fail to link against the rest of the
# project (always /MT). Mirror both mechanisms vcpkg's own toolchain uses:
# CMAKE_MSVC_RUNTIME_LIBRARY (ignored by ports whose cmake_minimum_required predates
# CMP0091) and an explicit /MT or /MD in the per-config flags.
if(DEFINED VCPKG_CRT_LINKAGE)
    if(VCPKG_CRT_LINKAGE STREQUAL "dynamic")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        set(CRT_FLAG "/MD")
    elseif(VCPKG_CRT_LINKAGE STREQUAL "static")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set(CRT_FLAG "/MT")
    else()
        message(FATAL_ERROR "Invalid setting for VCPKG_CRT_LINKAGE: \"${VCPKG_CRT_LINKAGE}\". It must be \"static\" or \"dynamic\"")
    endif()
    set(CMAKE_C_FLAGS_DEBUG     "${CRT_FLAG}d /Z7 /Ob0 /Od" CACHE STRING "")
    set(CMAKE_CXX_FLAGS_DEBUG   "${CRT_FLAG}d /Z7 /Ob0 /Od" CACHE STRING "")
    set(CMAKE_C_FLAGS_RELEASE   "${CRT_FLAG} /O2 /Oi /Gy /DNDEBUG /Z7" CACHE STRING "")
    set(CMAKE_CXX_FLAGS_RELEASE "${CRT_FLAG} /O2 /Oi /Gy /DNDEBUG /Z7" CACHE STRING "")
    unset(CRT_FLAG)
endif()
