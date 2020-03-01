cmake_minimum_required(VERSION 3.15)

# Important: you HAVE to build this project in 32bit configuration
#
# For Visual studio 2019, the command to use is:
#
# cmake -G "Visual Studio 16 2019" -A Win32 ..

project(gwtoolbox)

add_definitions(-DNOMINMAX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")

include(gwca)
include(imgui)
include(directx)
include(utf8proc)
include(easywsclient)
include(simpleini)
include(discord_game_sdk)
include(json)

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
    )

target_link_libraries(gwtoolbox PRIVATE
    imgui
    gwca
    directx
    utf8proc
    easywsclient
    discord_game_sdk
    json
    simpleini
    Urlmon.lib
    Shlwapi.lib
    Wininet.lib
    Dbghelp.lib # for MiniDump
    )

add_subdirectory(CSLauncher)

add_library(module_base)
target_sources(module_base PUBLIC
    "${PROJECT_SOURCE_DIR}/modules/base/dllmain.cpp"
    "${PROJECT_SOURCE_DIR}/modules/base/module_base.h")
target_include_directories(module_base PUBLIC "${PROJECT_SOURCE_DIR}/modules/base")
target_include_directories(module_base PUBLIC "${PROJECT_SOURCE_DIR}/GWToolbox/GWToolbox")
target_link_libraries(module_base PUBLIC 
    imgui
    simpleini
    gwca
    )
target_compile_definitions(module_base PUBLIC BUILD_DLL)


add_library(InstanceTimer SHARED)
target_sources(InstanceTimer PRIVATE 
    "${PROJECT_SOURCE_DIR}/modules/InstanceTimer/InstanceTimer.h"
    "${PROJECT_SOURCE_DIR}/modules/InstanceTimer/InstanceTimer.cpp")
target_include_directories(InstanceTimer PRIVATE "${PROJECT_SOURCE_DIR}/modules/InstanceTimer")
target_link_libraries(InstanceTimer PRIVATE module_base)

