include_guard()
include(FetchContent)

FetchContent_Declare(
    DirectXTex
    GIT_REPOSITORY https://github.com/microsoft/DirectXTex
    GIT_TAG jul2022)
FetchContent_GetProperties(directxtex)
if (directxtex_POPULATED)
    return()
endif()

FetchContent_Populate(directxtex)

add_library(directxtex)
set(SOURCES
    "${directxtex_SOURCE_DIR}/DDSTextureLoader/DDSTextureLoader9.h"
    "${directxtex_SOURCE_DIR}/DDSTextureLoader/DDSTextureLoader9.cpp"
    "${directxtex_SOURCE_DIR}/WICTextureLoader/WICTextureLoader9.h"
    "${directxtex_SOURCE_DIR}/WICTextureLoader/WICTextureLoader9.cpp"
    )
source_group(TREE ${directxtex_SOURCE_DIR} FILES ${SOURCES})
target_sources(directxtex PRIVATE ${SOURCES})
target_include_directories(directxtex PUBLIC "${directxtex_SOURCE_DIR}")

set_target_properties(directxtex PROPERTIES FOLDER "Dependencies/")
