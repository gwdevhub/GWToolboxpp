---
layout: default
---

<figure>
<img src="https://user-images.githubusercontent.com/11432831/28233524-4cf116ee-68ac-11e7-9a97-1a4bcc2d2659.PNG"/>
<figcaption>Dialogs Window</figcaption>
</figure>

# Dialogs

The Dialogs module in GWToolbox++ enhances your interaction with NPCs by providing advanced dialog handling capabilities. It consists of both a user interface window and underlying functionality that works behind the scenes.

## Dialogs Window

The Dialogs window allows you to send dialogs directly to NPCs without actually having to talk to them, and in some cases without having completed all the pre-requisites to make that dialog available.

You still have to interact with the NPC (press space) in order to activate the dialog, but you can do so even while they refuse to talk to you because they're in combat. You can move away from the NPC, and so long as you don't go beyond compass range or interact with another, sending the dialog will make you walk back.

### Default Dialogs
The 4 buttons at the top are for the quests that commonly need to be taken while the quest NPC is in aggro and unable to talk.

The 7 buttons below are for teleporting to each of the Reapers in the Underworld. Although you need to be at a Reaper to use the dialog, the destination Reaper does not need to have been spawned.

### Favorites
If you want more elite area dialogs to appear on the window, you can select them from the first drop down menu.

You can have as many favorites as you want: go on [Settings](settings) to add more.

### Custom Dialogs
The bottom drop down menu contains a list of some of the most popular dialogs, none of which require pre-requisites.

You can also use the input field, or the chat command `/dialog <id>`, to send any dialog you wish, so long as you know the code for it. This can be found using the [Info window](info).

## Advanced Features

The Dialogs module includes several advanced features that enhance your gameplay experience:

### Dialog Queueing
- The module can queue multiple dialogs to be sent in sequence
- Useful for complex interactions that require multiple dialog choices
- Automatically handles prerequisites for certain dialog types

### Special Dialog Handling
- **Quest Dialogs**: Automatically handles the sequence of dialogs needed to accept or complete quests
- **UW Teleports**: Automatically sends the prerequisite dialogs needed for Underworld Reaper teleports
- **Profession Changes**: Handles the sequence of dialogs needed to change professions

### Automatic Quest Acceptance
- Can automatically accept the first available quest from an NPC
- Particularly useful in areas like the Underworld where specific quest order is important
- Prioritizes certain quests (like UW Restore and UW Escort) when multiple are available

### Automatic Bounty Acceptance
- Can automatically accept bounties from faction NPCs
- Handles the specific dialog sequences needed for Luxon and Kurzick bounties
- Checks if you already have a faction blessing to avoid duplicates

## Chat Commands

- `/dialog <id>`: Send a specific dialog ID to the current NPC
- `/dialog 0`: Automatically accept the first available quest or bounty from the current NPC

## Technical Notes

The module tracks all available dialog options from NPCs and can send any valid dialog ID. It also handles embedded dialog options that appear within NPC text, making it possible to interact with dialog choices that appear in the middle of conversations.

[back](./)
