---
layout: default
---

# Crash Handler

The Crash Handler module in GWToolbox++ provides enhanced crash handling capabilities for Guild Wars, helping to diagnose and troubleshoot issues when the game crashes.

## Features

### Crash Dump Creation
- Automatically creates detailed crash dumps when Guild Wars or GWToolbox++ crashes
- Stores crash dumps in the `crashes` folder within your GWToolbox++ directory
- Includes comprehensive information about the state of the game at the time of the crash

### User Notification
- Displays a user-friendly message when a crash occurs
- Provides information about the crash and the location of the crash dump file
- Helps users understand what happened and what steps to take next

### Assertion Handling
- Captures and reports assertion failures in the code
- Provides detailed information about the assertion that failed, including the file and line number
- Helps developers pinpoint the exact location of issues

## Technical Details

The Crash Handler works by hooking into Guild Wars' built-in crash handling system and extending it with additional functionality. When a crash occurs:

1. The handler intercepts the crash
2. It gathers information about the crash, including:
   - The exception that occurred
   - The state of the program at the time of the crash
   - Any assertion messages or other relevant information
3. It creates a minidump file with this information
4. It displays a message to the user informing them of the crash and the location of the dump file

## For Users

If you experience a crash while using GWToolbox++:

1. A message will appear informing you that a crash dump has been created
2. The crash dump will be stored in the `crashes` folder in your GWToolbox++ directory
3. If you're reporting the issue to the developers, include this crash dump file as it contains valuable diagnostic information

## For Developers

The crash dumps created by this module contain detailed information that can help diagnose issues, including:

- The call stack at the time of the crash
- The values of variables in memory
- Information about the threads that were running
- Any assertion messages or other diagnostic information

This information is invaluable for identifying and fixing bugs in the code.

[back](./)
