include_guard()
include(FetchContent)

FetchContent_Declare(
    utf8proc
    GIT_REPOSITORY https://github.com/JuliaStrings/utf8proc.git
    GIT_TAG v2.7.0)
FetchContent_GetProperties(utf8proc)
if (utf8proc_POPULATED)
    return()
endif()

FetchContent_Populate(utf8proc)

set(SOURCES 
    "${utf8proc_SOURCE_DIR}/utf8proc.h"
    "${utf8proc_SOURCE_DIR}/utf8proc.c"
    )

add_library(utf8proc)
target_sources(utf8proc PRIVATE ${SOURCES})
target_include_directories(utf8proc PUBLIC "${utf8proc_SOURCE_DIR}")
target_compile_definitions(utf8proc PUBLIC UTF8PROC_STATIC)

set_target_properties(utf8proc PROPERTIES FOLDER "Dependencies/")
