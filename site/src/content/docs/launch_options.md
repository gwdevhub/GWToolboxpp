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
| `/uninstall` | Remove all data used by GWToolbox, then exit. |
| `/reinstall` | Do a fresh installation (uninstall, then install), then exit. |
| `/asadmin` | Relaunch as administrator if not already elevated. |
| `/noupdate` | Don't check for or download an updated DLL on launch. |
| `/noexecheck` | Don't check GitHub for a newer `GWToolbox.exe`. |
| `/noinstall` | Don't offer to install the DLL if it's missing. |
| `/localdll` | Inject the `GWToolboxdll.dll` sitting next to `GWToolbox.exe` instead of the installed copy. Implies `/noupdate` and `/noinstall`. |
| `/pid <process id>` | Inject into the Guild Wars process with the given PID instead of searching for one. |

## Notes

- `/install`, `/uninstall`, and `/reinstall` are mutually exclusive — pass at most one.
- `/quiet` requires Toolbox to already be installed; it will error out rather than prompt you to install.
- `/localdll` is mainly for developers running a local build — it loads the DLL from the launcher's own directory and skips updating and installing entirely.

## Examples

Launch silently using a locally-built DLL, without updating or installing:

```text
GWToolbox.exe /noupdate /noinstall /quiet /localdll
```

Run without ever checking for updates (e.g. to stay on a specific release):

```text
GWToolbox.exe /noupdate
```

> **Note:** `/noupdate` only stops the *launcher* from updating the DLL at startup. The Toolbox DLL has its own update setting (Toolbox **Settings → Updater → Update mode**) which may be set to check for and apply updates on its own. To stay on a specific release, also set that to **Do not check for updates**.

Inject into a specific Guild Wars process:

```text
GWToolbox.exe /pid 12345
```
