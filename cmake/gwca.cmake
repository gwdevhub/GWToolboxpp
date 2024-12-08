include_guard()

set(GWCA_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/GWCA")

find_package(minhook CONFIG REQUIRED)

if(EXISTS "${GWCA_FOLDER}/Source")
    add_library(gwca)
    file(GLOB SOURCES
        "${GWCA_FOLDER}/Source/*.cpp"
        "${GWCA_FOLDER}/Include/GWCA/*.h"
        "${GWCA_FOLDER}/Include/GWCA/Constants/*.h"
        "${GWCA_FOLDER}/Include/GWCA/Context/*.h"
        "${GWCA_FOLDER}/Include/GWCA/GameContainers/*.h"
        "${GWCA_FOLDER}/Include/GWCA/GameEntities/*.h"
        "${GWCA_FOLDER}/Include/GWCA/Managers/*.h"
        "${GWCA_FOLDER}/Include/GWCA/Packets/*.h"
        "${GWCA_FOLDER}/Include/GWCA/Utilities/*.h")
    source_group(TREE "${GWCA_FOLDER}" FILES ${SOURCES})
    target_sources(gwca PRIVATE ${SOURCES})
    target_precompile_headers(gwca PRIVATE "${GWCA_FOLDER}/Include/GWCA/stdafx.h")
    target_include_directories(gwca PUBLIC "${GWCA_FOLDER}/Include/")
    set_target_properties(gwca PROPERTIES CXX_STANDARD 23)
    target_compile_options(gwca PRIVATE /W4 /WX)
    target_link_options(gwca PRIVATE /WX /SAFESEH:NO)
	target_link_libraries(gwca PUBLIC minhook::minhook)
else()
    add_library(gwca STATIC IMPORTED GLOBAL)
	set_target_properties(gwca PROPERTIES
		IMPORTED_LOCATION_DEBUG "${GWCA_FOLDER}/lib/gwcad.lib"
		IMPORTED_LOCATION_RELEASE "${GWCA_FOLDER}/lib/gwca.lib"
		INTERFACE_INCLUDE_DIRECTORIES "${GWCA_FOLDER}/include")
    target_link_libraries(gwca INTERFACE minhook::minhook)
endif()
