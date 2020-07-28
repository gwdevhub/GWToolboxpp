include_guard()

set(MINHOOK_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/GWCA/Dependencies/minhook/")

add_library(minhook)

file(GLOB SOURCES
    "${MINHOOK_FOLDER}/include/*.h"
    "${MINHOOK_FOLDER}/src/*.h"
    "${MINHOOK_FOLDER}/src/*.c"
    "${MINHOOK_FOLDER}/src/hde/*.h"
    "${MINHOOK_FOLDER}/src/hde/*.c")
source_group(TREE "${MINHOOK_FOLDER}" FILES ${SOURCES})
target_sources(minhook PRIVATE ${SOURCES})

target_include_directories(minhook PUBLIC "${MINHOOK_FOLDER}/include/")

set_target_properties(minhook PROPERTIES FOLDER "Dependencies/")
