cmake_minimum_required(VERSION 3.29)

project(gwtoolbox)

set(GWTOOLBOXEXE_VERSION "4.8")
set(GWTOOLBOXEXE_MODULE_VERSION "4,8,0,0") # used for exe module info. See GWToolbox/GWToolbox.rc
set(GWTOOLBOXDLL_VERSION "8.35")
set(GWTOOLBOXDLL_VERSION_BETA "") # can be anything. Marks beta version if not empty.
set(GWTOOLBOX_MODULE_VERSION "8,35,0,0")


# Everything below this point (toolchain checks, compiler flags, native-only dependency
# acquisition) assumes the llvm-mingw Win32 build -- none of it applies configuring for Emscripten.
if(EMSCRIPTEN)
    message(STATUS "Configuring for Emscripten: native-only dependencies "
        "(DirectXTex, Steamworks) are skipped.")

    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

    # imgui (portable core + the stock GLES3 backend, see cmake/imgui.cmake) is the
    # one native-side dependency this configure still needs. Everything else
    # GWToolboxdll/CMakeLists.txt's wasm branch links is header-only or part of the
    # explicitly-curated wasm source list itself.
    include(imgui)

    # GWToolboxdll is the only wasm-relevant target: this is an MVP scaffold (mod_init,
    # the render pipeline, a demo window -- see GWToolboxdll/Wasm/), not the real
    # product yet, so Core/RestClient/GWToolbox (the native injector exe) stay out of
    # this configure until something wasm-side actually needs them.
    add_subdirectory(GWToolboxdll)
else()

if(NOT (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"))
    message(FATAL_ERROR "GWToolbox builds with llvm-mingw only: configure with one of the presets "
        "(relwithdebinfo, debug, release) after running the bootstrap script for this platform.")
endif()
set(GWTB_MINGW ON)

# Increase constexpr and template instantiation depth for glaze's compile-time reflection
add_compile_options(-fconstexpr-depth=2048 -fconstexpr-steps=10000000 -ftemplate-depth=2048)

if(NOT(CMAKE_SIZEOF_VOID_P EQUAL 4))
    message(FATAL_ERROR "You are configuring a non 32-bit build, this is not supported. Run cmake with `-A Win32`")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Parallel compiles are capped by memory as well as cores: the heaviest files peak at about 0.8 GB, and 4 GB is left
# for the IDE and desktop. A Ninja job pool holds whatever -j the IDE or script passes.
cmake_host_system_information(RESULT GWTB_CORES QUERY NUMBER_OF_LOGICAL_CORES)
cmake_host_system_information(RESULT GWTB_MEMORY_MB QUERY TOTAL_PHYSICAL_MEMORY)
math(EXPR GWTB_COMPILE_JOBS "(${GWTB_MEMORY_MB} - 4096) / 1024")
if(GWTB_COMPILE_JOBS GREATER GWTB_CORES)
    set(GWTB_COMPILE_JOBS ${GWTB_CORES})
elseif(GWTB_COMPILE_JOBS LESS 1)
    set(GWTB_COMPILE_JOBS 1)
endif()
set_property(GLOBAL APPEND PROPERTY JOB_POOLS compile=${GWTB_COMPILE_JOBS})
set(CMAKE_JOB_POOL_COMPILE compile)

add_compile_definitions(
    "NOMINMAX"
    "WIN32_LEAN_AND_MEAN"
    # MinGW's dshow.h includes strsafe.h, whose deprecation macros would rewrite strcpy/swprintf calls.
    "STRSAFE_NO_DEPRECATE"
    # glaze's zmij float writer uses _mm_cvtsi128_si64, an x64-only intrinsic, when SSE is detected; we build x86
    "ZMIJ_USE_SIMD=0"
    # glaze only reads and writes settings and a few JSON files; its forced inlining made it a quarter of the dll's code.
    "GLZ_DISABLE_ALWAYS_INLINE"

    # Dev test tooling (TestHarness module + extra pathfinding diagnostics) is gated on _DEBUG only, so
    # it is compiled out of the shipped RelWithDebInfo/Release builds. Debug now also mirrors the log to
    # log.txt (Logger.cpp) so the harness can read it. Define GWTB_HARNESS here to opt a non-Debug build
    # into the harness, but it is OFF by default (the gates are `_DEBUG || GWTB_HARNESS`).

    "GWTOOLBOXEXE_VERSION=\"${GWTOOLBOXEXE_VERSION}\""
    "GWTOOLBOXEXE_MODULE_VERSION=${GWTOOLBOXEXE_MODULE_VERSION}"
    "GWTOOLBOXDLL_VERSION=\"${GWTOOLBOXDLL_VERSION}\""
    "GWTOOLBOXDLL_VERSION_BETA=\"${GWTOOLBOXDLL_VERSION_BETA}\""
    "GWTOOLBOX_MODULE_VERSION=${GWTOOLBOX_MODULE_VERSION}"
    )
	
# Stored in a variable so targets that turn on -Wall/-Wextra can re-apply them after it.
set(CLANG_WARNING_DISABLES
    # glaze's exported INTERFACE_COMPILE_OPTIONS include MSVC-only switches
    # (/Zc:preprocessor, /Zc:lambda, etc.)
    -Wno-unused-command-line-argument

    # Microsoft-specific extensions accepted by MSVC that clang flags
    -Wno-microsoft-cast
    -Wno-microsoft-include
    -Wno-nonportable-include-path
    -Wno-pragma-once-outside-header
    -Wno-backslash-newline-escape
    -Wno-unknown-attributes
    -Wno-ignored-attributes

    # Pointer / cast conversions
    -Wno-pointer-bool-conversion
    -Wno-int-to-void-pointer-cast
    -Wno-cast-function-type-mismatch

    # Unused code (kept on purpose in this codebase)
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-unused-private-field
    -Wno-unused-lambda-capture
    -Wno-unused-const-variable
    -Wno-unused-value
    -Wno-unused-but-set-variable
    -Wno-unused-parameter
    -Wno-unused-local-typedef
    -Wno-unused-but-set-parameter
    -Wno-missing-declarations

    # Cosmetic / stylistic
    -Wno-ignored-qualifiers
    -Wno-missing-field-initializers
    -Wno-missing-designated-field-initializers
    -Wno-missing-braces
    -Wno-braced-scalar-init
    -Wno-reorder-ctor
    -Wno-inconsistent-missing-override
    -Wno-parentheses
    -Wno-logical-op-parentheses
    -Wno-bitwise-op-parentheses
    -Wno-misleading-indentation

    # Switch / enum coverage (deliberate fall-through in this codebase)
    -Wno-switch

    # Format strings (ImGui::Text(buf) style calls — buf never contains user-controlled %)
    -Wno-format-security
    -Wno-format
    -Wno-format-extra-args

    # Char/sign comparison (codebase only uses ASCII subscripts)
    -Wno-char-subscripts
    -Wno-sign-compare
    -Wno-tautological-constant-out-of-range-compare
    -Wno-multichar
    -Wno-sizeof-array-argument

    # Defaulted/deleted special members + non-virtual dtor through base
    -Wno-defaulted-function-deleted
    -Wno-delete-non-abstract-non-virtual-dtor
)
add_compile_options(${CLANG_WARNING_DISABLES})

# Optimise for size by default; per-frame code opts back into speed with GWTB_SPEED_OPTIMISATION. clang's -Oz is
# both smaller and quicker to compile than -Os.
# Windows 10 API level (BCryptHash and friends); MinGW defaults lower.
add_compile_definitions(_WIN32_WINNT=0x0A00 WINVER=0x0A00)
set(GWTB_SIZE_OPTIMISATION -Oz)
set(GWTB_SPEED_OPTIMISATION -O2)
add_compile_options(-mavx2 -mfma -mfpmath=sse -ffunction-sections -fdata-sections)
# No C++ exceptions: their unwind tables were a third of the dll. The few files that catch one build with
# GWTB_EXCEPTIONS instead (see GWToolboxdll/CMakeLists.txt).
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
set(GWTB_EXCEPTIONS -fexceptions)
# No RTTI either: nothing uses typeid or dynamic_cast (TBHotkey::AsGroup and Pcon::AsGeneric stand in), and
# clang still emits type information for the exceptions those files catch.
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
add_link_options(-Wl,--gc-sections $<$<NOT:$<CONFIG:Debug>>:-Wl,--icf=all>)
add_compile_options($<$<NOT:$<CONFIG:Debug>>:${GWTB_SIZE_OPTIMISATION}>)

# Don't raise this: the MinGW build uses GCC 13's libstdc++, which supports nothing above C++23.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# No C++20 modules are used, and scanning for them costs a full extra preprocessing pass per file.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)


list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# windres re-parses -D values through a shell and drops their quotes, so .rc files take version strings from here.
set(GWTOOLBOX_RESOURCE_INCLUDE_DIR "${PROJECT_BINARY_DIR}/generated")
configure_file("${PROJECT_SOURCE_DIR}/cmake/ResourceVersion.h.in" "${GWTOOLBOX_RESOURCE_INCLUDE_DIR}/ResourceVersion.h" @ONLY)

# Outputs dll, exe, and pdb into a /bin/config folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>")

include(gwca)
include(directxtexloader)
include(easywsclient)
include(imgui)
include(gwtoolboxdll_plugins)
include(nativefiledialog)
include(steamworks_sdk)

find_path(EARCUT_HPP_INCLUDE_DIRS "mapbox/earcut.hpp")
find_package(glaze CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)

add_subdirectory(GWToolboxdll)
add_subdirectory(Core)
add_subdirectory(RestClient)
add_subdirectory(GWToolbox)
add_subdirectory(bake_pathing)

endif() # EMSCRIPTEN
