cmake_minimum_required(VERSION 3.15)

# Important: you HAVE to build this project in 32bit configuration
#
# For Visual studio 2019, the command to use is:
#
# cmake -G "Visual Studio 16 2019" -A Win32 ..

project(gwtoolbox)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_definitions(-DNOMINMAX)
add_definitions(-D_USE_MATH_DEFINES)

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

add_subdirectory(GWToolboxdll)
add_subdirectory(Core)
add_subdirectory(RestClient)
add_subdirectory(GWToolbox)


add_library(module_base INTERFACE)
target_sources(module_base INTERFACE
    "modules/base/dllmain.cpp"
    "modules/base/module_base.h")
target_include_directories(module_base INTERFACE "modules/base")
target_include_directories(module_base INTERFACE "GWToolboxdll/GWToolbox")
target_link_libraries(module_base INTERFACE 
    imgui
    simpleini
    gwca
    )
target_compile_definitions(module_base INTERFACE BUILD_DLL)

macro(add_tb_module MODULE)
    add_library(${MODULE} SHARED)
    file(GLOB SOURCES 
        "${PROJECT_SOURCE_DIR}/modules/${MODULE}/*.h"
        "${PROJECT_SOURCE_DIR}/modules/${MODULE}/*.cpp")
    target_sources(${MODULE} PRIVATE ${SOURCES})
    target_include_directories(${MODULE} PRIVATE "${PROJECT_SOURCE_DIR}/modules/${MODULE}")
    target_link_libraries(${MODULE} PRIVATE module_base)
    set_target_properties(${MODULE} PROPERTIES FOLDER "Modules/")
endmacro()

# add_tb_module(InstanceTimer)
# add_tb_module(clock)

