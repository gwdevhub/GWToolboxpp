---
layout: default
---

# Quest Functionality

GWToolbox++ includes enhanced functionality for quests, providing users with improved navigation and information display.

## Features

### Active Quest Widget

The Active Quest Widget displays information about the currently active quest, including:

- Quest name
- Quest objectives
- Completion status of each objective

This widget provides a clear and concise view of your current quest progress, helping you stay focused on your goals.

### Quest Pathing

GWToolbox++ includes an advanced quest pathing feature that calculates the optimal route to your quest objective. This feature:

- Automatically calculates a path to the quest marker
- Updates the path in real-time as you move
- Displays the path on the minimap
- Redirects the in-game quest marker to guide you along the calculated path

## How It Works

1. When a quest becomes active, GWToolbox++ calculates a path from your current position to the quest marker.
2. The calculated path is displayed on the minimap as a series of connected lines.
3. The in-game quest marker is updated to point to the next waypoint in the calculated path.
4. As you move, the path is continuously updated:
   - If you move significantly away from the calculated path, it will recalculate.
   - As you reach waypoints along the path, the quest marker will update to the next waypoint.

## Benefits

- Easier navigation, especially in complex areas
- Reduced time spent figuring out how to reach quest objectives
- Improved efficiency in completing quests

## Notes

- The quest pathing feature works best in areas where GWToolbox++ has accurate map data.
- In some cases, especially in very complex areas or where map data is incomplete, the calculated path may not be optimal. Always use your judgment and knowledge of the game world.
- This feature is designed to assist players, not to replace exploration or learning of the game world. We encourage players to use it as a tool to enhance their gameplay experience, not as a crutch.

Remember, while this feature can greatly assist in navigation, the joy of Guild Wars often comes from exploration and discovery. Use this tool to enhance your experience, not to diminish it!
