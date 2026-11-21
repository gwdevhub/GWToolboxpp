add_library(plugin_base INTERFACE)
target_sources(plugin_base INTERFACE
    "plugins/Base/dllmain.cpp"
    "plugins/Base/PluginApi.cpp"
    "plugins/Base/stl.h"
    "plugins/Base/ToolboxPlugin.h"
    "plugins/Base/ToolboxPlugin.cpp"
    "plugins/Base/PluginUtils.h"
    "plugins/Base/PluginUtils.cpp"
    "plugins/Base/ToolboxUIPlugin.h"
    "plugins/Base/ToolboxUIPlugin.cpp"
    "GWToolboxdll/RectF.h"
    "GWToolboxdll/MinimapPlugin.h"
    "GWToolboxdll/PluginApi.h"
    # Compiled into each plugin DLL: plugins get their own SettingsDoc/ToolboxIni (not exported by GWToolboxdll).
    "GWToolboxdll/ToolboxIni.cpp"
    "GWToolboxdll/Utils/SettingsDoc.cpp")
# Plugins use GWToolboxdll's headers only: everything they need from the toolbox goes through PluginApi.h.
target_include_directories(plugin_base INTERFACE
    "plugins/Base"
    "GWToolboxdll"
    )
target_link_libraries(plugin_base INTERFACE
    imgui
    glaze::glaze
    gwca
    IconFontCppHeaders
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
    set_target_properties(${PLUGIN} PROPERTIES PREFIX "")
    set_target_properties(${PLUGIN} PROPERTIES FOLDER "plugins/")
endmacro()

option(GWTOOLBOX_BUILD_EXAMPLE_PLUGIN "Build the ExamplePlugin sample (plugin-author reference; skipped in CI to save build time and keep it out of release artifacts)" ON)
if(GWTOOLBOX_BUILD_EXAMPLE_PLUGIN)
    add_tb_plugin(ExamplePlugin)
endif()
