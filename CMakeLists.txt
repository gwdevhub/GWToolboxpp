cmake_minimum_required(VERSION 3.16)

set(CMAKE_GENERATOR_PLATFORM win32)

project(gwtoolbox)

set(GWTOOLBOXEXE_VERSION "4.1")
set(GWTOOLBOXDLL_VERSION "6.22")
set(GWTOOLBOXDLL_VERSION_BETA "") # can be anything. Marks beta version if not empty.
set(GWTOOLBOX_MODULE_VERSION "6,22,0,0") # used for Dll module info. See GWToolboxdll/GWToolbox.rc


if(MSVC_TOOLSET_VERSION LESS 143)
    message(FATAL_ERROR "Build using Visual Studio 2022 toolset (v143) or higher. Run cmake with `-T143`. You're currently using ${MSVC_TOOLSET_VERSION}")
endif()

if(NOT(CMAKE_SIZEOF_VOID_P EQUAL 4))
    message(FATAL_ERROR "You are configuring a non 32-bit build, this is not supported. Run cmake with `-A Win32`")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_compile_definitions(
    "NOMINMAX"
    "_WIN32_WINNT=_WIN32_WINNT_WIN7"
    "WIN32_LEAN_AND_MEAN"

    "GWTOOLBOXEXE_VERSION=\"${GWTOOLBOXEXE_VERSION}\""
    "GWTOOLBOXDLL_VERSION=\"${GWTOOLBOXDLL_VERSION}\""
    "GWTOOLBOXDLL_VERSION_BETA=\"${GWTOOLBOXDLL_VERSION_BETA}\""
    "GWTOOLBOX_MODULE_VERSION=${GWTOOLBOX_MODULE_VERSION}"
    )
	
add_compile_options(/MP /permissive-)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Outputs dll, exe, and pdb into a /bin/config folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/")

include(gwca)
include(imgui)
include(wintoast)
include(directxtex)
include(easywsclient)
include(simpleini)
include(discord_game_sdk)
include(gwtoolboxdll_plugins)
include(nativefiledialog)

add_subdirectory(GWToolboxdll)
add_subdirectory(Core)
add_subdirectory(RestClient)
add_subdirectory(GWToolbox)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT GWToolbox)
