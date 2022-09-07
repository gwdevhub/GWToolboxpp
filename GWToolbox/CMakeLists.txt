

FILE(GLOB SOURCES
    "*.h"
    "*.cpp"
    "GWToolbox.rc")

add_executable(GWToolbox)
target_sources(GWToolbox PRIVATE ${SOURCES})
target_precompile_headers(GWToolbox PRIVATE "stdafx.h")

set_target_properties(GWToolbox PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
set_target_properties(GWToolbox PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:WINDOWS")
set_target_properties(GWToolbox PROPERTIES LINK_FLAGS_MINRELSIZE "/SUBSYSTEM:WINDOWS")

include(json)

target_link_libraries(GWToolbox PRIVATE
    # cmake dependencies:
    GWToolboxdll
    Core
    RestClient
    nlohmann_json::nlohmann_json
    )
