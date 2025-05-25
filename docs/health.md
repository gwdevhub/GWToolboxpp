---
layout: default
---

# Health Widget

The Health Widget in GWToolbox++ displays detailed health information about your current target, with customizable thresholds and visual indicators.

## Features

### Health Display
- Shows the target's health as both a percentage and absolute value
- Customizable text sizes for different parts of the display
- Option to print target health to chat with Ctrl+Click

### Customizable Thresholds
- Create custom health thresholds with unique names and colors
- Thresholds can be set to activate at specific health percentages
- Optional filters to only show thresholds for specific:
  - Model IDs (specific enemy types)
  - Skill IDs (when certain skills are relevant)
  - Map IDs (in specific areas)

### Visual Indicators
- Color-coded display based on the target's current health percentage
- Easily identify when targets reach important health thresholds
- Helpful for timing skills that are more effective at certain health levels

## Settings

### Display Options
- **Hide in outpost**: Hides the widget when in outposts
- **Ctrl+Click to print target health**: Enables printing the target's health to chat when Ctrl+clicking the widget
- **Text sizes**: Customize the size of different text elements:
  - 'Health' header
  - Percent value
  - Absolute value

### Threshold Management
- Add, edit, and delete custom thresholds
- For each threshold, you can set:
  - Name: A descriptive label for the threshold
  - Color: The color to display when the target's health is below this threshold
  - Value: The health percentage at which this threshold activates
  - Optional filters: Model ID, Skill ID, and Map ID

## Usage

The Health Widget displays the health of your currently targeted agent. When you target an enemy, ally, or object with health, the widget will show:

1. The word "Health" as a header
2. The health percentage (e.g., "75%")
3. The absolute health value (e.g., "750/1000")

The display will change color based on the thresholds you've set. For example, you might set:
- A green threshold at 90% for "Edge of Extinction" (which triggers at 90% health)
- A yellow threshold at 50% for "Finish Him!" (which is more effective below 50% health)

This widget is particularly useful for:
- Timing skills that are more effective at certain health thresholds
- Monitoring boss health during difficult encounters
- Coordinating spike damage with your team

[back](./)
