file(READ "${PROJECT_SOURCE_DIR}/tools/bake_cartography/placed_maps.txt" PLACED_MAPS)
configure_file(PlacedMaps.h.in "${CMAKE_CURRENT_BINARY_DIR}/PlacedMaps.h" @ONLY)

add_executable(bake_pathing EXCLUDE_FROM_ALL
    main.cpp
    DatArchive.cpp
    DatArchive.h
    gw_inflate.c
    gw_inflate.h
    "${PROJECT_SOURCE_DIR}/GWToolboxdll/Utils/ArenaNetFileParser.cpp"
    "${PROJECT_SOURCE_DIR}/GWToolboxdll/Utils/GwDat/xentax.cpp"
    "${PROJECT_SOURCE_DIR}/GWToolboxdll/Windows/Pathfinding/PathingMapDataLoader.h"
    "${CMAKE_CURRENT_BINARY_DIR}/PlacedMaps.h")

target_include_directories(bake_pathing PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/GWToolboxdll"
    "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/include")
target_compile_options(bake_pathing PRIVATE -Wall -Wextra)
set_source_files_properties("${PROJECT_SOURCE_DIR}/GWToolboxdll/Utils/GwDat/xentax.cpp"
    PROPERTIES COMPILE_OPTIONS -w)
set_target_properties(bake_pathing PROPERTIES OUTPUT_NAME "bake_pathing")
# The entry point is wmain, which MinGW only links when told to.
target_link_options(bake_pathing PRIVATE -municode)
# An offline tool that reports failures by throwing.
target_compile_options(bake_pathing PRIVATE ${GWTB_EXCEPTIONS})
