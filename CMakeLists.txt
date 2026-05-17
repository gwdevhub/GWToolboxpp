cmake_minimum_required(VERSION 3.29)

if(NOT CMAKE_GENERATOR MATCHES "Ninja|Makefile")
    set(CMAKE_GENERATOR_PLATFORM win32)
endif()

project(gwtoolbox)

set(GWTOOLBOXEXE_VERSION "4.6")
set(GWTOOLBOXEXE_MODULE_VERSION "4,6,0,0") # used for exe module info. See GWToolbox/GWToolbox.rc
set(GWTOOLBOXDLL_VERSION "8.17")
set(GWTOOLBOXDLL_VERSION_BETA "") # can be anything. Marks beta version if not empty.
set(GWTOOLBOX_MODULE_VERSION "8,17,0,0") # used for Dll module info. See GWToolboxdll/GWToolbox.rc


if(MSVC_TOOLSET_VERSION LESS 143)
    message(FATAL_ERROR "Build using Visual Studio 2022 toolset (v143) or higher. Run cmake with `-T143`. You're currently using ${MSVC_TOOLSET_VERSION}")
endif()

# Increase constexpr and template instantiation depth for CTRE compile-time regex library
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # SHELL: prefix prevents CMake from deduplicating the repeated -Xclang flags.
    add_compile_options("SHELL:-Xclang -fconstexpr-depth=2048" "SHELL:-Xclang -fconstexpr-steps=10000000" /bigobj)
else()
    add_compile_options(/constexpr:depth2048 /constexpr:steps10000000 /bigobj /templateDepth:2048)
endif()

if(NOT(CMAKE_SIZEOF_VOID_P EQUAL 4))
    message(FATAL_ERROR "You are configuring a non 32-bit build, this is not supported. Run cmake with `-A Win32`")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_compile_definitions(
    "NOMINMAX"
    "WIN32_LEAN_AND_MEAN"

    "GWTOOLBOXEXE_VERSION=\"${GWTOOLBOXEXE_VERSION}\""
    "GWTOOLBOXEXE_MODULE_VERSION=${GWTOOLBOXEXE_MODULE_VERSION}"
    "GWTOOLBOXDLL_VERSION=\"${GWTOOLBOXDLL_VERSION}\""
    "GWTOOLBOXDLL_VERSION_BETA=\"${GWTOOLBOXDLL_VERSION_BETA}\""
    "GWTOOLBOX_MODULE_VERSION=${GWTOOLBOX_MODULE_VERSION}"
    )
	
add_compile_options(/permissive-)
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # MSVC: whole-program optimization + link-time codegen in non-Debug
    add_compile_options(/MP)
    add_compile_options($<$<NOT:$<CONFIG:Debug>>:/GL>)
    add_link_options($<$<NOT:$<CONFIG:Debug>>:/LTCG>)
endif()

set(CMAKE_CXX_STANDARD 26)
# CMake 4.2 doesn't yet know MSVC 14.50 supports C++26 — fall back to 23 for MSVC.
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_STANDARD 23)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Outputs dll, exe, and pdb into a /bin/config folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/")

include(gwca)
include(directxtexloader)
include(easywsclient)
include(imgui)
include(gwtoolboxdll_plugins)
include(nativefiledialog)
include(wintoast)
include(steamworks_sdk)

find_library(GAME_SDK discord_game_sdk)
find_path(GAME_SDK_INCLUDE discord-game-sdk/discord.h)
find_path(EARCUT_HPP_INCLUDE_DIRS "mapbox/earcut.hpp")
find_path(SIMPLEINI_INCLUDE_DIRS "ConvertUTF.c")
find_package(minhook CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(ctre CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)

add_subdirectory(GWToolboxdll)
add_subdirectory(Core)
add_subdirectory(RestClient)
add_subdirectory(GWToolbox)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT GWToolbox)
