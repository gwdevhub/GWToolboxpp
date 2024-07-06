---
layout: default
---

# Chat Filter

The Chat Filter module in GWToolbox++ allows you to customize which messages appear in your chat window. You can filter out various types of messages to reduce clutter and focus on the information that matters to you.

## Features

### Drops
- Filter messages for rare and common item drops for you and your allies
- Filter messages for allies picking up rare and common items

Note: "Rare" items include anything with a yellow/gold name, Globs of Ectoplasm, or Obsidian Shards. "Common" refers to all other items.

### Announcements
- Filter Hall of Heroes winner announcements
- Filter Favor of the Gods announcements
- Filter "You have been playing for..." messages
- Filter "[Player X] has achieved [title]..." messages

### Other Messages
- Filter skill point gain messages
- Filter PvP update messages (e.g., "[Skill] was updated for PvP!")
- Filter Nine Rings game messages
- Filter Lunar fortune messages
- Filter "No one hears you..." messages
- Filter away status messages

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
