---
title: "Quality of Life Fixes"
section: features
---

Alongside its bigger windows and widgets, GWToolbox++ quietly fixes a number of small base-game bugs and annoyances. These are enabled by default, and most players never realise that Toolbox is the reason things "just work". This page collects them in one place.

Each can be toggled in its own section of the [Settings](/docs/settings/) panel.

## Fixes

- **Hero command panel positions** — Remembers where you place each hero's skill-bar / command panel and restores it when the panel reappears, so they stop scattering across the screen after you reorder your party or swap characters. In the Party Settings section you can choose whether positions are remembered by **party slot** (the default — each slot keeps its placement no matter which hero is in it) or by **hero** (the placement follows the specific hero across slots and characters).

- **Experience bar shows XP progress** — Replaces the experience bar text so it shows your progress toward the next level (current XP / XP needed) instead of just repeating your current level number.

- **Keyboard Language Fix** — Stops Guild Wars from silently switching your Windows keyboard layout to en-US on startup, which would otherwise trigger the Win+Space language switcher and persist until you change it back in Windows. See [Input Modules](/docs/input_modules/).

- **Mouse Fix** — Fixes an occasional camera glitch when looking around with the mouse, and optionally lets you resize the in-game cursor. See [Input Modules](/docs/input_modules/).

- **Vendor Fix** — Fixes a base-game bug where collectable items sitting beyond the 56th inventory slot aren't recognised by collectors and crafters, so trade-ins work no matter where the item sits in your bags.

- **Code Optimiser** — Swaps Guild Wars' internal checksum (CRC32) routine for a much faster one, cutting the time the game spends validating large chunks of data and the stutter that can come with it.

- **FPS Fix** — Removes Guild Wars' hard-coded 90 FPS cap so the frame rate can match your monitor's refresh rate.
