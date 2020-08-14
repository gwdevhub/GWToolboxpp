include_guard()

set(UTF8PROC_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/utf8proc/")

set(SOURCES 
    "${UTF8PROC_FOLDER}/utf8proc.h"
    "${UTF8PROC_FOLDER}/utf8proc.c")

add_library(utf8proc)
target_sources(utf8proc PRIVATE ${SOURCES})
target_include_directories(utf8proc PUBLIC "${UTF8PROC_FOLDER}")
target_compile_definitions(utf8proc PUBLIC UTF8PROC_STATIC)


set_target_properties(utf8proc PROPERTIES FOLDER "Dependencies/")
