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
The damage monitor records how much damage has been dealt by each party member and displays it as a number, as a percentage of the party's total, and as a thick horizontal bar.

Life steal is counted as damage, but degen is not; if it doesn't show as a pop-up number above the enemy, it isn't counted by the monitor.

As well as the thick bar showing each party member's damage, there is a thinner bar showing recent damage. This resets after dealing no damage for a short time, which can be customized in [Settings](settings).

You can customize the color of the bars in [Settings](settings). You can also adjust the row height to make the monitor line up better with your party window.

Bear in mind that if a party member or enemy is beyond compass range, your client cannot see the damage packets they are dealing/receiving, so these will not show up on the damage monitor.

## Bonds
The bonds monitor shows which maintained monk enchantments you currently have on the allies in the party window. There is an option in [Settings](settings) to only show bonds for party members.

The bonds are displayed in the order they appear in your skill bar when entering the zone. There is a button in [Settings](settings) to refresh the order if you re-order your skill bar in an explorable area. You can also make them appear in reverse order.

Only enchantments that can be maintained on another ally will show up.

You can click on the monitor to cast or dismiss enchantments. This can be disabled in [Settings](settings).

You can customize the background color of the monitor in [Settings](settings). You can also adjust the row height to make it line up better with your party window.

## Clock
The clock simply shows your local time of day. You can choose 12 or 24-hour clock.

## Vanquish
The vanquish monitor shows how many enemies are in a zone, and how many have already been killed. This updates whenever you spawn additional enemies e.g. from quests or pop-ups.

This widget will not appear unless you are in an explorable area and in Hard Mode. In areas that cannot be vanquished, such as missions and dungeons, the counter will show 0/0.

[back](./)
