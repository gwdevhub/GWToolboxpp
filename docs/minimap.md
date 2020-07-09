---
layout: default
---

<figure>
<img src="https://user-images.githubusercontent.com/11432831/28382699-ed34bf1e-6c73-11e7-8280-bdfa85d90ae0.PNG"/>
<figcaption>Minimap</figcaption>
</figure>

# Minimap
The Minimap widget is an advanced version of the Guild Wars compass.

## Features Overview
* `Ctrl+click` on the map to target the nearest targetable agent within 600 distance units (gwinches). This includes all allies and enemies, as well as locked chests (but not other chests).
* The minimap shows a larger area of the map than the default Guild Wars compass. While the minimap shows everything within party range (5000 gwinches), which is the same distance at which agents disappear from sight, the default compass has a radius of less than 90% of this. While agents further than 90% can still be targeted normally and with the minimap, they cannot be targeted by tabbing through targets.
* Unlike the default compass, the minimap turns immediately when you look around.
* Rather than the mission map background, the minimap has an accurate pathing map that shows where agents can move.
* You can make custom lines and circles on the map to replace points of reference from the textures of the default compass.
* Party range, spirit range, cast range, and aggro range each have an accurate circle on the minimap. If you have Heart of Shadow or Viper's Defense in your build, another circle will appear, showing that range, with a line pointing away from your target to show where you would land.
* All 7 heroes can be individually flagged on the minimap, although flagging on the ground in the open world is not currently supported.

### Agents
* Edge of Extinction and Quickening Zephyr areas of effect are visible on the minimap.
* Agents that can move or turn around have the shape of a teardrop, with the point of the teardrop indicating the direction they are facing.
* Signposts and items on the ground appear as squares.
* Other agents that cannot move or turn, such as passive spirits, appear as circles.
* Bosses and other important targets can be set to appear as a different size to other agents, to make them distinguishable on the minimap.  
  The following non-boss foes appear as bosses on the minimap:
  * Stygian Underlord (dervish and ranger)
  * Stygian Lord (monk, elementalist, mesmer, and necromancer)
  * The Black Beast of Arrgh
  * Smothering Tendrils
  * Lord Jadoth
  * Keeper of Souls
  * The Four Horsemen (Madruk, Ghozer, Thul Za, and Khazad Dhuum)
  * Slayer (Demon Assassin)
  * Shard Wolf
  * Cursed Brigand
  * Fendi Nin
  * Soul of Fendi Nin

<figure>
<img src="https://user-images.githubusercontent.com/11432831/28233561-79208cae-68ac-11e7-8e7e-af4bb1b1264e.PNG"/>
<figcaption>Minimap Settings</figcaption>
</figure>

## Customizing appearance
All of these settings can be adjusted in [Settings](settings) or in the `GWToolbox.ini` file in the settings folder.

**Scale** controls the zoom level. This can also be adjusted with `Shift+mousewheel`.</br>
`Shift+drag` to view other areas of the map. In-game movement will snap the minimap back to centering on you.</br>
Moving the map view like this does not allow you to see agents that are beyond party range.

You can change the color and opacity of all elements of the minimap. Set the alpha to 0 to disable any minimap feature.

### Agents
* **EoE** and **QZ** show the areas of the map affected by Edge of Extinction and Quickening Zephyr, respectively. The colors at the middle of these circle are the same, with alpha -50, and a gradient in between.</br></br>
* **Target** is the border surrounding the currently targeted agent.
* **Player (alive)** is you while you're alive.
* **Player (dead)** is you while you're dead.
* **Signpost** is an interactive object that provides some information to players.
* **Item** is an item dropped on the ground.
* **Hostile (>90%)** is an enemy with more than 90% health.
* **Hostile (<90%)** is an enemy with less than 90% health.
* **Hostile (dead)** is the corpse of an enemy.
* **Neutral** is a charmable animal that has not yet turned hostile. This does not include charmed pets, which are counted as allied NPCs.
* **Ally (party member)** is any non-hostile player in the instance.
* **Ally (NPC)** is any non-hostile NPC.
* **Ally (spirit)** is any non-hostile spirit.
* **Ally (minion)** is any non-hostile minion.
* **Ally (dead)** is the corpse of any ally.</br></br>
* **Agent modifier** is an effect applied to all agents to create a shaded appearance. Each agent has this value removed on the border and added at the center. Zero makes agents have solid color, while a high number makes them appear more shaded.

The appearance of all agents can be re-sized:
* **Default Size** controls the size of all agents except the player, signposts, items, bosses, and minions.
* **Player Size** controls the size of the player.
* **Signpost Size** controls the size of signposts.
* **Item Size** controls the size of items on the ground.
* **Boss Size** controls the size of bosses.
* **Minion Size** controls the size of minions.

### Ranges
These are the colors of the range circles.

### Pings and drawings
* **Drawings** are what you and other party members draw on the minimap.
* **Pings** are when you and other party members click on the minimap.</br></br>
* **Shadowstep Marker** appears when you use Shadow of Haste, Shadow Walk, Aura of Displacement, or Shadow Meld. It marks the original location, so you can see where you'll shadowstep back to when your stance/enchantment ends.
* **Shadowstep Line** is a straight line between you and the Shadowstep Marker, so you can see if terrain is obstructing your path.</br></br>
* When you approach max Recall range (5000 gwinches), the Shadowstep Line gradually changes color to provide a warning.
  * **Shadowstep Line (Max range)** shows the final color of the line at max range.
  * **Max range start** is the percentage of Recall range at which the line starts the change color.
  * **Max range end** is the percentage of Recall range at which the line reaches the max range color.

### Symbols
* **Quest Marker** is a star showing the goal of the active quest. If the goal is not in compass range, the star becomes an arrow on the edge of the compass, pointing towards the goal.
* **North Marker** is an arrow on the northernmost point of the compass.</br></br>
* **Symbol modifier** is an effect applied to all symbols to create a shaded appearance. Each symbol has this value removed on the border and added at the center. Zero makes symbols have solid color, while a high number makes them appear more shaded.

### Terrain
* **Map** is the area agents can move within.
* **Shadow** is the same as the map area, but moved down several pixels, to create an illusion of thickness, which makes the map appear more solid.

### Custom Markers
Custom markers are lines between 2 points, or circles radiating from one point, drawn on the minimap to to replace points of reference from the textures of the default compass.

To add a new marker, choose the `Add Line` button to add a line, or `Add Marker` to add a circle. The new marker will then appear in the list of markers, and you can edit it.

Use the [Info](info) window to find the co-ordinates and Map ID of where you want the marker to appear.</br>
Lines require a co-ordinate for each end.</br>
Circles require a co-ordinate for the center and a value for the radius. You can choose from a drop-down if you want the circle to be hollow or filled.</br>
The next field is for the Map ID. If you choose 0, the marker will appear on all maps.</br>
The last field is to enter a name for the marker, so you can easily remember what it is.</br>
The toggle to the left turns the marker off.</br>
The X to the right deletes the marker, without a prompt.

Note: custom markers are stored in `Markers.ini` in the settings folder. You can share this file with other players or paste other people's markers into it.

### Hero flagging
Hero flags become visible when there are heroes in your party and you are in an explorable area.

You can turn them on/off with the **Show hero flag controls** toggle.

You can attach/detach them to/from the minimap with the **Attach to minimap** toggle. If detached, you can move/resize the window with `Unlock Move All` (at the top of the Settings window).

You can also change the color of the flag controls' background.

## Chat Commands

You can use the chat commands to flag heroes using the minimap.

`/flag [all|clear|<number>]` to flag a hero in the minimap (same as using the buttons by the minimap).

`/flag <number> clear` to clear flag for a hero.

`/flag [all|<number>] [x] [y]` to flag a hero to coordinates `[x]`,`[y]`.



[back](./)
