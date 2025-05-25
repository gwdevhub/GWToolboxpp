---
layout: default
---

# Chat Filter

The Chat Filter module in GWToolbox++ allows you to customize which messages appear in your chat window. You can filter out various types of messages to reduce clutter and focus on the information that matters to you.

## Features

### Drops
- Filter messages for rare and common item drops for you and your allies
- Filter messages for you or allies picking up rare and common items
- Filter salvaging messages
- Filter ashes dropped messages

Note: "Rare" items include anything with a yellow/gold name, Globs of Ectoplasm, Obsidian Shards, or Lockpicks. "Common" refers to all other items.

### Announcements
- Filter guild announcements
- Filter Hall of Heroes winner announcements
- Filter Favor of the Gods announcements
- Filter "You have been playing for..." messages
- Filter "[Player X] has achieved [title]..." messages
- Filter faction gain messages
- Filter challenge mission messages (e.g., "Hold-out bonus: +2 points")

### Warnings and Errors
- Filter "Invalid target" messages (including obstructed view, out of range, etc.)
- Filter "Inventory is full" messages
- Filter chest opening messages (locked, empty, in use)
- Filter "Item already identified" messages
- Filter "Not enough energy/adrenaline" messages
- Filter "Unable to use item" messages (including restrictions on usage)

### Targeting Messages
- Filter targeting messages from yourself ("I'm following...", "I'm attacking...", etc.)
- Filter targeting messages from others

### Other Messages
- Filter skill point gain messages
- Filter PvP update messages (e.g., "[Skill] was updated for PvP!")
- Filter Nine Rings game messages
- Filter Lunar fortune messages
- Filter "No one hears you..." messages
- Filter away status messages ("Player X might not reply because their status is set to away")

### Custom Filters
You can create custom filters to block specific content:
- Enter words or phrases to filter, one per line
- Messages containing an exact match (case-insensitive) will be filtered out
- Supports regular expressions for more advanced filtering

### Channel Filtering
- Option to filter messages based on chat channels (e.g., All, Guild, Team, Trade, Alliance, Emotes)
- Option to block messages from inactive chat channels

## Usage

To access the Chat Filter settings:
1. Open the GWToolbox++ Settings window
2. Navigate to the "Chat Filter" section

Here you can enable or disable various filter options and add custom filter rules.

## Advanced Filtering

For more complex filtering needs, you can use regular expressions. Enable the "Use regular expressions" option to interpret your custom filters as regex patterns.

Example regex filters:
- `^Player.*joined the game\.` - Filters out all "Player has joined the game" messages
- `\b(rare|unique|gold)\b` - Filters messages containing the words "rare", "unique", or "gold"

Remember to test your regex patterns to ensure they're working as intended.

[back](./)
