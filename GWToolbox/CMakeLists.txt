
FILE(GLOB SOURCES
    "*.h"
    "*.cpp"
    "GWToolbox.rc")

add_executable(GWToolbox)
target_sources(GWToolbox PRIVATE ${SOURCES})
target_precompile_headers(GWToolbox PRIVATE "stdafx.h")

set_target_properties(GWToolbox PROPERTIES
    LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS"
    LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:WINDOWS"
    LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS"
    VS_GLOBAL_RunCodeAnalysis false
    VS_GLOBAL_EnableMicrosoftCodeAnalysis true
    VS_GLOBAL_EnableClangTidyCodeAnalysis true
)

include(json)

target_link_libraries(GWToolbox PRIVATE
    # cmake dependencies:
    GWToolboxdll
    Core
    RestClient
    directxtex
    nlohmann_json::nlohmann_json

    version.lib # for GetFileVersionInfo
    )
