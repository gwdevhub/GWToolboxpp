
set(wintoast_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/wintoast/")

add_library(wintoast)

set(SOURCES
    "${wintoast_FOLDER}/wintoastlib.h"
    "${wintoast_FOLDER}/wintoastlib.cpp"
    )
    
target_sources(wintoast PRIVATE ${SOURCES})
target_include_directories(wintoast PUBLIC "${wintoast_FOLDER}")

set_target_properties(wintoast PROPERTIES FOLDER "Dependencies/")
