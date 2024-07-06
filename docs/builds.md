---
layout: default
---

<figure>
<img src="https://i.imgur.com/Z6kP0V1.png"/>
<figcaption>Edit Teambuild Window</figcaption>
</figure>

# Builds
The Builds window allows you to create and edit teambuilds, which can then be sent directly to party chat or loaded onto your character. Clicking "Send" on one of your teambuilds will send the name, followed by each build in a loadable template to the party chat. Clicking "Load" will load the build onto your character.

To create a new teambuild, click "Add Teambuild", or to edit an existing teambuild, click on its name. This will open a new window, which can be independently moved and re-sized. You can have multiple windows open at once. In these windows, you can write the name of the team and the individual builds, and paste in the template codes.

## Features

- **Pcons**: The "Pcons" button allows you to toggle specific [Pcons](pcons) on or off when you load the build. When the Pcons button is greyed out, it won't change your pcons when you load the build. You can also send the list of Pcons for a build to chat.

- **View/Send**: The "View" button allows you to view the build in-game. Ctrl + Click to Send the build to chat.

- **Load**: The "Load" button loads the build onto your character and activates the associated Pcons.

- **Show numbers**: Adds a number to the name of each build when sending to chat.

- **Blank template**: If you leave a template code blank, it will simply appear as a message, instead of a blank template, and will not be given a number. This is useful for sending general instructions to the whole team.

- **Teambuild management**: "Up" and "Down" change the teambuild's position in the list of teambuilds, and "Delete" will delete the entire teambuild from the list.

- **Preferred Skill Orders**: You can set preferred skill orders for builds. When a matching set of skills is loaded, it will be automatically reordered to match your preference.

## Settings

- **Hide when entering explorable**: Automatically hides the Builds window when entering an explorable area.
- **Only show one teambuild at a time**: Closes other teambuild windows when you open a new one.
- **Auto load pcons**: Automatically loads pcons for a build when loaded onto a character.
- **Send pcons when pinging a build**: Automatically sends a second message after the build template in team chat, showing the pcons that the build uses.
- **Order teambuilds by**: Choose to order teambuilds by index or name.

## Chat Commands
`/load [build template|build name] [Hero index]` loads a build from your saved templates in Guild Wars. The build name must be between quotes if it contains spaces. First Hero in the party is 1, and so on up to 7. Leave out for player.

`/loadbuild [teambuild] <build name|build code>` loads a build via GWToolbox Builds window. Does a partial search on team build name/build name/build code. Matches current player's profession.
* Example: `/loadbuild fbgm tk` would load the build called `Me/A | Tendril Killer (TK)` from the teambuild called `DoA | FBGM Tactics` onto your player if you're a Mesmer

[back](./)
