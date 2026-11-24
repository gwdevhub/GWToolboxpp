include("${PROJECT_SOURCE_DIR}/GWToolbox/launcher/sources.cmake")

set(GWTB_WINDOWS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform/windows")

set(GWTB_WINDOWS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Cli.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Install.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Process.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Settings.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/stdafx.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/WasmInject.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/WindowsDefender.cpp"
    "${GWTB_WINDOWS_DIR}/DllDownload.cpp"
    "${GWTB_WINDOWS_DIR}/Elevation.cpp"
    "${GWTB_WINDOWS_DIR}/FileDialog.cpp"
    "${GWTB_WINDOWS_DIR}/Installation.cpp"
    "${GWTB_WINDOWS_DIR}/LauncherUpdate.cpp"
    "${GWTB_WINDOWS_DIR}/Releases.cpp"
    "${GWTB_WINDOWS_DIR}/ScanGuildWars.cpp"
    "${GWTB_WINDOWS_DIR}/WindowsInject.cpp")

set(GWTB_WINDOWS_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/Cli.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Install.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Process.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Registry.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Settings.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/stdafx.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/WasmInject.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/WindowsDefender.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/resource.h"
    "${GWTB_WINDOWS_DIR}/Releases.h"
    "${GWTB_WINDOWS_DIR}/Utf8.h"
    "${GWTB_WINDOWS_DIR}/WindowsInject.h")

add_executable(GWToolbox_Launcher_Windows)
target_sources(GWToolbox_Launcher_Windows PRIVATE
    ${GWTB_WINDOWS_SOURCES}
    ${GWTB_WINDOWS_HEADERS}
    ${GWTB_LAUNCHER_SOURCES}
    ${GWTB_LAUNCHER_HEADERS}
    "${CMAKE_CURRENT_SOURCE_DIR}/GWToolbox.rc")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/GWToolbox.rc" PROPERTIES
    OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/resources/gwtoolbox.ico")
target_precompile_headers(GWToolbox_Launcher_Windows PRIVATE "stdafx.h")
target_include_directories(GWToolbox_Launcher_Windows PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${GWTB_WINDOWS_DIR}"
    "${GWTB_LAUNCHER_DIR}"
    "${GWTOOLBOX_RESOURCE_INCLUDE_DIR}"
    "${PROJECT_BINARY_DIR}")
add_dependencies(GWToolbox_Launcher_Windows GWToolboxWeb)
webview_window_embed_loader(GWToolbox_Launcher_Windows)
target_compile_options(GWToolbox_Launcher_Windows PRIVATE ${GWTB_EXCEPTIONS})

set_target_properties(GWToolbox_Launcher_Windows PROPERTIES
    OUTPUT_NAME GWToolbox
    WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
    VS_GLOBAL_RunCodeAnalysis false
    VS_GLOBAL_EnableMicrosoftCodeAnalysis true
    VS_GLOBAL_EnableClangTidyCodeAnalysis true
)

target_link_libraries(GWToolbox_Launcher_Windows PRIVATE
    # cmake dependencies:
    Core
    RestClient
    glaze::glaze
    webview_window

    comctl32 # for TaskDialogIndirect (Defender prompt checkbox)
    ole32 # for the file dialogs
    shell32 # for the file dialogs and ShellExecute
    uuid # for the file dialog class ids
    )

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# GWToolbox.exe is a multi-config (VS) build, so it lands in a per-config
# folder like bin/RelWithDebInfo/ -- but the wasm build (a separate
# single-config Emscripten tree) only ever writes gwtoolbox.gwmod to bin/
# itself. /localdll looks next to the exe, so without this a RelWithDebInfo
# (or any non-Release) dev build never finds it. See tools/sync_gwmod.py.
add_custom_command(TARGET GWToolbox_Launcher_Windows POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/sync_gwmod.py"
            native-build "${PROJECT_SOURCE_DIR}" "$<TARGET_FILE_DIR:GWToolbox_Launcher_Windows>" "$<CONFIG>"
    COMMENT "syncing gwtoolbox.gwmod ($<CONFIG>) for $<TARGET_FILE_DIR:GWToolbox_Launcher_Windows>"
    VERBATIM
)
