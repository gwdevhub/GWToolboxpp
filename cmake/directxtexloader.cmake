include_guard()
include(FetchContent)

cmake_policy(PUSH)
cmake_policy(SET CMP0169 OLD)
FetchContent_Declare(
    directxtexloader
    GIT_REPOSITORY https://github.com/microsoft/directxtex
    GIT_TAG oct2024)
FetchContent_GetProperties(directxtexloader)
if (directxtexloader_POPULATED)
    return()
endif()

FetchContent_Populate(directxtexloader)
cmake_policy(POP)

add_library(directxtexloader)
set(SOURCES
    "${directxtexloader_SOURCE_DIR}/DDSTextureLoader/DDSTextureLoader9.h"
    "${directxtexloader_SOURCE_DIR}/DDSTextureLoader/DDSTextureLoader9.cpp"
    "${directxtexloader_SOURCE_DIR}/WICTextureLoader/WICTextureLoader9.h"
    "${directxtexloader_SOURCE_DIR}/WICTextureLoader/WICTextureLoader9.cpp"
    )
source_group(TREE ${directxtexloader_SOURCE_DIR} FILES ${SOURCES})
target_sources(directxtexloader PRIVATE ${SOURCES})
target_include_directories(directxtexloader PUBLIC "${directxtexloader_SOURCE_DIR}")

set_target_properties(directxtexloader PROPERTIES FOLDER "Dependencies/")
