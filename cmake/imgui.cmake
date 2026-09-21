include_guard()
include(FetchContent)

# apply_patch.bat can't run directly when configuring from a non-Windows host (e.g. the
# clang/xwin cross-compile toolchain in scripts/build-xwin.sh); use the POSIX equivalent there.
if(CMAKE_HOST_WIN32)
    set(_IMGUI_PATCH_COMMAND "${CMAKE_CURRENT_LIST_DIR}/patches/apply_patch.bat")
else()
    set(_IMGUI_PATCH_COMMAND sh "${CMAKE_CURRENT_LIST_DIR}/patches/apply_patch.sh")
endif()

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.7-docking
    PATCH_COMMAND ${_IMGUI_PATCH_COMMAND} imgui_transparent_viewports.patch
    LOG_PATCH true
    LOG_MERGED_STDOUTERR true
    LOG_OUTPUT_ON_FAILURE true
    )
unset(_IMGUI_PATCH_COMMAND)
FetchContent_GetProperties(imgui)
if (imgui_POPULATED)
    return()
endif()

FetchContent_MakeAvailable(imgui)

add_library(imgui)
set(SOURCES
    "${imgui_SOURCE_DIR}/imgui.h"
    "${imgui_SOURCE_DIR}/imgui_internal.h"
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    # we copied (and modified) impl_imgui_dx9.h/cpp files under GWToolboxdll
    # we copied (and modified) imgui_impl_win32.h/cpp files under GWToolboxdll
)
if(EMSCRIPTEN)
    # Stock backend, unmodified (unlike the D3D9/Win32 pair above): the GLES3 surface
    # gw_in_browser's loader wires up (harness/gllib.js) is real WebGL2, so this needs
    # no wasm-specific changes -- see GWToolboxdll/Wasm/main_wasm.cpp.
    list(APPEND SOURCES
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.h"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3_loader.h"
    )
endif()
set(HOOK_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../Dependencies/imgui_test_engine_hooks/imgui_test_engine_hooks.h"
    "${CMAKE_CURRENT_LIST_DIR}/../Dependencies/imgui_test_engine_hooks/imgui_test_engine_hooks.cpp"
)
source_group(TREE "${imgui_SOURCE_DIR}" FILES ${SOURCES})
target_sources(imgui PRIVATE ${SOURCES} ${HOOK_SOURCES})
target_include_directories(imgui PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/../Dependencies"
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
	)
target_compile_definitions(imgui PUBLIC
    IMGUI_USER_CONFIG="${CMAKE_CURRENT_LIST_DIR}/../GWToolboxdll/imconfig.h")
if(EMSCRIPTEN)
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)
    # imconfig.h (IMGUI_USER_CONFIG above) includes GWCA/stdafx.h, which branches on
    # this to pick Win32Shim.h over real Windows.h -- needed here too, not just on
    # GWToolboxdll, since imgui.cpp itself is compiled against imconfig.h.
    target_compile_definitions(imgui PUBLIC GWCA_WASM=1)
    target_include_directories(imgui PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/../Dependencies/GWCA/include"
        "${CMAKE_CURRENT_LIST_DIR}/../Dependencies/GWCA/source")
endif()

# imgui is the per-frame hot path and nobody steps into it, so keep it optimised in Debug too:
# unoptimised imgui is most of what a Debug-build profile measures (~25us for a 28-button window
# vs ~7us once this is on), which buries the toolbox's own costs under framework noise.
if(MSVC AND NOT GWTB_DEBUG_RUNTIME_CHECKS)
    target_compile_options(imgui PRIVATE $<$<CONFIG:Debug>:/O2;/Ob2>)
endif()

set_target_properties(imgui PROPERTIES FOLDER "Dependencies/")
