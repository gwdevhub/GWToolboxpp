
FILE(GLOB SOURCES CONFIGURE_DEPENDS
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

target_link_libraries(GWToolbox PRIVATE
    # cmake dependencies:
    Core
    RestClient
    glaze::glaze

    version.lib # for GetFileVersionInfo
    bcrypt.lib # for sha256 of the launcher exe
    comctl32.lib # for TaskDialogIndirect (Defender prompt checkbox)
    )

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# GWToolbox.exe is a multi-config (VS) build, so it lands in a per-config
# folder like bin/RelWithDebInfo/ -- but the wasm build (a separate
# single-config Emscripten tree) only ever writes gwtoolbox.gwmod to bin/
# itself. /localdll looks next to the exe, so without this a RelWithDebInfo
# (or any non-Release) dev build never finds it. See tools/sync_gwmod.py.
add_custom_command(TARGET GWToolbox POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/sync_gwmod.py"
            native-build "${PROJECT_SOURCE_DIR}" "$<TARGET_FILE_DIR:GWToolbox>" "$<CONFIG>"
    COMMENT "syncing gwtoolbox.gwmod ($<CONFIG>) for $<TARGET_FILE_DIR:GWToolbox>"
    VERBATIM
)
