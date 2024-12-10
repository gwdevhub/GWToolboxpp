include_guard()

set(GWCA_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/GWCA")

find_package(minhook CONFIG REQUIRED)

add_library(gwca STATIC IMPORTED GLOBAL)
set_target_properties(gwca PROPERTIES
	IMPORTED_LOCATION_DEBUG "${GWCA_FOLDER}/lib/gwca.debug.lib"
	IMPORTED_LOCATION_RELEASE "${GWCA_FOLDER}/lib/gwca.lib"
	INTERFACE_INCLUDE_DIRECTORIES "${GWCA_FOLDER}/include")
target_link_libraries(gwca INTERFACE minhook::minhook)