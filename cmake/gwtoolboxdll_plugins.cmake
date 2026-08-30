add_library(plugin_base INTERFACE)
target_sources(plugin_base INTERFACE
    "plugins/Base/dllmain.cpp"
    "plugins/Base/stl.h"
    "plugins/Base/ToolboxPlugin.h"
    "plugins/Base/ToolboxPlugin.cpp"
    "plugins/Base/PluginUtils.h"
    "plugins/Base/PluginUtils.cpp"
    "plugins/Base/ToolboxUIPlugin.h"
    "plugins/Base/ToolboxUIPlugin.cpp"
    "GWToolboxdll/RectF.h"
    "GWToolboxdll/MinimapPlugin.h"
    # Compiled into each plugin DLL: plugins get their own SettingsDoc/ToolboxIni (not exported by GWToolboxdll).
    "GWToolboxdll/ToolboxIni.cpp"
    "GWToolboxdll/Utils/SettingsDoc.cpp")
target_include_directories(plugin_base INTERFACE
    "plugins/Base"
    "GWToolboxdll" # careful here, we only get access to exported and header functions!
    )
target_link_libraries(plugin_base INTERFACE
    imgui
    glaze::glaze
    gwca
    IconFontCppHeaders
    GWToolboxdll # for GetFont
    )
target_compile_definitions(plugin_base INTERFACE BUILD_DLL)

macro(add_tb_plugin PLUGIN)
    add_library(${PLUGIN} SHARED)
    file(GLOB SOURCES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/plugins/${PLUGIN}/*.h"
        "${PROJECT_SOURCE_DIR}/plugins/${PLUGIN}/*.cpp")
    target_sources(${PLUGIN} PRIVATE ${SOURCES})
    target_include_directories(${PLUGIN} PRIVATE "${PROJECT_SOURCE_DIR}/plugins/${PLUGIN}")
    target_link_libraries(${PLUGIN} PRIVATE plugin_base)
    target_compile_options(${PLUGIN} PRIVATE /wd4201 /wd4505)
    target_compile_options(${PLUGIN} PRIVATE /Gy)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # See note in GWToolboxdll/CMakeLists.txt — /W4 (=-Wall -Wextra)
        target_compile_options(${PLUGIN} PRIVATE $<$<CONFIG:Debug>:/Z7 /Od>)
        target_link_options(${PLUGIN} PRIVATE /OPT:REF /OPT:ICF /SAFESEH:NO)
        target_link_options(${PLUGIN} PRIVATE $<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO>)
    else()
        target_compile_options(${PLUGIN} PRIVATE /W4 /WX)
        target_compile_options(${PLUGIN} PRIVATE $<$<NOT:$<CONFIG:Debug>>:/GL>)
        target_compile_options(${PLUGIN} PRIVATE $<$<CONFIG:Debug>:/ZI /Od>)
        target_link_options(${PLUGIN} PRIVATE /WX /OPT:REF /OPT:ICF /SAFESEH:NO)
        target_link_options(${PLUGIN} PRIVATE $<$<NOT:$<CONFIG:Debug>>:/LTCG /INCREMENTAL:NO>)
    endif()
    target_link_options(${PLUGIN} PRIVATE $<$<CONFIG:Debug>:/IGNORE:4098 /OPT:NOREF /OPT:NOICF>)
    target_link_options(${PLUGIN} PRIVATE $<$<CONFIG:RelWithDebInfo>:/OPT:NOICF>)
    set_target_properties(${PLUGIN} PROPERTIES FOLDER "plugins/")
endmacro()

add_tb_plugin(ExamplePlugin)

add_tb_plugin(SCTracker)
# Core (PathGetDocumentsPath/PathGetComputerName) so the plugin writes into the same
# Documents\GWToolboxpp\<computer>\runs folder GWToolboxdll uses, without linking GWToolboxdll internals.
# RestClient (AsyncRestClient, WinHTTP-backed) for publishing runs to a backend endpoint.
target_link_libraries(SCTracker PRIVATE Core RestClient)

# Bump this by hand whenever a new SCTracker build should be treated as required by the backend's
# minimum-version check (X-Plugin-Version header, GET /plugin-version) - everything downstream of
# this one variable (the compiled-in kPluginVersion constant and the SCTracker.version.json shipped
# alongside the built dll, both below) stays in sync automatically, so there's nothing else to edit
# by hand.
set(SCTRACKER_PLUGIN_VERSION 10 CACHE STRING "SCTracker plugin protocol version (see PluginVersion.generated.h.in)" FORCE)

configure_file(
    "${PROJECT_SOURCE_DIR}/plugins/SCTracker/PluginVersion.generated.h.in"
    "${CMAKE_BINARY_DIR}/generated/SCTracker/PluginVersion.generated.h"
    @ONLY)
target_include_directories(SCTracker PRIVATE "${CMAKE_BINARY_DIR}/generated/SCTracker")

# SCTracker.version.json needs a fresh compiled_at on every actual build, not just when the version
# number changes - a POST_BUILD custom command (which reruns on every build) rather than
# configure_file (which only reruns when CMakeLists/*.cmake themselves change) is what gets that.
# Pure CMake script mode (cmake -P), not a shell/PowerShell script, so this needs no host-OS
# branching to work cross-platform.
add_custom_command(TARGET SCTracker POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DVERSION=${SCTRACKER_PLUGIN_VERSION}
            -DOUTPUT_PATH=$<TARGET_FILE_DIR:SCTracker>/SCTracker.version.json
            -P "${PROJECT_SOURCE_DIR}/plugins/SCTracker/write-version-metadata.cmake"
    COMMENT "Writing SCTracker.version.json"
    VERBATIM
)
