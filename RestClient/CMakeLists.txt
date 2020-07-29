include(curl)

FILE(GLOB SOURCES 
    "*.h" 
    "*.cpp")

add_library(RestClient)
target_sources(RestClient PRIVATE ${SOURCES})
target_include_directories(RestClient PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(RestClient PUBLIC 
    curl
    Core)
