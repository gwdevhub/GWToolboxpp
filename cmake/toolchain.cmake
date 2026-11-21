set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

# llvm-mingw (clang, lld, libc++, MinGW-w64), installed by bootstrap.sh / bootstrap.bat.
# clang rather than GCC because only clang writes CodeView, so crash dumps are debugged against a .pdb.
# One folder per host, so a dual-boot checkout keeps both builds instead of each bootstrap replacing the other's.
if(CMAKE_HOST_WIN32)
    set(LLVM_MINGW_HOST "windows")
    set(LLVM_MINGW_EXE ".exe")
else()
    set(LLVM_MINGW_HOST "linux")
endif()
get_filename_component(LLVM_MINGW_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.toolchain/llvm-mingw-${LLVM_MINGW_HOST}" ABSOLUTE)
if(NOT EXISTS "${LLVM_MINGW_ROOT}/bin")
    message(FATAL_ERROR "llvm-mingw not found at ${LLVM_MINGW_ROOT}; run the bootstrap script for this platform")
endif()
set(LLVM_MINGW_PREFIX "${LLVM_MINGW_ROOT}/bin/i686-w64-mingw32-")

set(CMAKE_C_COMPILER "${LLVM_MINGW_PREFIX}clang${LLVM_MINGW_EXE}")
set(CMAKE_CXX_COMPILER "${LLVM_MINGW_PREFIX}clang++${LLVM_MINGW_EXE}")
set(CMAKE_RC_COMPILER "${LLVM_MINGW_PREFIX}windres${LLVM_MINGW_EXE}")
set(CMAKE_AR "${LLVM_MINGW_PREFIX}ar${LLVM_MINGW_EXE}")
set(CMAKE_RANLIB "${LLVM_MINGW_PREFIX}ranlib${LLVM_MINGW_EXE}")

# On a case-sensitive host, includes like <ShlObj.h> resolve through symlinks made by scripts/toolchain.py.
# Never on Windows: a dual-boot checkout's Linux symlinks are unreadable there and shadow the real headers.
get_filename_component(MINGW_CASE_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.toolchain/case-include" ABSOLUTE)
if(NOT CMAKE_HOST_WIN32 AND EXISTS "${MINGW_CASE_INCLUDE_DIR}")
    set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES "${MINGW_CASE_INCLUDE_DIR}")
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES "${MINGW_CASE_INCLUDE_DIR}")
    set(CMAKE_RC_FLAGS_INIT "-I${MINGW_CASE_INCLUDE_DIR}")
endif()

# Function and data sections let the linker drop what the dll doesn't use, including from the vcpkg ports, which
# build with this file too (libuv alone carried 200 KB of unused filesystem code without them).
set(CMAKE_C_FLAGS_INIT "-gcodeview -ffunction-sections -fdata-sections -Wno-error=incompatible-pointer-types -Wno-error=incompatible-function-pointer-types")
set(CMAKE_CXX_FLAGS_INIT "-gcodeview -ffunction-sections -fdata-sections")

set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW_ROOT}/i686-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)

# The dll is injected into Gw.exe, so the C++ runtime is linked in statically. --pdb= names the .pdb after the output;
# -s drops the runtime libraries' DWARF (the .pdb already covers debugging) without losing it.
foreach(TARGET_KIND EXE SHARED MODULE)
    set(CMAKE_${TARGET_KIND}_LINKER_FLAGS_INIT "-static -Wl,--pdb= -Wl,-s")
endforeach()
