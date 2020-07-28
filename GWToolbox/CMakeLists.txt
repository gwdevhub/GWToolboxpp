

FILE(GLOB SOURCES 
    "*.h" 
    "*.cpp")

add_executable(GWToolbox)
target_sources(GWToolbox PRIVATE ${SOURCES})

target_link_libraries(GWToolbox PRIVATE GWToolboxdll)
