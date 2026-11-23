include("${PROJECT_SOURCE_DIR}/GWToolbox/launcher/sources.cmake")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(GWTB_CONFIG "${CMAKE_BUILD_TYPE}")
if(NOT GWTB_CONFIG)
    set(GWTB_CONFIG RelWithDebInfo)
endif()
string(TOLOWER "${GWTB_CONFIG}" GWTB_PRESET)

set(GWTB_WINDOWS_LAUNCHER_BUILT "${PROJECT_SOURCE_DIR}/bin/${GWTB_CONFIG}/GWToolbox.exe")
set(GWTB_WINDOWS_LAUNCHER "${CMAKE_CURRENT_BINARY_DIR}/windows/GWToolbox.exe")
set(GWTB_WINDOWS_LAUNCHER_PREBUILT "" CACHE FILEPATH "GWToolbox.exe to embed, built beforehand; empty builds it with scripts/build.py")

if(GWTB_WINDOWS_LAUNCHER_PREBUILT)
    if(NOT EXISTS "${GWTB_WINDOWS_LAUNCHER_PREBUILT}")
        message(FATAL_ERROR "GWTB_WINDOWS_LAUNCHER_PREBUILT is missing: ${GWTB_WINDOWS_LAUNCHER_PREBUILT}")
    endif()
    set(GWTB_WINDOWS_LAUNCHER "${GWTB_WINDOWS_LAUNCHER_PREBUILT}")
    add_custom_target(GWToolbox_Launcher_Windows)
else()
file(GLOB GWTB_WINDOWS_LAUNCHER_INPUTS CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/CMakeLists.txt"
    "${PROJECT_SOURCE_DIR}/vcpkg.json"
    "${PROJECT_SOURCE_DIR}/GWToolbox/*.cpp"
    "${PROJECT_SOURCE_DIR}/GWToolbox/*.h"
    "${PROJECT_SOURCE_DIR}/GWToolbox/*.rc"
    "${PROJECT_SOURCE_DIR}/GWToolbox/CMakeLists.txt"
    "${PROJECT_SOURCE_DIR}/GWToolbox/web/index.html"
    "${PROJECT_SOURCE_DIR}/GWToolbox/web/package.json"
    "${PROJECT_SOURCE_DIR}/GWToolbox/web/vite.config.ts")
file(GLOB_RECURSE GWTB_WINDOWS_LAUNCHER_INPUTS_TREE CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/GWToolbox/launcher/*"
    "${PROJECT_SOURCE_DIR}/GWToolbox/platform/windows/*"
    "${PROJECT_SOURCE_DIR}/GWToolbox/web/src/*"
    "${PROJECT_SOURCE_DIR}/Core/*"
    "${PROJECT_SOURCE_DIR}/RestClient/*"
    "${PROJECT_SOURCE_DIR}/Dependencies/WebViewWindow/*"
    "${PROJECT_SOURCE_DIR}/cmake/*")

add_custom_command(
    OUTPUT "${GWTB_WINDOWS_LAUNCHER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/windows"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/build.py" --preset ${GWTB_PRESET} --target GWToolbox_Launcher_Windows
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GWTB_WINDOWS_LAUNCHER_BUILT}" "${GWTB_WINDOWS_LAUNCHER}"
    DEPENDS ${GWTB_WINDOWS_LAUNCHER_INPUTS} ${GWTB_WINDOWS_LAUNCHER_INPUTS_TREE}
    COMMENT "Building GWToolbox.exe for the Linux launcher"
    VERBATIM)
add_custom_target(GWToolbox_Launcher_Windows DEPENDS "${GWTB_WINDOWS_LAUNCHER}")
endif()

add_executable(GWToolbox_Launcher_Linux
    ${GWTB_LAUNCHER_SOURCES}
    ${GWTB_LAUNCHER_HEADERS}
    Command.cpp
    DllVersion.cpp
    Elevation.cpp
    EmbeddedIcon.cpp
    EmbeddedLauncher.cpp
    FileDialog.cpp
    LauncherUpdate.cpp
    Installation.cpp
    LauncherPaths.cpp
    Main.cpp
    ScanGuildWars.cpp
    "${GWTB_PAGES_HEADER}")

set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/EmbeddedLauncher.cpp" PROPERTIES
    COMPILE_DEFINITIONS "GWTB_WINDOWS_LAUNCHER_PATH=\"${GWTB_WINDOWS_LAUNCHER}\""
    OBJECT_DEPENDS "${GWTB_WINDOWS_LAUNCHER}")

set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/EmbeddedIcon.cpp" PROPERTIES
    COMPILE_DEFINITIONS "GWTB_APP_ICON_PATH=\"${PROJECT_SOURCE_DIR}/resources/gwtoolbox-256.png\""
    OBJECT_DEPENDS "${PROJECT_SOURCE_DIR}/resources/gwtoolbox-256.png")

target_include_directories(GWToolbox_Launcher_Linux PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${GWTB_LAUNCHER_DIR}"
    "${PROJECT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/Dependencies/WebViewWindow/third_party/gtk/include")

add_dependencies(GWToolbox_Launcher_Linux GWToolboxWeb GWToolbox_Launcher_Windows)
target_link_libraries(GWToolbox_Launcher_Linux PRIVATE webview_window)

set_target_properties(GWToolbox_Launcher_Linux PROPERTIES
    OUTPUT_NAME GWToolbox
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/${GWTB_CONFIG}")

find_program(GWTB_OBJCOPY NAMES objcopy llvm-objcopy REQUIRED)
set(GWTB_LINUX_DEBUG_FILE "$<TARGET_FILE_DIR:GWToolbox_Launcher_Linux>/GWToolbox.debug")
add_custom_command(TARGET GWToolbox_Launcher_Linux POST_BUILD
    COMMAND "${GWTB_OBJCOPY}" --only-keep-debug "$<TARGET_FILE:GWToolbox_Launcher_Linux>" "${GWTB_LINUX_DEBUG_FILE}"
    COMMAND "${GWTB_OBJCOPY}" --strip-debug "--add-gnu-debuglink=${GWTB_LINUX_DEBUG_FILE}" "$<TARGET_FILE:GWToolbox_Launcher_Linux>"
    COMMENT "Splitting the debug info of GWToolbox into GWToolbox.debug"
    VERBATIM)
