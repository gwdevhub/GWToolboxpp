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
    add_library(GWToolbox_DLL SHARED
        Wasm/main_wasm.cpp
        Wasm/Logger_Wasm.cpp
    )
    set_target_properties(GWToolbox_DLL PROPERTIES
        PREFIX "" OUTPUT_NAME "GWToolboxdll" SUFFIX ".wasm")
    # Ninja + CXX_STANDARD 23 would trigger C++20 module dependency scanning, which
    # emscan-deps cannot answer; this target uses no modules. Same note as GWCA's own
    # CMakeLists and kamadanwasm's.
    set_target_properties(GWToolbox_DLL PROPERTIES CXX_SCAN_FOR_MODULES OFF)

    target_include_directories(GWToolbox_DLL PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

    target_precompile_headers(GWToolbox_DLL PRIVATE "stdafx.h")

    target_compile_definitions(GWToolbox_DLL PRIVATE GWCA_WASM=1)
    # -fshort-wchar: GWCA's structs mirror the game's UTF-16 (2-byte wchar_t) layout;
    # clang defaults to 4. Matches kamadanwasm's CMakeLists note on the same flag.
    target_compile_options(GWToolbox_DLL PRIVATE -fshort-wchar -fdeclspec)

    target_include_directories(GWToolbox_DLL PRIVATE
        "${PROJECT_SOURCE_DIR}/Dependencies"
        "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/include"
        "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/source"
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(GWToolbox_DLL PRIVATE imgui)

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
    target_link_options(GWToolbox_DLL PRIVATE
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

    # Packaging is its own target, not just a build step of GWToolbox_DLL: the
    # artifact gw_in_browser's loader actually takes is the .gwmod bundle
    # (GWToolboxdll.wasm + the gwca.wasm it was built against, zipped with a
    # manifest -- see tools/package.py), not the bare .wasm, and giving it a
    # name (gwtoolbox_gwmod) makes it independently buildable/discoverable
    # (`cmake --build build/<host>/wasm --target gwtoolbox_gwmod`) rather than only
    # ever firing as a side effect of building GWToolboxdll. Still runs on a
    # plain build too -- it's in ALL -- so nothing about the default flow changes.
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(GWTOOLBOX_WASM_DIST_DIR "${PROJECT_SOURCE_DIR}/bin"
        CACHE PATH "where gwtoolbox.gwmod is written")

    # GWCA's create_release also builds gwcad.wasm (DEBUG_POSTFIX "d", asserts +
    # [SCAN] logging compiled in -- see GWCA/CMakeLists.txt's wasm-debug preset),
    # same "d"-suffix convention as the native gwcad.dll/gwcad.lib. Off by default
    # (the ordinary `cmake --build build/<host>/wasm` flow keeps embedding the release
    # gwca.wasm); GWToolboxpp2's build_wasm project (GWToolboxdll/CMakeLists.txt's
    # native branch) sets this ON when the VS config it was triggered from is
    # Debug -- see tools/sync_gwmod.py.
    option(GWTOOLBOX_WASM_GWCA_DEBUG "Embed GWCA's debug wasm build (gwcad.wasm) instead of the release one" OFF)
    if(GWTOOLBOX_WASM_GWCA_DEBUG)
        set(GWTOOLBOX_WASM_GWCA_FILE "gwcad.wasm")
    else()
        set(GWTOOLBOX_WASM_GWCA_FILE "gwca.wasm")
    endif()
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/wasm/${GWTOOLBOX_WASM_GWCA_FILE}")
        message(FATAL_ERROR "The pinned native GWCA release has no wasm binary. Supply a matching ${GWTOOLBOX_WASM_GWCA_FILE} before building Toolbox for wasm.")
    endif()

    add_custom_command(
        OUTPUT "${GWTOOLBOX_WASM_DIST_DIR}/gwtoolbox.gwmod"
               "${GWTOOLBOX_WASM_DIST_DIR}/gwtoolbox.modfile.json"
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/package.py"
                --dll  "$<TARGET_FILE:GWToolbox_DLL>"
                --gwca "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/wasm/${GWTOOLBOX_WASM_GWCA_FILE}"
                --out  "${GWTOOLBOX_WASM_DIST_DIR}"
                # Same version string the native dll's PE resource carries (see
                # GWToolboxdll/GWToolbox.rc's ProductVersion, sourced from this
                # same CMake variable) -- stamped into the bundled manifest.json
                # since a .gwmod has no PE resource of its own to carry one.
                --version "${GWTOOLBOXDLL_VERSION}"
        DEPENDS GWToolbox_DLL "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/wasm/${GWTOOLBOX_WASM_GWCA_FILE}"
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

add_library(GWToolbox_DLL SHARED)
set_target_properties(GWToolbox_DLL PROPERTIES PREFIX "" OUTPUT_NAME "GWToolboxdll")
target_compile_options(GWToolbox_DLL PRIVATE -gdwarf-4)
find_program(GWTOOLBOX_LLVM_OBJCOPY NAMES llvm-objcopy llvm-objcopy.exe HINTS "${LLVM_MINGW_ROOT}/bin" REQUIRED)
add_custom_command(TARGET GWToolbox_DLL POST_BUILD
    COMMAND "${GWTOOLBOX_LLVM_OBJCOPY}" --only-keep-debug "$<TARGET_FILE:GWToolbox_DLL>" "$<TARGET_FILE_DIR:GWToolbox_DLL>/GWToolboxdll.debug"
    COMMAND "${GWTOOLBOX_LLVM_OBJCOPY}" --strip-all "$<TARGET_FILE:GWToolbox_DLL>"
    VERBATIM)

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
# Per-frame code is built for speed in GWToolboxdll_per_frame and GWToolboxdll_pathfinding below.
file(GLOB PER_FRAME_SOURCES CONFIGURE_DEPENDS "Widgets/Minimap/*.cpp" "Utils/GameWorldCompositor.cpp")
file(GLOB PATHFINDING_SOURCES CONFIGURE_DEPENDS "Windows/Pathfinding/*.cpp")
list(REMOVE_ITEM SOURCES ${PER_FRAME_SOURCES} ${PATHFINDING_SOURCES})
# These catch C++ exceptions, so they keep them: the crash handler reports one that escaped, SearchPattern rejects a
# regex the user typed that doesn't compile, and the prop index drops a snapshot it ran out of memory on. clang won't
# use the shared precompiled header across that setting.
if(GWTB_EXCEPTIONS)
    set_property(SOURCE Modules/CrashHandler.cpp Utils/TextUtils_SearchPattern.cpp Utils/PropSurfaceIndex.cpp APPEND PROPERTY COMPILE_OPTIONS ${GWTB_EXCEPTIONS})
    set_property(SOURCE Modules/CrashHandler.cpp Utils/TextUtils_SearchPattern.cpp Utils/PropSurfaceIndex.cpp PROPERTY SKIP_PRECOMPILE_HEADERS ON)
endif()
target_sources(GWToolbox_DLL PRIVATE ${SOURCES})

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
target_sources(GWToolbox_DLL PRIVATE ${WASM_SOURCES})
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
target_sources(GWToolbox_DLL PRIVATE ${GWDAT_SOURCES})
set_source_files_properties(${GWDAT_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/Utils/GwDat/xentax.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Utils/GwDat/AtexAsm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Utils/GwDat/AtexReader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Utils/GwDat/AtexDecompress.cpp"
    PROPERTIES COMPILE_OPTIONS -w)

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
add_dependencies(GWToolbox_DLL gwca_embed_stamp)
# The larger resources are embedded LZ4-compressed (Utils/Lz4.cpp decompresses them): 680 KB down to 330 KB.
set(PACKED_RESOURCES)
foreach(RESOURCE "${GWCA_EMBED_DLL}" "${CMAKE_CURRENT_SOURCE_DIR}/Windows/Pathfinding/portal_connections.json")
    get_filename_component(RESOURCE_NAME "${RESOURCE}" NAME)
    set(PACKED "${CMAKE_CURRENT_BINARY_DIR}/${RESOURCE_NAME}.lz4")
    add_custom_command(OUTPUT "${PACKED}"
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/lz4_pack.py" "${RESOURCE}" "${PACKED}"
        DEPENDS "${RESOURCE}" "${PROJECT_SOURCE_DIR}/tools/lz4_pack.py"
        COMMENT "Compressing ${RESOURCE_NAME}"
        VERBATIM)
    list(APPEND PACKED_RESOURCES "${PACKED}")
endforeach()
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/GWToolbox.rc"
    PROPERTIES OBJECT_DEPENDS "${PACKED_RESOURCES};${GWCA_EMBED_STAMP}")

target_include_directories(GWToolbox_DLL PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

target_precompile_headers(GWToolbox_DLL PRIVATE "stdafx.h")
source_group(Generated REGULAR_EXPRESSION cmake_pch)

target_compile_definitions(GWToolbox_DLL PRIVATE
    "_USRDLL"
    )
      

target_include_directories(GWToolbox_DLL PRIVATE
    "${PROJECT_SOURCE_DIR}/Dependencies"
    "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${GWTOOLBOX_RESOURCE_INCLUDE_DIR}"
    )

include(imgui_fonts)


find_package(recastnavigation CONFIG REQUIRED)

set(GWTOOLBOXDLL_LIBRARIES
    # cmake targets:
    Core
    RestClient
    imgui
    Microsoft::DirectXTex
	directxtexloader
    gwca
    steamworks_sdk
    easywsclient
    glaze::glaze
    imgui::fonts
    IconFontCppHeaders
    nativefiledialog
    RecastNavigation::Detour

    # libs:
    dbghelp # for MiniDump
    bcrypt  # for SHA256 (Resources.cpp)
    version
    strmiids
    gdi32
    dwmapi
    ws2_32  # IRC, Teamspeak and the objective timer's WebSocketServer
    )
target_link_libraries(GWToolbox_DLL PRIVATE ${GWTOOLBOXDLL_LIBRARIES})

foreach(lib IN LISTS GWTOOLBOXDLL_LIBRARIES)
    if(TARGET ${lib})
        get_target_property(lib_type ${lib} TYPE)
        get_target_property(lib_alias ${lib} ALIASED_TARGET)
        get_target_property(lib_imported ${lib} IMPORTED)
        if(NOT lib_imported AND NOT lib_alias AND NOT lib_type STREQUAL "INTERFACE_LIBRARY")
            target_compile_options(${lib} PRIVATE "$<$<CONFIG:Debug>:-gdwarf-4>")
        endif()
    endif()
endforeach()

# The per-frame code is built at -O2 while the rest is optimised for size, and clang rejects a precompiled header
# built with other -O or exception settings. Each such group is its own target with its own copy of the header;
# without one these were the slowest files to build. It compiles with GWToolbox_DLL's settings and is linked into it.
function(gwtoolboxdll_add_part NAME)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;OPTIONS")
    add_library(${NAME} OBJECT ${ARG_SOURCES})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ARG_SOURCES})
    foreach(PROPERTY INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS)
        set_property(TARGET ${NAME} PROPERTY ${PROPERTY} $<TARGET_PROPERTY:GWToolbox_DLL,${PROPERTY}>)
    endforeach()
    target_link_libraries(${NAME} PRIVATE ${GWTOOLBOXDLL_LIBRARIES})
    target_compile_options(${NAME} PRIVATE ${ARG_OPTIONS})
    target_precompile_headers(${NAME} PRIVATE "stdafx.h")
    target_link_libraries(GWToolbox_DLL PRIVATE ${NAME})
    add_dependencies(${NAME} compile_shaders)
endfunction()
set(SPEED_OPTIMISATION $<$<NOT:$<CONFIG:Debug>>:${GWTB_SPEED_OPTIMISATION}>)
gwtoolboxdll_add_part(GWToolboxdll_per_frame SOURCES ${PER_FRAME_SOURCES} OPTIONS ${SPEED_OPTIMISATION})
# Building the path graphs recovers from running out of memory by catching std::bad_alloc, so the whole of
# pathfinding keeps exceptions: every frame between the allocation and the catch needs unwind tables.
gwtoolboxdll_add_part(GWToolboxdll_pathfinding SOURCES ${PATHFINDING_SOURCES} OPTIONS ${SPEED_OPTIMISATION} ${GWTB_EXCEPTIONS})

# Only 1.19 has actually been validated against these shaders; 1.6 (the version most
# distros still package) miscompiles them. Raise the floor, never lower it, without testing.
set(VKD3D_MINIMUM_VERSION 1.19)

get_filename_component(WINDOWS_KITS_ROOT "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE)
file(GLOB FXC_PATHS "${WINDOWS_KITS_ROOT}/bin/*/x86" "C:/Program Files (x86)/Windows Kits/10/bin/*/x86")
find_program(FXC fxc DOC "hlsl compiler" PATHS ${FXC_PATHS})
if(FXC)
    set(HLSL_COMPILE_COMMAND ${FXC})
elseif(CMAKE_HOST_WIN32)
    # Without the Windows SDK, call D3DCompile in Windows' own d3dcompiler_47.dll - the compiler fxc.exe wraps.
    set(HLSL_COMPILE_COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/hlsl_compile.py" --d3dcompiler)
    message(STATUS "HLSL: using d3dcompiler_47.dll (fxc not found)")
else()
    # No fxc: fall back to vkd3d-shader, the only non-Windows HLSL compiler that emits
    # d3dbc (shader model 1-3). dxc dropped every profile below SM6, so it can't be used.
    if(NOT VKD3D_COMPILER AND DEFINED ENV{VKD3D_COMPILER})
        set(VKD3D_COMPILER "$ENV{VKD3D_COMPILER}" CACHE FILEPATH "vkd3d-shader HLSL compiler, fxc stand-in")
    endif()
    # The bootstrap scripts build it into the toolchain folder.
    find_program(VKD3D_COMPILER vkd3d-compiler DOC "vkd3d-shader HLSL compiler, fxc stand-in"
        PATHS "${CMAKE_SOURCE_DIR}/.toolchain/vkd3d-${VKD3D_MINIMUM_VERSION}")
    if(NOT VKD3D_COMPILER)
        message(SEND_ERROR "no HLSL compiler found: need either fxc (Windows SDK) or vkd3d-compiler ${VKD3D_MINIMUM_VERSION}+")
    else()
        execute_process(COMMAND ${VKD3D_COMPILER} --version OUTPUT_VARIABLE VKD3D_VERSION_OUTPUT ERROR_QUIET)
        string(REGEX MATCH "vkd3d-shader ([0-9]+\\.[0-9]+)" _ "${VKD3D_VERSION_OUTPUT}")
        # Older releases silently miscompile these shaders ("Unhandled instruction type HLSL_IR_JUMP").
        if(CMAKE_MATCH_1 VERSION_LESS ${VKD3D_MINIMUM_VERSION})
            message(SEND_ERROR "vkd3d-compiler ${CMAKE_MATCH_1} is too old for SM3 HLSL; need ${VKD3D_MINIMUM_VERSION}+")
        endif()
        message(STATUS "HLSL: using vkd3d-compiler ${CMAKE_MATCH_1} (fxc not found)")
    endif()
    set(HLSL_COMPILE_COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/hlsl_compile.py" --vkd3d-compiler ${VKD3D_COMPILER})
endif()

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
                     COMMAND ${HLSL_COMPILE_COMMAND} /nologo /E main /T ${shadertype}_3_0 /Vn ${FILE_NOEXT} /O3 /Zi /Fh ${OUTPUT_FILE} ${FILE}
                     MAIN_DEPENDENCY ${FILE}
                     COMMENT "HLSL ${FILE}"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     VERBATIM)
  list(APPEND SHADER_OUTPUTS ${OUTPUT_FILE})
endforeach()

add_custom_target(compile_shaders DEPENDS ${SHADER_OUTPUTS})

add_dependencies(GWToolbox_DLL compile_shaders)

