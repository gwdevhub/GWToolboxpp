include_guard()
include(FetchContent)

FetchContent_Declare(
    simpleini
    GIT_REPOSITORY https://github.com/brofield/simpleini.git
    GIT_TAG v4.19)
FetchContent_GetProperties(simpleini)
if (simpleini_POPULATED)
    return()
endif()

FetchContent_Populate(simpleini)

add_library(simpleini INTERFACE)
target_include_directories(simpleini INTERFACE "${simpleini_SOURCE_DIR}")
