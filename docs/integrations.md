---
layout: default
---

# Integrations

GWToolbox++ offers several integrations with external services to enhance your Guild Wars experience.

## Discord Integration

The Discord integration allows GWToolbox++ to send in-game information to Discord, providing rich presence functionality.

Features:
- Display your current in-game location
- Show your character information
- Share party information
- Allow other players to join your party through Discord

To enable and configure Discord integration:
1. Go to GWToolbox++ settings
2. Check "Enable Discord integration"
3. Customize the displayed information as desired

## Teamspeak 5 Integration

The Teamspeak 5 integration connects GWToolbox++ with your Teamspeak 5 client, allowing for seamless communication between the game and your voice chat.

Features:
- View current Teamspeak server information in-game
- Send your current server info to the in-game chat
- Automatically connect to the appropriate Teamspeak channel based on your in-game location

To set up Teamspeak 5 integration:
1. Ensure Teamspeak 5 is running and the "Remote Apps" feature is enabled
2. In GWToolbox++ settings, enable "Teamspeak 5 integration"
3. Use the "/ts5" command in-game to send your current server info

## Twitch Integration

The Twitch integration allows streamers to interact with their chat directly from within Guild Wars.

Features:
- Send and receive Twitch chat messages in-game
- Receive notifications when viewers join or leave your channel
- Customize the appearance of Twitch messages in your game chat

To configure Twitch integration:
1. In GWToolbox++ settings, enable "Twitch integration"
2. Enter your Twitch username and OAuth token
3. Customize notification settings as desired

These integrations help bridge the gap between Guild Wars and popular external services, enhancing communication and social features for players and streamers alike.

## LiveSplit Integration

The LiveSplit integration allows livesplit to communicate with toolbox and get updates when objectives are completed. This essentially works as an autosplitter integrated into GWToolbox++.

Features:
- Automatically start, split, and reset your LiveSplit runs
- Integrates directly into LiveSplit One
- Integrates with the LiveSplit Webserver using a third-party connector

To configure LiveSplit integration:
1. In GWToolbox++ settings, enable "Livsplit Websocket server" in the Objectives settings
2. Configure a port and press "Restart" if the server is not already running
3. Choose the corresponding format for the LiveSplit tool of choice
4. To configure your splits in LiveSplit One
    1. Set up you splits to match the objectives for your chosen run
    2. In LiveSplit One, go to "Settings" and connect to the server under "Network"
    3. The URL will be "ws://localhost:9001" or whatever port you configured in GWToolbox++ previously
5. To configure your Splits in LiveSplit using the LiveSplit Server
    1. Go to your LiveSplit Settings and configure the LiveSplit Server Port to something other than the GWToolbox++ server port and set the Startup Behaviour to "Start Websocket Server"
    2. Go to Control->"Start Websocket Server" if it is not already running
    3. Use something like [websocat](https://github.com/vi/websocat) to connect the two servers
        1. Example in linux: `websocat ws://localhost:9001 ws://localhost:16834/livesplit`
        2. Example in windows: `websocat.exe ws://localhost:9001 ws://localhost:16834/livesplit`


These integrations help bridge the gap between Guild Wars and popular external services, enhancing communication and social features for players and streamers alike.

[back](./)
