
add_library(GWToolboxdll SHARED)

set(GWTOOLBOX_FOLDER "${PROJECT_SOURCE_DIR}/GWToolboxdll")
file(GLOB SOURCES
    "*.h"
    "*.cpp"
    "Modules/*.h"
    "Modules/*.cpp"
    "Utils/*.h"
    "Utils/*.cpp"
    "Widgets/*.h"
    "Widgets/*.cpp"
    "Widgets/Minimap/*.h"
    "Widgets/Minimap/*.cpp"
    "Windows/*.h"
    "Windows/*.cpp")
source_group(TREE "${GWTOOLBOX_FOLDER}" FILES ${SOURCES})
target_sources(GWToolboxdll PRIVATE ${SOURCES})

target_include_directories(GWToolboxdll PRIVATE 
    "${GWTOOLBOX_FOLDER}"
    )

target_link_libraries(GWToolboxdll PRIVATE
    # cmake targets:
    imgui
    gwca
    directx
    utf8proc
    easywsclient
    discord_game_sdk
    json
    simpleini
    # libs:
    Urlmon.lib
    Shlwapi.lib
    Wininet.lib
    Dbghelp.lib # for MiniDump
    libeay32.lib
    ssleay32.lib
    )
