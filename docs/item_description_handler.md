---
layout: default
---

# Item Description Handler

The Item Description Handler module in GWToolbox++ is a utility component that enhances the way item descriptions are displayed in Guild Wars. This module works behind the scenes to allow other GWToolbox++ features to modify and add information to item tooltips.

## Features

### Enhanced Item Descriptions

The module provides a framework for other GWToolbox++ components to add useful information to item tooltips, such as:

- **Price information**: Shows the estimated value of items based on trading websites
- **Salvage information**: Displays what materials can be obtained from salvaging an item
- **Item properties**: Highlights special properties or uses of items
- **Custom notes**: Allows for additional contextual information about items

### Item Name Processing

The module includes utilities for processing item names:

- Extracts item names without prefix and suffix modifiers
- Identifies which parts of an item name are inherent to the item versus which are from upgrades
- Provides consistent item name formatting for other modules

## Technical Details

The Item Description Handler works by:

1. Hooking into Guild Wars' item description system
2. Intercepting requests for item descriptions
3. Allowing other modules to register callbacks that can modify the descriptions
4. Maintaining a priority system ("altitude") to control the order in which modifications are applied

## Integration with Other Modules

Several GWToolbox++ modules use the Item Description Handler to enhance item tooltips:

- **Price Checker Module**: Adds price information from trading websites
- **Salvage Info Module**: Shows potential materials from salvaging
- **Game Settings**: Controls when and how item descriptions are displayed
- **Inventory Manager**: Uses item name processing for inventory management features

## Benefits

While this module doesn't have a direct user interface, it provides significant benefits:

- **More informative tooltips**: Get additional information about items without having to look it up elsewhere
- **Time-saving**: Quickly see item values and properties at a glance
- **Consistency**: Ensures that item information is displayed in a consistent format
- **Extensibility**: Allows for new types of item information to be added by other modules

## For Developers

If you're developing a GWToolbox++ module that needs to modify item descriptions:

1. Create a callback function that follows the `GetItemDescriptionCallback` signature
2. Register your callback using `ItemDescriptionHandler::RegisterDescriptionCallback()`
3. Specify an altitude value to control when your modifications are applied
4. Unregister your callback when your module is terminated

This module is primarily a utility for other GWToolbox++ components and operates silently in the background to enhance the Guild Wars user experience.

[back](./)
