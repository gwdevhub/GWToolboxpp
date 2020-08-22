if (TARGET iconfontheaders)
    return()
endif()

include_guard()
include(FetchContent)

FetchContent_Declare(
    iconfontheaders
    GIT_REPOSITORY https://github.com/juliettef/IconFontCppHeaders.git
    GIT_TAG 8cac2b5d9cfc566d3fa90bf6820a95e3c5ed5478
    )
FetchContent_MakeAvailable(iconfontheaders)

add_library(iconfontheaders INTERFACE)
target_include_directories(iconfontheaders INTERFACE ${iconfontheaders_SOURCE_DIR})
