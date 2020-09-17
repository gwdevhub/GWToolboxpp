include_guard()
include(FetchContent)

FetchContent_Declare(
    earcut
    GIT_REPOSITORY https://github.com/mapbox/earcut.hpp.git
    GIT_TAG a3807695eb92d208643e15e112982b482ce36c92)
FetchContent_GetProperties(earcut)
if (earcut_POPULATED)
    return()
endif()

FetchContent_Populate(earcut)

add_library(earcut INTERFACE)
add_library(mapbox::earcut ALIAS earcut)

set(SOURCES
    "${earcut_SOURCE_DIR}/include/mapbox/earcut.hpp")
source_group(TREE "${earcut_SOURCE_DIR}" FILES ${SOURCES})
target_sources(earcut INTERFACE ${SOURCES})
target_include_directories(earcut INTERFACE "${earcut_SOURCE_DIR}/include")
