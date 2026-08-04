---
title: "Finding crashes in Event Viewer"
description: "A beginner-friendly guide to opening Windows Event Viewer and finding the entries Windows logs when Guild Wars or GWToolbox++ crashes, is blocked by Defender, or refuses to start."
section: meta
---

**Event Viewer** is a program built into Windows that keeps a diary of everything that goes wrong on your PC. When Guild Wars closes to desktop (a "CTD") and Toolbox showed you *no* error popup and left *no* crash dump, Event Viewer is usually the only place a clue survives.

This page walks you through it click by click. You do not need to understand any of it — you just need to find the right entry, copy it, and paste it to the team.

> **Read this first — it saves you the trouble.**
>
> When Toolbox catches a crash it handles everything itself: it shows you a popup and writes a `.dmp` **crash dump** file, and it deliberately tells Windows *not* to log the crash. So for a normal Toolbox crash, **Event Viewer will be empty — that is expected and not a problem.** The `.dmp` file is far more useful to the developers than anything Event Viewer can tell them.
>
> - **Did you get a Toolbox crash popup?** → Send the [crash dump](/docs/faq/#gwtoolbox-just-crashed-how-do-i-send-a-crash-dump-to-the-team) instead. You do not need this page.
> - **Did Guild Wars just vanish with no popup and no `.dmp`?** → Event Viewer is exactly the right tool. Keep reading.
> - **Does `GWToolbox.exe` not start at all, or do files keep disappearing?** → See [Windows Defender blocks](#windows-defender-blocked-something) below, then [Troubleshooting](/docs/troubleshooting/).

## Step 1: Open Event Viewer

Any one of these works — pick whichever looks easiest:

- **Easiest:** press the **Start** button (or the <kbd>Windows</kbd> key), type `Event Viewer`, and press <kbd>Enter</kbd>.
- Press <kbd>Windows</kbd> + <kbd>R</kbd>, type `eventvwr.msc`, press <kbd>Enter</kbd>.
- Right-click the **Start** button (or press <kbd>Windows</kbd> + <kbd>X</kbd>) and choose **Event Viewer**.

A window opens with a tree of folders on the left, a big list in the middle, and an **Actions** panel on the right. It can take a few seconds to load — that is normal.

> You do **not** need administrator rights for the Application log used below. If a log refuses to open, close Event Viewer, right-click it in the Start menu and choose **Run as administrator**.

## Step 2: Go to the Application log

In the left-hand tree:

1. Click the arrow next to **Windows Logs** to expand it.
2. Click **Application**.

The middle list fills with events, newest at the top. This is where Windows records program crashes.

## Step 3: Narrow it down to the time you crashed

The list is long and mostly irrelevant. Filter it:

1. In the **Actions** panel on the right, click **Filter Current Log…**
2. At the top, set **Logged** to **Last hour** (or **Last 12 hours** if the crash was a while ago).
3. Tick **Critical** and **Error**. Leave the rest unticked.
4. Click **OK**.

You should now be looking at a short list. Find the entries whose time matches the moment Guild Wars closed.

## Step 4: Spot the Toolbox / Guild Wars entries

Click each red **Error** entry near your crash time and read the text in the pane below. The ones worth caring about have a **Source** of:

| Source | What it means |
| --- | --- |
| **Application Error** (Event ID 1000) | A program crashed. This is the important one. |
| **Application Hang** (Event ID 1002) | Guild Wars froze rather than crashed. |
| **Windows Error Reporting** (Event ID 1001) | A follow-up report about the crash above. |
| **.NET Runtime** / **SideBySide** | A missing or broken Windows component stopped a program from starting. |

Inside an **Application Error** entry you will see two lines that matter:

```
Faulting application name: Gw.exe, version: ...
Faulting module name: GWToolboxdll.dll, version: ...
```

- **Faulting application name** — which program died. `Gw.exe` is Guild Wars, `GWToolbox.exe` is the Toolbox launcher.
- **Faulting module name** — which piece of code died inside it. This is the line that tells you whether Toolbox was involved:

| Faulting module | Most likely culprit |
| --- | --- |
| `GWToolboxdll.dll` | GWToolbox++ itself — definitely worth reporting. |
| `gwca.dll`, `gMod.dll`, or a plugin `.dll` | A Toolbox add-on or texture mod. Try disabling plugins. |
| `Gw.exe` or `ntdll.dll` | Guild Wars itself, or something too generic to tell. Still worth reporting. |
| `d3d9.dll`, `nvwgf2um.dll`, `atidxx32.dll`, `dxgi.dll` | Graphics driver. Update (or roll back) your GPU drivers. |
| Anything from an overlay (Discord, MSI Afterburner, RivaTuner, Steam) | That overlay. Turn it off and test again. |

If **no** Toolbox or Guild Wars entry exists at all around the crash time, that is genuinely useful information too — say so when you report it.

## Step 5: Copy the entry so you can send it

Do **not** retype it or crop a screenshot — copy the real text:

1. Right-click the entry in the list.
2. Choose **Copy → Copy Details as Text**.
3. Paste it into Discord or your GitHub issue. If it is long, paste it into a code block (put ` ``` ` on its own line before and after) or attach it as a `.txt` file.

**Prefer one command to all the clicking?** Press <kbd>Windows</kbd> + <kbd>R</kbd>, type `powershell`, press <kbd>Enter</kbd>, then paste this in and press <kbd>Enter</kbd>. It prints every Guild Wars / Toolbox-related event from the last hour:

```powershell
Get-WinEvent -FilterHashtable @{LogName='Application'; StartTime=(Get-Date).AddHours(-1)} -ErrorAction SilentlyContinue |
  Where-Object { $_.Message -match 'Gw\.exe|GWToolbox|gwca|gMod' } |
  Select-Object TimeCreated, Id, ProviderName, Message | Format-List
```

Change `AddHours(-1)` to `AddHours(-24)` to look back a full day. Select the output with the mouse and press <kbd>Enter</kbd> to copy it.

## Windows Defender blocked something

If `GWToolbox.exe` will not start, a `.dll` keeps disappearing, or no crash dump can be written, **Windows Defender** is the usual cause — and it logs its blocks in a *different* place. Toolbox already reads this log automatically and shows you the reason in its error popups, but you can check it yourself:

1. In the left-hand tree, expand **Applications and Services Logs → Microsoft → Windows → Windows Defender**.
2. Click **Operational**.
3. Use **Filter Current Log…** and set **Logged** to **Last 24 hours**.

Look for entries mentioning `GWToolbox`, `GWToolboxdll.dll`, `gwca.dll`, `gMod.dll`, `Gw.exe`, or your `Documents\GWToolboxpp` folder:

| Event ID | What Defender did |
| --- | --- |
| 1116 / 1117 | Detected and removed/quarantined a file (a false positive on a Toolbox file). |
| 1118 / 1119 | Tried to remove it and failed. |
| 1121 | An **Attack Surface Reduction** rule blocked it. |
| 1123 | **Controlled Folder Access** blocked a write to a protected folder — this is what stops crash dumps being saved. |

Any of these means the fix is on the [Troubleshooting](/docs/troubleshooting/) page: add an [antivirus exclusion](/docs/troubleshooting/#antivirus-exclusions) and allow Guild Wars and Toolbox through [Controlled Folder Access](/docs/troubleshooting/#controlled-folder-access).

The PowerShell equivalent, if you prefer:

```powershell
Get-WinEvent -FilterHashtable @{LogName='Microsoft-Windows-Windows Defender/Operational'; StartTime=(Get-Date).AddHours(-24)} -ErrorAction SilentlyContinue |
  Where-Object { $_.Message -match 'GWToolbox|gwca|gMod|Gw\.exe' } |
  Select-Object TimeCreated, Id, Message | Format-List
```

**On Windows 11 and `GWToolbox.exe` silently does nothing?** [Smart App Control](/docs/troubleshooting/#smart-app-control) may be blocking it. Its blocks are logged under **Applications and Services Logs → Microsoft → Windows → CodeIntegrity → Operational**, usually as Event ID **3077** or **3076** naming `GWToolbox.exe`.

## What to send the team

When you report a crash on the [Discord](https://discord.gg/ZpKzer4dK9) or the [bug tracker](https://github.com/gwdevhub/GWToolboxpp/issues), include as much of this as you have:

- The **crash dump** `.dmp` from `Documents\GWToolboxpp\<your PC name>\crashes`, zipped — this is the single most useful thing. See the [FAQ](/docs/faq/#gwtoolbox-just-crashed-how-do-i-send-a-crash-dump-to-the-team).
- The **Event Viewer entry** copied as text (Step 5), including the *faulting module name* line.
- Your **Toolbox version**, and confirmation you re-downloaded the latest `GWToolbox.exe` from [gwtoolbox.com](https://gwtoolbox.com/).
- Whether you had **plugins**, texture mods, or overlays running.
- What you were doing in game when it happened.

> **Careful with screenshots:** an Event Viewer entry can contain your Windows username and file paths. Blank out anything you would rather not share.

## Frequently asked

**There is nothing at all around my crash time.**
Normal for a crash Toolbox handled itself — Toolbox tells Windows not to log it. Look in `Documents\GWToolboxpp\<your PC name>\crashes` for a `.dmp` file instead. If there is no `.dmp` *and* nothing in Event Viewer, note that in your report; it points at something outside both programs (a driver, an overlay, or security software killing the process).

**The list is full of red errors from other programs.**
Ignore them. Windows logs errors constantly and almost none are related. Only entries whose text mentions `Gw.exe`, `GWToolbox`, `gwca` or `gMod`, at your crash time, are relevant.

**Can I clear these logs / will this break anything?**
Just reading is completely safe. There is no need to clear anything, and you should not — the entries are the evidence.

**Event Viewer says a log is unavailable or access denied.**
Re-open Event Viewer as administrator: right-click it in the Start menu → **Run as administrator**.

## See also

- [Troubleshooting](/docs/troubleshooting/) — fixing antivirus, Controlled Folder Access, Smart App Control and crash-dump errors.
- [FAQ](/docs/faq/) — including how to send a crash dump.

[back](./)
