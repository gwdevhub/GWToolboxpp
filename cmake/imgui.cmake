include_guard()
include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9-docking
    PATCH_COMMAND ${CMAKE_CURRENT_LIST_DIR}/patches/apply_patch.bat imgui_transparent_viewports.patch
    LOG_PATCH true
    LOG_MERGED_STDOUTERR true
    LOG_OUTPUT_ON_FAILURE true
    )
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
source_group(TREE "${imgui_SOURCE_DIR}" FILES ${SOURCES})
target_sources(imgui PRIVATE ${SOURCES})
target_include_directories(imgui PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/../Dependencies"
    "${imgui_SOURCE_DIR}"
	)
target_compile_definitions(imgui PUBLIC 
    IMGUI_USER_CONFIG="${CMAKE_CURRENT_LIST_DIR}/../GWToolboxdll/imconfig.h")

set_target_properties(imgui PROPERTIES FOLDER "Dependencies/")
