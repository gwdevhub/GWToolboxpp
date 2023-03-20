include_guard()
include(FetchContent)

FetchContent_Declare(
    nativefiledialog
    GIT_REPOSITORY https://github.com/mlabbe/nativefiledialog.git
    GIT_TAG release_116)
FetchContent_GetProperties(nativefiledialog)
if (nativefiledialog_POPULATED)
    return()
endif()

FetchContent_Populate(nativefiledialog)

add_library(nativefiledialog INTERFACE)
	
target_include_directories(nativefiledialog INTERFACE "${nativefiledialog_SOURCE_DIR}/src")
