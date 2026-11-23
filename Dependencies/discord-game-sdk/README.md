# Discord GameSDK C header

`discord_game_sdk.h` from the Discord GameSDK 2.5.6, the version of `resources/discord_game_sdk.dll`. It is the only
part of the SDK the toolbox uses: `Modules/DiscordModule.cpp` loads the dll and resolves `DiscordCreate` at runtime, so
nothing links against the SDK's import library or its C++ wrapper.

Your use of the GameSDK is bound by the Discord Developer Terms of Service, https://discord.com/developers/docs/legal.
