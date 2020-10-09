include_guard()
include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG 95c99aaa4be611716093edcb6b01146ab9483f30)
FetchContent_GetProperties(imgui)
if (imgui_POPULATED)
    return()
endif()

FetchContent_Populate(imgui)

add_library(imgui)
set(SOURCES
    "${imgui_SOURCE_DIR}/imgui.h"
    "${imgui_SOURCE_DIR}/imgui_internal.h"
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    # we copied (and modified) impl_imgui_dx9.h/cpp files under GWToolboxdll
    "${imgui_SOURCE_DIR}/examples/imgui_impl_win32.h"
    "${imgui_SOURCE_DIR}/examples/imgui_impl_win32.cpp")
source_group(TREE "${imgui_SOURCE_DIR}" FILES ${SOURCES})
target_sources(imgui PRIVATE ${SOURCES})
target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}")
target_compile_definitions(imgui PUBLIC 
    IMGUI_USER_CONFIG="${PROJECT_SOURCE_DIR}/GWToolboxdll/imconfig.h")

set_target_properties(imgui PROPERTIES FOLDER "Dependencies/")
