
FILE(GLOB SOURCES 
    "*.h" 
    "*.cpp")

add_library(Core)
target_sources(Core PRIVATE ${SOURCES})
target_include_directories(Core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
