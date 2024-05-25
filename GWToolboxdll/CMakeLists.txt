add_library(GWToolboxdll SHARED)

file(GLOB SOURCES
    "*.h"
    "*.cpp"
    "GWToolbox.rc"
    "Constants/*.h"
    "Constants/*.cpp"
    "Modules/*.h"
    "Modules/*.cpp"
    "Utils/*.h"
    "Utils/*.cpp"
    "Widgets/*.h"
    "Widgets/*.cpp"
    "Widgets/Minimap/*.h"
    "Widgets/Minimap/*.cpp"
    "Windows/*.h"
    "Windows/*.cpp"
    )

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})
target_sources(GWToolboxdll PRIVATE ${SOURCES})

target_precompile_headers(GWToolboxdll PRIVATE "stdafx.h")
target_compile_definitions(GWToolboxdll PRIVATE "_USRDLL")

target_compile_options(GWToolboxdll PRIVATE /W4 /WX /Gy)
target_compile_options(GWToolboxdll PRIVATE $<$<CONFIG:Debug>:/ZI /Od>)
target_compile_options(GWToolboxdll PRIVATE $<$<CONFIG:RelWithDebInfo>:/Zi>)

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
    $<$<CONFIG:DEBUG>:/NODEFAULTLIB:LIBCMT>
    /IGNORE:4099 # pdb not found for github action
    )

target_include_directories(GWToolboxdll PRIVATE
    "${PROJECT_SOURCE_DIR}/Dependencies"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    )

include(json)
include(imgui_fonts)
include(earcut)

target_link_libraries(GWToolboxdll PRIVATE
    # cmake targets:
    RestClient
    imgui
    directxtex
    gwca
    earcut
    easywsclient
    discord_game_sdk
    nlohmann_json::nlohmann_json
    simpleini
    imgui::fonts
    IconFontCppHeaders
    nativefiledialog
    wintoast
    wolfssl

    # libs:
    Dbghelp.lib # for MiniDump
    )

file(GLOB FXC_PATHS "C:/Program Files (x86)/Windows Kits/10/bin/*/x86")
find_program(FXC fxc DOC "hlsl compiler" PATHS ${FXC_PATHS})
if(NOT FXC)
    message(SEND_ERROR "unable to find Fxc.exe for HLSL compilation")
endif(NOT FXC)

add_custom_target(shaders)

file(GLOB HLSL_VERTEX_SHADER_FILES
    Widgets/Minimap/Shaders/*_vs.hlsl
)
file(GLOB HLSL_PIXEL_SHADER_FILES
    Widgets/Minimap/Shaders/*_ps.hlsl
)

set_source_files_properties(${HLSL_VERTEX_SHADER_FILES} PROPERTIES ShaderType "vs")
set_source_files_properties(${HLSL_PIXEL_SHADER_FILES} PROPERTIES ShaderType "ps")

foreach(FILE ${HLSL_VERTEX_SHADER_FILES} ${HLSL_PIXEL_SHADER_FILES})
  get_filename_component(FILE_NOEXT ${FILE} NAME_WE)
  get_filename_component(FILE_DIR ${FILE} DIRECTORY)
  get_source_file_property(shadertype ${FILE} ShaderType)
  add_custom_command(TARGET shaders
                     COMMAND ${FXC} /nologo /E main /T ${shadertype}_2_0 /O3 /Zi /Fh ${FILE_DIR}/${FILE_NOEXT}.h ${FILE}
                     MAIN_DEPENDENCY ${FILE}
                     COMMENT "HLSL ${FILE}"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     VERBATIM)
endforeach(FILE)

add_dependencies(GWToolboxdll shaders)
