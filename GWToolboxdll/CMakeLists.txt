add_library(GWToolboxdll SHARED)

file(GLOB SOURCES
    "*.h"
    "*.cpp"
    "*.cc"
    "GWToolbox.rc"
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

target_link_options(GWToolboxdll PRIVATE /WX /OPT:REF /OPT:ICF /SAFESEH:NO)
target_link_options(GWToolboxdll PRIVATE $<$<NOT:$<CONFIG:Debug>>:/LTCG /INCREMENTAL:NO>)
target_link_options(GWToolboxdll PRIVATE $<$<CONFIG:Debug>:/IGNORE:4098 /OPT:NOREF /OPT:NOICF>)
target_link_options(GWToolboxdll PRIVATE $<$<CONFIG:RelWithDebInfo>:/OPT:NOICF>)
target_link_options(GWToolboxdll PRIVATE /IGNORE:4099) # pdb not found for github action

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
    utf8proc
    easywsclient
    discord_game_sdk
    nlohmann_json::nlohmann_json
    simpleini
    imgui::fonts
    IconFontCppHeaders
    nativefiledialog
    wintoast

    # libs:
    Dbghelp.lib # for MiniDump
    )
