include_guard()

set(GWCA_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/GWCA")

find_package(minhook CONFIG REQUIRED)

add_library(gwca SHARED IMPORTED GLOBAL)
set_target_properties(gwca PROPERTIES
	IMPORTED_IMPLIB_DEBUG "${GWCA_FOLDER}/lib/gwca.lib"
    IMPORTED_IMPLIB_RELEASE "${GWCA_FOLDER}/lib/gwca.lib"
    IMPORTED_LOCATION_DEBUG "${GWCA_FOLDER}/bin/gwca.dll"
    IMPORTED_LOCATION_RELEASE "${GWCA_FOLDER}/bin/gwca.dll"
	INTERFACE_INCLUDE_DIRECTORIES "${GWCA_FOLDER}/include")
target_link_libraries(gwca INTERFACE minhook::minhook)