add_library(GWToolboxdll SHARED)

file(GLOB SOURCES CONFIGURE_DEPENDS
    "*.h"
    "*.cpp"
    "GWToolbox.rc"
    "Constants/*.h"
    "Constants/*.cpp"
    "Modules/*.h"
    "Modules/*.cpp"
    "Utils/*.h"
    "Utils/*.cpp"
    "Utils/*.ixx"
    "Widgets/*.h"
    "Widgets/*.cpp"
    "Widgets/Minimap/*.h"
    "Widgets/Minimap/*.cpp"
    "Windows/*.h"
    "Windows/*.cpp"
    "Windows/Hotkeys/*.h"
    "Windows/Hotkeys/*.cpp"
    "Windows/Pathfinding/*.h"
    "Windows/Pathfinding/*.hpp"
    "Windows/Pathfinding/*.cpp"
    )
    
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})
target_sources(GWToolboxdll PRIVATE ${SOURCES})

set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/GWToolbox.rc"
    PROPERTIES OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/Dependencies/GWCA/bin/gwca.dll")

# auto-generated files - export header & pch 
include(GenerateExportHeader)
generate_export_header(GWToolboxdll)
#target_sources(GWToolboxdll PRIVATE FILE_SET generated_header TYPE HEADERS BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR} FILES ${CMAKE_CURRENT_BINARY_DIR}/gwtoolboxdll_export.h)
target_sources(GWToolboxdll PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gwtoolboxdll_export.h)
target_include_directories(GWToolboxdll PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/gwtoolboxdll_export.h)

target_precompile_headers(GWToolboxdll PRIVATE "stdafx.h")
source_group(Generated REGULAR_EXPRESSION cmake_pch)

target_compile_definitions(GWToolboxdll PRIVATE
    "_USRDLL"
    "GWCA_IMPORT"
    )
      
target_compile_options(GWToolboxdll PRIVATE /Gy /utf-8)
target_compile_options(GWToolboxdll PRIVATE $<$<CONFIG:RelWithDebInfo>:/Zi>)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Skip /W4 for clang-cl: it expands to -Wall -Wextra
    target_compile_options(GWToolboxdll PRIVATE $<$<CONFIG:Debug>:/Z7 /Od>)
else()
    target_compile_options(GWToolboxdll PRIVATE /W4 /WX)
    target_compile_options(GWToolboxdll PRIVATE $<$<CONFIG:Debug>:/ZI /Od>)
endif()

set_target_properties(GWToolboxdll PROPERTIES
    VS_GLOBAL_RunCodeAnalysis false
    VS_GLOBAL_EnableMicrosoftCodeAnalysis true
    VS_GLOBAL_EnableClangTidyCodeAnalysis false
)

target_link_options(GWToolboxdll PRIVATE
    /WX /OPT:REF /OPT:ICF /SAFESEH:NO
    $<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO>
    $<$<CONFIG:Debug>:/IGNORE:4098 /OPT:NOREF /OPT:NOICF>
    $<$<CONFIG:RelWithDebInfo>:/OPT:NOICF>
    $<$<CONFIG:Debug>:/NODEFAULTLIB:LIBCMT>
    /IGNORE:4099 # pdb not found for github action
    "/DELAYLOAD:gwca.dll"
    )

target_include_directories(GWToolboxdll PRIVATE
    "${PROJECT_SOURCE_DIR}/Dependencies"
    "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    ) 

include(imgui_fonts)

find_package(minhook CONFIG REQUIRED)

find_package(unofficial-uwebsockets CONFIG REQUIRED)
find_package(recastnavigation CONFIG REQUIRED)

target_link_libraries(GWToolboxdll PRIVATE
    # cmake targets:
    RestClient
    imgui
    Microsoft::DirectXTex
	directxtexloader
    gwca
    steamworks_sdk
    easywsclient
    ${CPP_GAME_SDK}
    glaze::glaze
    imgui::fonts
    IconFontCppHeaders
    nativefiledialog
    wintoast
	minhook::minhook
	ctre::ctre
    unofficial::uwebsockets::uwebsockets
    RecastNavigation::Detour

    # libs:
    Dbghelp.lib # for MiniDump
    bcrypt.lib  # for SHA256 (Resources.cpp)
	delayimp # for delay loading gwca
    )

file(GLOB FXC_PATHS "C:/Program Files (x86)/Windows Kits/10/bin/*/x86")
find_program(FXC fxc DOC "hlsl compiler" PATHS ${FXC_PATHS})
if(NOT FXC)
    message(SEND_ERROR "unable to find Fxc.exe for HLSL compilation")
endif(NOT FXC)

file(GLOB HLSL_VERTEX_SHADER_FILES CONFIGURE_DEPENDS
    Widgets/Minimap/Shaders/*_vs.hlsl
)
file(GLOB HLSL_PIXEL_SHADER_FILES CONFIGURE_DEPENDS
    Widgets/Minimap/Shaders/*_ps.hlsl
)

set_source_files_properties(${HLSL_VERTEX_SHADER_FILES} PROPERTIES ShaderType "vs")
set_source_files_properties(${HLSL_PIXEL_SHADER_FILES} PROPERTIES ShaderType "ps")

foreach(FILE ${HLSL_VERTEX_SHADER_FILES} ${HLSL_PIXEL_SHADER_FILES})
  get_filename_component(FILE_NOEXT ${FILE} NAME_WE)
  get_filename_component(FILE_DIR ${FILE} DIRECTORY)
  get_source_file_property(shadertype ${FILE} ShaderType)
  set(OUTPUT_FILE ${FILE_DIR}/${FILE_NOEXT}.h)
  add_custom_command(OUTPUT ${OUTPUT_FILE}
                     COMMAND ${FXC} /nologo /E main /T ${shadertype}_3_0 /Vn ${FILE_NOEXT} /O3 /Zi /Fh ${OUTPUT_FILE} ${FILE}
                     MAIN_DEPENDENCY ${FILE}
                     COMMENT "HLSL ${FILE}"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     VERBATIM)
  list(APPEND SHADER_OUTPUTS ${OUTPUT_FILE})
endforeach()

add_custom_target(compile_shaders DEPENDS ${SHADER_OUTPUTS})

add_dependencies(GWToolboxdll compile_shaders) 

add_custom_command(TARGET GWToolboxdll POST_BUILD
    COMMAND powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/compress.ps1"
            "$<TARGET_PDB_FILE:GWToolboxdll>"
            "$<CONFIG>"
    COMMENT "Compressing PDB for distribution (RelWithDebInfo only)"
    VERBATIM
)
