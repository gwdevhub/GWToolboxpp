include_guard()

add_library(discord_game_sdk INTERFACE)
target_include_directories(discord_game_sdk INTERFACE "${PROJECT_SOURCE_DIR}/dependencies/discord")
