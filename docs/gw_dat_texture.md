---
layout: default
---

# GW Dat Texture Module

The GW Dat Texture Module is a utility component in GWToolbox++ that provides access to textures stored in Guild Wars' .dat files. This module allows other components to load and display textures, icons, and images directly from the game's data files.

## Features

### Texture Loading
- Load textures from Guild Wars' .dat files using file IDs
- Convert between file hashes and file IDs
- Cache loaded textures to improve performance
- Support for various texture formats used by Guild Wars

### Technical Capabilities
- Hooks into Guild Wars' internal texture handling functions
- Converts Guild Wars' texture formats to DirectX-compatible formats
- Manages memory for loaded textures
- Provides thread-safe access to textures

## Usage

This module is primarily used by other components of GWToolbox++ rather than directly by users. It provides the following key functions:

- `LoadTextureFromFileId(uint32_t file_id)`: Loads a texture from the Guild Wars .dat file using a file ID
- `FileHashToFileId(const wchar_t* fileHash)`: Converts a file hash to a file ID

## Components That Use This Module

Several GWToolbox++ components rely on this module to display textures and icons:

- **Materials Window**: Displays icons for various materials
- **World Map Widget**: Loads map textures and icons
- **Info Window**: Shows textures for items and skills
- **Active Quest Widget**: Displays quest marker icons
- **Armory Window**: Shows equipment and item textures
- **Completion Window**: Loads achievement and completion icons

## Technical Details

The module works by:

1. Hooking into Guild Wars' internal functions for handling .dat file resources
2. Loading the raw texture data from the .dat file
3. Decoding the texture data from Guild Wars' formats
4. Converting the texture to a DirectX-compatible format
5. Caching the texture for future use

This process allows GWToolbox++ to display authentic Guild Wars textures in its interface without needing to include them as separate files.

## Benefits

- **Authentic Appearance**: Uses the actual textures from the game
- **Reduced File Size**: No need to include texture files with GWToolbox++
- **Consistency**: Ensures UI elements match the game's visual style
- **Efficiency**: Caching system prevents redundant loading of textures

This module operates silently in the background and requires no user configuration.

[back](./)
