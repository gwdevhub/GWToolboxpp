---
layout: default
---

# Chat Log

The Chat Log module in GWToolbox++ enhances the in-game chat functionality by providing persistent chat history and additional features.

## Features

### Persistent Chat History
- Maintains chat history across map changes and game sessions
- Automatically saves chat logs to disk
- Loads previous chat history when starting a new session

### Extended Chat Log
- Stores more messages than the default Guild Wars chat log
- Includes both received and sent messages

### Multi-Character Support
- Maintains separate chat logs for each Guild Wars account

### Automatic Injection
- Automatically injects saved chat history into the game's chat log when changing maps or characters

## Usage

The Chat Log module works automatically in the background. Once enabled, it will:
- Save your chat history to disk periodically and when you exit the game
- Load your previous chat history when you start the game or change characters
- Inject the saved chat history into your game client's chat log

## Settings

To access Chat Log settings:
1. Open the GWToolbox++ Settings window
2. Navigate to the "Chat Log" section

Available options:
- Enable/disable the Chat Log module
- Configure the maximum number of messages to store

## File Location

Chat logs are stored in the GWToolbox++ settings folder:
```
C:\Users\[Username]\Documents\GWToolboxpp\[ComputerName]\
```

Files are named based on the account email address:
- `recv_[account_email].ini` for received messages
- `sent_[account_email].ini` for sent messages

## Notes

- The Chat Log module respects your in-game chat filter settings
- Chat history is stored securely on your local machine and is not shared or uploaded anywhere
