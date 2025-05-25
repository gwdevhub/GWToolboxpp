---
layout: default
---

# Timer

The Timer widget in GWToolbox++ provides precise timing functionality for Guild Wars, allowing you to track how long you've spent in instances, missions, and special areas.

## Features

### Main Timer
- **Instance Timer**: Tracks time since the current instance was created (resets when zoning)
- **Real-time Timer**: Tracks time continuously across zones (doesn't reset when zoning)
- **Customizable Display**: Control the number of decimal places shown (0-3)
- **Objective Completion**: Option to stop the timer when mission objectives are completed

### Extra Timers
The Timer widget can display specialized timers for specific areas:
- **Deep Aspects**: Shows time remaining for aspects in The Deep
- **DoA Cave**: Shows time remaining for cave collapse in Domain of Anguish
- **Dhuum**: Shows time remaining for Dhuum fight
- **Urgoz Doors**: Shows time remaining for doors in Urgoz's Warren
- **Dungeon Traps**: Shows time remaining for various dungeon traps

### Spirit Timers
- Track the duration of spirit effects such as:
  - Edge of Extinction (EoE)
  - Quickening Zephyr (QZ)
  - Frozen Soil
  - Union, Shelter, Displacement
  - And many more

## Settings

### Display Options
- **Text Size**: Adjust the size of the main timer and extra timers text
- **Hide in Outpost**: Hide the timer when in outposts
- **Show Decimals**: Choose how many decimal places to display (0-3)

### Timer Behavior
- **Instance/Real-time**: Choose between instance timer or real-time timer
- **Never Reset**: Prevent the timer from resetting when entering new areas
- **Stop at Objective Completion**: Automatically stop the timer when objectives are completed
- **Also Show Instance Timer**: Display both real-time and instance timers simultaneously

### Print Options
Print the current time to chat:
- **With Ctrl+Click**: Print time when Ctrl+clicking on the timer
- **At Objective Completion**: Print time when objectives are completed
- **When Leaving Explorables**: Print time when leaving explorable areas

## Usage

The Timer widget appears as a small window showing the current time in hours:minutes:seconds format. Depending on your settings, it may also show additional timers for spirits and special areas.

### Chat Commands
- `/age2`: Prints the current timer value to chat

### Resetting the Timer
- The timer resets automatically based on your settings
- Use `/resettimer` to force a reset at the next loading screen

This widget is particularly useful for speedruns, tracking mission completion times, and monitoring the duration of spirit effects.

[back](./)
