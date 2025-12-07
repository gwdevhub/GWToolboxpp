include_guard()
include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9-docking
    )
FetchContent_GetProperties(imgui)
if (imgui_POPULATED)
    return()
endif()

FetchContent_MakeAvailable(imgui)

# Apply the patch without halting the build on failure, in case it's already applied
execute_process(
    COMMAND git apply "${CMAKE_CURRENT_LIST_DIR}/../cmake/patches/imgui_transparent_viewports.patch"
    WORKING_DIRECTORY ${imgui_SOURCE_DIR}
    RESULT_VARIABLE patch_result
    ERROR_VARIABLE patch_error
    OUTPUT_VARIABLE patch_output
)

if (NOT patch_result EQUAL 0)
    message(WARNING "Failed to apply patch: ${patch_error}")
    message(WARNING "Patch output: ${patch_output}")
endif()

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

set_target_properties(imgui PROPERTIES FOLDER "${CMAKE_CURRENT_LIST_DIR}/../Dependencies/")
