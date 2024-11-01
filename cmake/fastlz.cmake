
set(fastlz_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/fastlz/")

add_library(fastlz)

set(SOURCES
    "${fastlz_FOLDER}/fastlz.h"
    "${fastlz_FOLDER}/fastlz.cpp"
    )

target_sources(fastlz PRIVATE ${SOURCES})
target_include_directories(fastlz PUBLIC "${fastlz_FOLDER}")

set_target_properties(fastlz PROPERTIES FOLDER "Dependencies/")
