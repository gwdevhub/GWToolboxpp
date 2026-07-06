
FILE(GLOB SOURCES CONFIGURE_DEPENDS
    "*.h"
    "*.cpp"
    "GWToolboxSetup.rc")

add_executable(GWToolboxSetup)
target_sources(GWToolboxSetup PRIVATE ${SOURCES})
target_precompile_headers(GWToolboxSetup PRIVATE "stdafx.h")

set_target_properties(GWToolboxSetup PROPERTIES
    LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS"
    LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:WINDOWS"
    LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS"
)

# Links Core only (Window, Path, Defender) - deliberately not the launcher's injection code, so this stays clean to AV.
target_link_libraries(GWToolboxSetup PRIVATE Core)
