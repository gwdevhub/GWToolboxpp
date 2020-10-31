---
layout: default
---

# Widgets
Unlike [Windows](windows), widgets are designed to stay open all the time, as part of your user interface. Mostly widgets are non-interactive, and just display information.

All widgets have options in [Settings](settings) to specify the exact co-ordinates of their on-screen position and prevent the widget from being moved around or re-sized.

## Interface
* Click and drag on the lower right corner to re-size a widget.
* Click and drag anywhere to move a widget.

<figure>
<img src="https://user-images.githubusercontent.com/11432831/28233476-fa86197c-68ab-11e7-85ac-f9c21f4e30db.PNG"/>
<figcaption>Timer Widget</figcaption>
</figure>
  
There are several ways to open and close widgets:
* Checkboxes in the [Info](info) window.
* Checkboxes in the right of each header on [Settings](settings).
* Chat command `/hide (name of widget)`.
* Chat command `/show (name of widget)`.
* Chat command `/tb (name of widdget)` to toggle show/hide.

<figure>
<img src="https://user-images.githubusercontent.com/11432831/28233454-decaf91e-68ab-11e7-8547-e584bd499628.PNG"/>
<figcaption>Health Widget</figcaption>
</figure>

## Timer
The timer shows the *instance time* of the current map, meaning for how long the instance/map/district has been running.

## Health
The health monitor shows your target's current health, maximum health, and health as a percentage.

Until you directly affect the health of an agent (see a pop-up number over it), you will only be able to see the percentage. If its maximum health changes (e.g. a player using a cupcake or getting a morale boost), the health monitor won't show this until you affect its health again.

<figure>
<img src="https://user-images.githubusercontent.com/11432831/28233453-dcab43f0-68ab-11e7-9788-c5e39799d0ee.PNG"/>
<figcaption>Distance Widget</figcaption>
</figure>

## Distance
The distance monitor shows your distance from your target as a percentage of compass range and in GW distance units (gwinches).

## Minimap
The [minimap](minimap) is an improved version of the default Guild Wars compass, with such features as targeting, zoom, and accurate pathing maps.

## Damage
The [damage monitor](damage_monitor) records how much damage has been dealt by each party member and displays it as a number, as a percentage of the party's total, and as a thick horizontal bar.

## Bonds
The bonds monitor shows which maintained monk enchantments you currently have on the allies in the party window. There is an option in [Settings](settings) to only show bonds for party members. The monitor also shows refrains ([Heroic Refrain](https://wiki.guildwars.com/wiki/Heroic_Refrain), [Bladeturn Refrain](https://wiki.guildwars.com/wiki/Bladeturn_Refrain), [Burning Refrain](https://wiki.guildwars.com/wiki/Burning_Refrain), [Hasty Refrain](https://wiki.guildwars.com/wiki/Hasty_Refrain), [Mending Refrain](https://wiki.guildwars.com/wiki/Mending_Refrain)) you have cast on yourself and your heroes.

The bonds are displayed in the order they appear in your skill bar when entering the zone. There is a button in [Settings](settings) to refresh the order if you re-order your skill bar in an explorable area. You can also make them appear in reverse order.

Self-targeted enchantments that cannot be maintained on another ally (e.g. Unyielding Aura) will not show up.

If your attributes have increased, allowing you to cast a stronger version of a bond or refrain, it will appear darkened on the monitor. This only works for yourself and your heroes.

You can click on the monitor to cast or dismiss bonds and cast refrains. This can be disabled in [Settings](settings).

You can customize the background color of the monitor in [Settings](settings). You can also adjust the row height to make it line up better with your party window.

## Clock
The clock simply shows your local time of day. You can choose 12 or 24-hour clock.

## Vanquish
The vanquish monitor shows how many enemies are in a zone, and how many have already been killed. This updates whenever you spawn additional enemies e.g. from quests or pop-ups.

This widget will not appear unless you are in an vanquishable area and in Hard Mode. 

## Alcohol
When in an explorable area, the alcohol monitor displays a timer of how long before the drunkenness effect wears off.

[back](./)
