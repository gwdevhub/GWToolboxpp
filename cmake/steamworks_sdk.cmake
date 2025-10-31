include_guard()

set(STEAMWORKS_SDK_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/steamworks_sdk")

add_library(steamworks_sdk SHARED IMPORTED GLOBAL)
set_target_properties(steamworks_sdk PROPERTIES
    IMPORTED_IMPLIB "${STEAMWORKS_SDK_FOLDER}/redistributable_bin/steam_api.lib"
    IMPORTED_LOCATION "${STEAMWORKS_SDK_FOLDER}/redistributable_bin/steam_api.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${STEAMWORKS_SDK_FOLDER}/public/steam")
