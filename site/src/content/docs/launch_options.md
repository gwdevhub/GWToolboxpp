---
title: "Launcher Command-Line Arguments"
section: meta
description: "Command-line arguments accepted by GWToolbox.exe, the launcher that injects Toolbox into Guild Wars."
---

`GWToolbox.exe` is the small launcher that installs, updates, and injects the Toolbox DLL into a running copy of Guild Wars. It accepts a handful of command-line arguments, which are useful when scripting startup, running from a third-party launcher, or working with a local build.

You can pass these by editing your shortcut's **Target** field, calling the exe from a batch/PowerShell script, or running it from a terminal. On Linux/Wine, append them after the exe path in your `wine start` command.

## Arguments

| Argument | Effect |
| --- | --- |
| `/?`, `/help` | Print the list of arguments and exit. |
| `/version` | Print the launcher version and exit. |
| `/quiet` | Run without any user interaction — no prompts or dialogs. Useful for scripted/silent launches. |
| `/install` | Create the necessary folders and download `GWToolboxdll.dll`, then exit. |
| `/asadmin` | Relaunch as administrator if not already elevated. |
| `/localdll` | Inject the `GWToolboxdll.dll` sitting next to `GWToolbox.exe` instead of the installed copy. |
| `/pid <process id>` | Inject into the Guild Wars process with the given PID instead of searching for one. |

## Notes

- `/quiet` requires Toolbox to already be installed. If it isn't, or no game can be injected into, the launcher window opens and shows the error.
- `/localdll` is mainly for developers running a local build — it loads the DLL from the launcher's own directory and ignores the installed copy.

## Examples

Launch silently using a locally-built DLL:

```text
GWToolbox.exe /quiet /localdll
```

Inject into a specific Guild Wars process:

```text
GWToolbox.exe /pid 12345
```
