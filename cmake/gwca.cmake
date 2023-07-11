include_guard()

set(GWCA_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/GWCA")

add_library(gwca)

file(GLOB SOURCES
    "${GWCA_FOLDER}/Source/stdafx.h"
    "${GWCA_FOLDER}/Source/*.cpp"
    "${GWCA_FOLDER}/Include/GWCA/*.h"
    "${GWCA_FOLDER}/Include/GWCA/Constants/*.h"
    "${GWCA_FOLDER}/Include/GWCA/Context/*.h"
    "${GWCA_FOLDER}/Include/GWCA/GameContainers/*.h"
    "${GWCA_FOLDER}/Include/GWCA/GameEntities/*.h"
    "${GWCA_FOLDER}/Include/GWCA/Managers/*.h"
    "${GWCA_FOLDER}/Include/GWCA/Packets/*.h"
    "${GWCA_FOLDER}/Include/GWCA/Utilities/*.h")
source_group(TREE "${GWCA_FOLDER}" FILES ${SOURCES})
target_sources(gwca PRIVATE ${SOURCES})
target_precompile_headers(gwca PRIVATE "${GWCA_FOLDER}/Source/stdafx.h")
target_include_directories(gwca PUBLIC "${GWCA_FOLDER}/Include/")
set_target_properties(gwca PROPERTIES CXX_STANDARD 17)
target_compile_options(gwca PRIVATE /W4 /WX)
target_link_options(gwca PRIVATE /WX /SAFESEH:NO)

include(minhook)

target_link_libraries(gwca PUBLIC
    minhook)
