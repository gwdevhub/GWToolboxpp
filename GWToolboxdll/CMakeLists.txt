if(EMSCRIPTEN)
    # MVP wasm scaffold: proves the mod_init/rendering pipeline (see Wasm/main_wasm.cpp
    # for exactly what, and why it's the whole file list -- deliberately no
    # Modules/Widgets/Windows/* yet, per the "compile only what's not problematic at
    # this stage" wasm-porting scoping). Not a wasm branch of the real product below.
    #
    # SHARED with the -shared flag dropped (CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS ""),
    # same as kamadanwasm/gw_in_browser's examples/mymod: CMake links instead of
    # archiving, but emcc does not map -shared onto SIDE_MODULE=1 -- this needs to stay
    # a static MAIN_MODULE=2, not a side module (see either project's CMakeLists for
    # the full rationale).
    set(CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "")
    add_library(GWToolboxdll SHARED
        Wasm/main_wasm.cpp
        Wasm/Logger_Wasm.cpp
    )
    set_target_properties(GWToolboxdll PROPERTIES
        PREFIX "" OUTPUT_NAME "GWToolboxdll" SUFFIX ".wasm")
    # Ninja + CXX_STANDARD 23 would trigger C++20 module dependency scanning, which
    # emscan-deps cannot answer; this target uses no modules. Same note as GWCA's own
    # CMakeLists and kamadanwasm's.
    set_target_properties(GWToolboxdll PROPERTIES CXX_SCAN_FOR_MODULES OFF)

    # Logger.h includes this unconditionally (GWTOOLBOXDLL_EXPORT), same as native.
    include(GenerateExportHeader)
    generate_export_header(GWToolboxdll)
    target_include_directories(GWToolboxdll PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

    target_precompile_headers(GWToolboxdll PRIVATE "stdafx.h")

    target_compile_definitions(GWToolboxdll PRIVATE GWCA_WASM=1)
    # -fshort-wchar: GWCA's structs mirror the game's UTF-16 (2-byte wchar_t) layout;
    # clang defaults to 4. Matches kamadanwasm's CMakeLists note on the same flag.
    target_compile_options(GWToolboxdll PRIVATE -fshort-wchar -fdeclspec)

    target_include_directories(GWToolboxdll PRIVATE
        "${PROJECT_SOURCE_DIR}/Dependencies"
        "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/include"
        "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/source"
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(GWToolboxdll PRIVATE imgui)

    # No gwca link step: GW::* calls resolve as env imports at injection time (see
    # gw_in_browser's docs/mod-loading.md), not at link time -- same as every other
    # wasm consumer of GWCA (kamadanwasm, gw_in_browser's example mods).
    #
    # GLOBAL_BASE/TABLE_BASE placement is coordinated against gwca.wasm's own fixed
    # range (GLOBAL_BASE ~0x400..0x16500, table-base 0x10000..0x1FFFF -- see GWCA's
    # Source/Platform/Wasm/README.md and kamadanwasm's CMakeLists) so this module's
    # statics/funcrefs cannot overlap and silently clobber gwca's. Not yet coordinated
    # against any OTHER mod (e.g. kamadanwasm's own 0x40000/0x20000) -- fine standalone,
    # but revisit if this is ever injected alongside another mod.
    target_link_options(GWToolboxdll PRIVATE
        -sMAIN_MODULE=2
        -sMALLOC=none
        -sIMPORTED_MEMORY=1
        -sALLOW_MEMORY_GROWTH=1
        --no-entry
        -sWASM_BIGINT=0
        -sSUPPORT_LONGJMP=0
        -sGLOBAL_BASE=524288
        -sTOTAL_STACK=131072
        -Wl,--table-base=262144
        -Wl,--growable-table
        "-sEXPORTED_FUNCTIONS=['_mod_init']"
        # Without this, emcc auto-links its OWN bundled GL implementation
        # (-lGL-getprocaddr etc, real functions from Emscripten's sysroot) the moment
        # imgui_impl_opengl3.cpp calls any gl* function -- silently defeating the whole
        # point of gllib.js: those real gl* names must stay UNDEFINED at link time so
        # MAIN_MODULE=2 turns them into env imports the loader resolves at injection
        # time (see harness/gllib.js's file header), not symbols resolved here against
        # a second, unrelated GL implementation that owns no actual WebGL context.
        -sAUTO_NATIVE_LIBRARIES=0
        # AUTO_NATIVE_LIBRARIES alone stopped the native (.a) GL library from linking,
        # but glGetString is ALSO one of Emscripten's own JS-library (library_webgl.js)
        # symbols, auto-included independently of that setting -- same fix, same
        # reasoning, the other half of the auto-include mechanism.
        -sAUTO_JS_LIBRARIES=0
    )

    # Packaging is its own target, not just a build step of GWToolboxdll: the
    # artifact gw_in_browser's loader actually takes is the .gwmod bundle
    # (GWToolboxdll.wasm + the gwca.wasm it was built against, zipped with a
    # manifest -- see tools/package.py), not the bare .wasm, and giving it a
    # name (gwtoolbox_gwmod) makes it independently buildable/discoverable
    # (`cmake --build build-wasm --target gwtoolbox_gwmod`) rather than only
    # ever firing as a side effect of building GWToolboxdll. Still runs on a
    # plain build too -- it's in ALL -- so nothing about the default flow changes.
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(GWTOOLBOX_WASM_DIST_DIR "${PROJECT_SOURCE_DIR}/bin"
        CACHE PATH "where gwtoolbox.gwmod is written")

    # GWCA's create_release also builds gwcad.wasm (DEBUG_POSTFIX "d", asserts +
    # [SCAN] logging compiled in -- see GWCA/CMakeLists.txt's wasm-debug preset),
    # same "d"-suffix convention as the native gwcad.dll/gwcad.lib. Off by default
    # (the ordinary `cmake --build build-wasm` flow keeps embedding the release
    # gwca.wasm); GWToolboxpp2's build_wasm project (GWToolboxdll/CMakeLists.txt's
    # native branch) sets this ON when the VS config it was triggered from is
    # Debug -- see tools/sync_gwmod.py.
    option(GWTOOLBOX_WASM_GWCA_DEBUG "Embed GWCA's debug wasm build (gwcad.wasm) instead of the release one" OFF)
    if(GWTOOLBOX_WASM_GWCA_DEBUG)
        set(GWTOOLBOX_WASM_GWCA_FILE "gwcad.wasm")
    else()
        set(GWTOOLBOX_WASM_GWCA_FILE "gwca.wasm")
    endif()

    add_custom_command(
        OUTPUT "${GWTOOLBOX_WASM_DIST_DIR}/gwtoolbox.gwmod"
               "${GWTOOLBOX_WASM_DIST_DIR}/gwtoolbox.modfile.json"
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/package.py"
                --dll  "$<TARGET_FILE:GWToolboxdll>"
                --gwca "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/wasm/${GWTOOLBOX_WASM_GWCA_FILE}"
                --out  "${GWTOOLBOX_WASM_DIST_DIR}"
                # Same version string the native dll's PE resource carries (see
                # GWToolboxdll/GWToolbox.rc's ProductVersion, sourced from this
                # same CMake variable) -- stamped into the bundled manifest.json
                # since a .gwmod has no PE resource of its own to carry one.
                --version "${GWTOOLBOXDLL_VERSION}"
        DEPENDS GWToolboxdll "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/wasm/${GWTOOLBOX_WASM_GWCA_FILE}"
        COMMENT "packaging ${GWTOOLBOX_WASM_DIST_DIR}/gwtoolbox.gwmod (${GWTOOLBOX_WASM_GWCA_FILE})"
        VERBATIM)
    add_custom_target(gwtoolbox_gwmod ALL
        DEPENDS "${GWTOOLBOX_WASM_DIST_DIR}/gwtoolbox.gwmod"
                "${GWTOOLBOX_WASM_DIST_DIR}/gwtoolbox.modfile.json")

    # Mirror into any native per-config folders (bin/RelWithDebInfo, bin/Release,
    # ...) that already exist, from an earlier native build -- so building wasm
    # after native keeps /localdll working there too without a native rebuild.
    # See tools/sync_gwmod.py.
    add_custom_command(TARGET gwtoolbox_gwmod POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/sync_gwmod.py"
                copy-to-configs "${PROJECT_SOURCE_DIR}"
        COMMENT "syncing gwtoolbox.gwmod into existing native config folders"
        VERBATIM)

    return()
endif()

add_library(GWToolboxdll SHARED)

file(GLOB SOURCES CONFIGURE_DEPENDS
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
    "Windows/Hotkeys/*.h"
    "Windows/Hotkeys/*.cpp"
    "Windows/Pathfinding/*.h"
    "Windows/Pathfinding/*.hpp"
    "Windows/Pathfinding/*.cpp"
    )
    
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})
target_sources(GWToolboxdll PRIVATE ${SOURCES})

# Wasm/*.cpp is the Emscripten-only mod_init scaffold (see Wasm/main_wasm.cpp) --
# never compiled into this native target (uses GLES3/emscripten_sleep, not part of
# the D3D9 build), but added HEADER_FILE_ONLY so it's visible and editable in the
# Visual Studio solution instead of only reachable by opening the file directly.
# The build_wasm project below (also in this solution) is what actually compiles it.
file(GLOB WASM_SOURCES CONFIGURE_DEPENDS
    "Wasm/*.h"
    "Wasm/*.cpp"
    )
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${WASM_SOURCES})
target_sources(GWToolboxdll PRIVATE ${WASM_SOURCES})
set_source_files_properties(${WASM_SOURCES} PROPERTIES HEADER_FILE_ONLY ON)

# A separate, explicitly-triggered project (Solution Explorer -> build_wasm ->
# Build) rather than something that runs on every native build: building wasm
# needs EMSDK activated and is far slower than an incremental native build, so
# it should not be an implicit side effect of building GWToolboxdll/GWToolbox.
# GWToolbox's own POST_BUILD step (see GWToolbox/CMakeLists.txt) still keeps
# things in sync automatically for whichever config you build there; this is
# for rebuilding the wasm side on its own and pushing it into every config's
# bin/ folder at once. See tools/sync_gwmod.py.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
# $<CONFIG> is whichever native config (Debug/Release/RelWithDebInfo) is selected
# in the IDE when you build this project -- forwarded so building build_wasm
# while Debug is selected embeds GWCA's debug wasm (gwcad.wasm) instead of
# always embedding the release one. See GWTOOLBOX_WASM_GWCA_DEBUG above.
add_custom_target(build_wasm
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/sync_gwmod.py"
            build-all-configs "${PROJECT_SOURCE_DIR}" "$<CONFIG>"
    COMMENT "Building the wasm mod ($<CONFIG>) and syncing gwtoolbox.gwmod into every bin/<Config> folder"
    VERBATIM
    SOURCES ${WASM_SOURCES})

# Native Gw.dat reader/decoders (Utils/GwDat). Added explicitly rather than via the
# Utils glob so the reverse-engineered ATEX/xentax sources can build with warnings
# relaxed and without the precompiled header (they are self-contained).
file(GLOB GWDAT_SOURCES CONFIGURE_DEPENDS
    "Utils/GwDat/*.h"
    "Utils/GwDat/*.cpp"
    )
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${GWDAT_SOURCES})
target_sources(GWToolboxdll PRIVATE ${GWDAT_SOURCES})
set_source_files_properties(${GWDAT_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/Utils/GwDat/xentax.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Utils/GwDat/AtexAsm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Utils/GwDat/AtexReader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Utils/GwDat/AtexDecompress.cpp"
    PROPERTIES COMPILE_FLAGS "/W0")

# Re-embed the current Dependencies/GWCA/bin/gwca.dll on every build
set(GWCA_EMBED_DLL "${CMAKE_SOURCE_DIR}/Dependencies/GWCA/bin/gwca.dll")
set(GWCA_EMBED_STAMP "${CMAKE_CURRENT_BINARY_DIR}/gwca_embed_stamp.h")
if(NOT EXISTS "${GWCA_EMBED_STAMP}")
    file(WRITE "${GWCA_EMBED_STAMP}" "#define GWCA_EMBED_HASH \"\"\n")
endif()
add_custom_target(gwca_embed_stamp ALL
    COMMAND "${CMAKE_COMMAND}"
            -D "GWCA_EMBED_DLL=${GWCA_EMBED_DLL}"
            -D "GWCA_EMBED_STAMP=${GWCA_EMBED_STAMP}"
            -P "${CMAKE_SOURCE_DIR}/cmake/embed_gwca_stamp.cmake"
    BYPRODUCTS "${GWCA_EMBED_STAMP}"
    COMMENT "Checking embedded gwca.dll is current"
    VERBATIM)
add_dependencies(GWToolboxdll gwca_embed_stamp)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/GWToolbox.rc"
    PROPERTIES OBJECT_DEPENDS "${GWCA_EMBED_DLL};${GWCA_EMBED_STAMP}")

# auto-generated files - export header & pch 
include(GenerateExportHeader)
generate_export_header(GWToolboxdll)
#target_sources(GWToolboxdll PRIVATE FILE_SET generated_header TYPE HEADERS BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR} FILES ${CMAKE_CURRENT_BINARY_DIR}/gwtoolboxdll_export.h)
target_sources(GWToolboxdll PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gwtoolboxdll_export.h)
target_include_directories(GWToolboxdll PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/gwtoolboxdll_export.h)

target_precompile_headers(GWToolboxdll PRIVATE "stdafx.h")
source_group(Generated REGULAR_EXPRESSION cmake_pch)

target_compile_definitions(GWToolboxdll PRIVATE
    "_USRDLL"
    "GWCA_IMPORT"
    )
      
target_compile_options(GWToolboxdll PRIVATE /Gy /utf-8)
target_compile_options(GWToolboxdll PRIVATE $<$<CONFIG:RelWithDebInfo>:/Zi>)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Skip /W4 for clang-cl: it expands to -Wall -Wextra
    target_compile_options(GWToolboxdll PRIVATE $<$<CONFIG:Debug>:/Z7 /Od>)
else()
    target_compile_options(GWToolboxdll PRIVATE /W4 /WX)
    target_compile_options(GWToolboxdll PRIVATE $<$<CONFIG:Debug>:/ZI /Od>)
endif()

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
    ) 

include(imgui_fonts)

find_package(minhook CONFIG REQUIRED)

find_package(unofficial-uwebsockets CONFIG REQUIRED)
find_package(recastnavigation CONFIG REQUIRED)

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
    glaze::glaze
    imgui::fonts
    IconFontCppHeaders
    nativefiledialog
    wintoast
	minhook::minhook
	ctre::ctre
    unofficial::uwebsockets::uwebsockets
    RecastNavigation::Detour

    # libs:
    dbghelp.lib # for MiniDump
    bcrypt.lib  # for SHA256 (Resources.cpp)
	delayimp # for delay loading gwca
    )

file(GLOB FXC_PATHS "C:/Program Files (x86)/Windows Kits/10/bin/*/x86")
find_program(FXC fxc DOC "hlsl compiler" PATHS ${FXC_PATHS})
if(NOT FXC)
    message(SEND_ERROR "unable to find Fxc.exe for HLSL compilation")
endif(NOT FXC)

file(GLOB HLSL_VERTEX_SHADER_FILES CONFIGURE_DEPENDS
    Widgets/Minimap/Shaders/*_vs.hlsl
)
file(GLOB HLSL_PIXEL_SHADER_FILES CONFIGURE_DEPENDS
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

# compress.ps1 needs PowerShell, which isn't available when configuring from a non-Windows
# host (e.g. the wine cross-build in scripts/build-wine-prefix.sh); gzip is a POSIX
# equivalent for the RelWithDebInfo-only PDB compression it does.
if(CMAKE_HOST_WIN32)
    add_custom_command(TARGET GWToolboxdll POST_BUILD
        COMMAND powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/compress.ps1"
                "$<TARGET_PDB_FILE:GWToolboxdll>"
                "$<CONFIG>"
        COMMENT "Compressing PDB for distribution (RelWithDebInfo only)"
        VERBATIM
    )
else()
    add_custom_command(TARGET GWToolboxdll POST_BUILD
        COMMAND $<IF:$<CONFIG:RelWithDebInfo>,gzip,${CMAKE_COMMAND}>
                $<IF:$<CONFIG:RelWithDebInfo>,-kf,-E>
                $<IF:$<CONFIG:RelWithDebInfo>,$<TARGET_PDB_FILE:GWToolboxdll>,true>
        COMMENT "Compressing PDB for distribution (RelWithDebInfo only)"
        VERBATIM
    )
endif()
