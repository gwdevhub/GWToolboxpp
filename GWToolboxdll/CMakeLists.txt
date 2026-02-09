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
    "Utils/*.ixx"
    "Widgets/*.h"
    "Widgets/*.cpp"
    "Widgets/Minimap/*.h"
    "Widgets/Minimap/*.cpp"
    "Windows/*.h" 
    "Windows/*.cpp"
    "Windows/Pathfinding/*.h"
    "Windows/Pathfinding/*.hpp"
    "Windows/Pathfinding/*.cpp"
    )  
     
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})
target_sources(GWToolboxdll PRIVATE ${SOURCES})
 
target_precompile_headers(GWToolboxdll PRIVATE "stdafx.h")
target_compile_definitions(GWToolboxdll PRIVATE
    "_USRDLL"
    "GWCA_IMPORT"
    )
      
target_compile_options(GWToolboxdll PRIVATE /W4 /WX /Gy /utf-8)
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
    $<$<CONFIG:Debug>:/NODEFAULTLIB:LIBCMT>
    /IGNORE:4099 # pdb not found for github action
    "/DELAYLOAD:gwca.dll"
    )

target_include_directories(GWToolboxdll PRIVATE
    "${PROJECT_SOURCE_DIR}/Dependencies"
    "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
	"${SIMPLEINI_INCLUDE_DIRS}"
    ) 

include(imgui_fonts)

find_package(minhook CONFIG REQUIRED)

find_package(unofficial-uwebsockets CONFIG REQUIRED)

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
    nlohmann_json::nlohmann_json
    imgui::fonts
    IconFontCppHeaders
    nativefiledialog
    wintoast
    wolfssl::wolfssl
	minhook::minhook
	ctre::ctre
    unofficial::uwebsockets::uwebsockets

    # libs:
    Dbghelp.lib # for MiniDump
	delayimp # for delay loading gwca
    )
      
file(GLOB FXC_PATHS "C:/Program Files (x86)/Windows Kits/10/bin/*/x86")
find_program(FXC fxc DOC "hlsl compiler" PATHS ${FXC_PATHS})
if(NOT FXC)
    message(SEND_ERROR "unable to find Fxc.exe for HLSL compilation")
endif(NOT FXC)

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
