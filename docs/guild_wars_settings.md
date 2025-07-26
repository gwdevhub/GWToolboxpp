---
layout: default
---

# Guild Wars Settings Module

The Guild Wars Settings Module allows you to save and load your Guild Wars in-game settings to external files, making it easy to transfer settings between accounts or create different setting profiles.

## Features

### Save and Load Settings
- Save all your Guild Wars settings to an INI file
- Load settings from a previously saved INI file
- Automatically uses your account email as the default filename

### Settings Saved
The module saves a comprehensive set of Guild Wars settings, including:
- All game preferences (graphics, sound, interface options)
- Window positions for all in-game windows
- Key mappings (keyboard shortcuts)
- Guild Wars window position and size
- Quest log entry visibility states

### Quest Log Management
- Maintains which quest categories are expanded or collapsed
- Shift-click on a quest category to expand or collapse all categories at once

## Usage

### Through the UI
1. Open the GWToolbox++ Settings window
2. Navigate to the "Guild Wars Settings" section
3. Click "Save to disk..." to save your current settings
4. Click "Load from disk..." to load settings from a file

### Chat Commands
- `/saveprefs [filename]` - Save your current settings to a file
  - If no filename is provided, uses your account email as the filename
  - Example: `/saveprefs mysettings` will save to `mysettings.ini`

- `/loadprefs [filename]` - Load settings from a file
  - If no filename is provided, uses your account email as the filename
  - Example: `/loadprefs mysettings` will load from `mysettings.ini`

## Benefits

### Account Management
- Easily transfer your preferred settings between multiple Guild Wars accounts
- Maintain consistent settings across all your characters

### Setting Profiles
- Create different setting profiles for different activities
  - PvE settings with specific window layouts
  - PvP settings with different key bindings
  - Speedclear settings optimized for specific roles

### Backup and Recovery
- Back up your settings before making significant changes
- Quickly restore your preferred settings if something goes wrong
- Share your settings with friends or guildmates

## Notes

- Settings files are saved in the GWToolbox++ directory
- Files use the INI format and can be manually edited if needed
- Loading settings will immediately apply them to your game
- Some settings may require a game restart to take full effect

[back](./)
