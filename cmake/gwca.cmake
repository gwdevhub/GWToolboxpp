include_guard()

set(GWCA_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/GWCA")

find_package(minhook CONFIG REQUIRED)

# Default to gwca.dll and gwca.lib for all configurations
set(GWCA_DLL_DEBUG "${GWCA_FOLDER}/bin/gwca.dll")
set(GWCA_LIB_DEBUG "${GWCA_FOLDER}/lib/gwca.lib")
set(GWCA_DLL_RELEASE "${GWCA_FOLDER}/bin/gwca.dll")
set(GWCA_LIB_RELEASE "${GWCA_FOLDER}/lib/gwca.lib")
set(GWCA_DLL_DEBUG_NAME "gwca.dll")
set(GWCA_DLL_RELEASE_NAME "gwca.dll")

# Override for Debug configuration if gwcad.dll exists
if (EXISTS "${GWCA_FOLDER}/bin/gwcad.dll")
    set(GWCA_DLL_DEBUG "${GWCA_FOLDER}/bin/gwcad.dll")
    set(GWCA_LIB_DEBUG "${GWCA_FOLDER}/lib/gwcad.lib")
	set(GWCA_DLL_DEBUG_NAME "gwcad.dll")
endif()

add_library(gwca SHARED IMPORTED GLOBAL)
set_target_properties(gwca PROPERTIES
    IMPORTED_IMPLIB_DEBUG "${GWCA_LIB_DEBUG}"
    IMPORTED_IMPLIB_RELEASE "${GWCA_LIB_RELEASE}"
    IMPORTED_LOCATION_DEBUG "${GWCA_DLL_DEBUG}"
    IMPORTED_LOCATION_RELEASE "${GWCA_DLL_RELEASE}"
    INTERFACE_INCLUDE_DIRECTORIES "${GWCA_FOLDER}/include")
target_link_libraries(gwca INTERFACE minhook::minhook)
