cmake_minimum_required(VERSION 3.15)

# Important: you HAVE to build this project in 32bit configuration
#
# For Visual studio 2019, the command to use is:
#
# cmake -G "Visual Studio 16 2019" -A Win32 ..

project(gwtoolbox)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")

include(gwca)
include(imgui)
include(directx)
include(utf8proc)
include(easywsclient)

add_library(gwtoolbox SHARED)

set(GWTOOLBOX_FOLDER "${PROJECT_SOURCE_DIR}/GWToolbox/GWToolbox")
file(GLOB SOURCES
    "${GWTOOLBOX_FOLDER}/*.h"
    "${GWTOOLBOX_FOLDER}/*.cpp"
    "${GWTOOLBOX_FOLDER}/Modules/*.h"
    "${GWTOOLBOX_FOLDER}/Modules/*.cpp"
    "${GWTOOLBOX_FOLDER}/Utils/*.h"
    "${GWTOOLBOX_FOLDER}/Utils/*.cpp"
    "${GWTOOLBOX_FOLDER}/Widgets/*.h"
    "${GWTOOLBOX_FOLDER}/Widgets/*.cpp"
    "${GWTOOLBOX_FOLDER}/Widgets/Minimap/*.h"
    "${GWTOOLBOX_FOLDER}/Widgets/Minimap/*.cpp"
    "${GWTOOLBOX_FOLDER}/Windows/*.h"
    "${GWTOOLBOX_FOLDER}/Windows/*.cpp")
source_group(TREE "${GWTOOLBOX_FOLDER}" FILES ${SOURCES})
target_sources(gwtoolbox PRIVATE ${SOURCES})

target_include_directories(gwtoolbox PRIVATE 
    "${GWTOOLBOX_FOLDER}"
    "${PROJECT_SOURCE_DIR}/Dependencies/"
    )

target_link_libraries(gwtoolbox PRIVATE
    imgui
    gwca
    directx
    utf8proc
    easywsclient
    Urlmon.lib
    Shlwapi.lib
    Wininet.lib
    Dbghelp.lib # for MiniDump
    )

add_subdirectory(CSLauncher)
