---
title: "Troubleshooting"
description: "Fixing security software that blocks GWToolbox++, covering antivirus, Controlled Folder Access, Smart App Control, crash dumps, and missing images (the -image download)."
section: meta
---

Most problems launching GWToolbox++, loading its add-ons, or saving a crash dump come down to **security software** — Windows Defender, third-party antivirus, **Controlled Folder Access**, or **Smart App Control** — blocking Toolbox from reading Guild Wars' memory, writing to your Documents folder, or loading a `.dll`. This page explains how to fix those, with direct links you can share.

If you are not sure which section you need, the error message Toolbox showed you links straight to the relevant section below.

## Before you start

Work through these first — they resolve the majority of issues:

1. **Update to the latest version.** Re-download `GWToolbox.exe` from [gwtoolbox.com](https://gwtoolbox.com/) and run it. Old versions are not supported.
2. **Disable plugins and other mods** and test again. Toolbox will not write a crash dump while plugins are loaded.
3. **Launch Guild Wars in windowed mode**, not minimized or fullscreen, before starting Toolbox.
4. **Run as administrator** if Guild Wars itself is running as administrator (for example when started by a launcher).
5. **Avoid other programs that hook Guild Wars** at the same time (TexMod, uMod, multi-client launchers, screen/game overlays).

If you are still stuck after this, the cause is almost always security software — keep reading.

## Your browser blocked the download

Chromium-based browsers (Chrome, Edge, Brave, Opera, and others) increasingly block `GWToolbox.exe` **before it even finishes downloading**, showing a warning like *"This file isn't commonly downloaded"* or *"Failed – Virus detected"* with no obvious way to keep it. This is **Safe Browsing** reacting to an unsigned, infrequently-downloaded executable — the same false positive your antivirus makes — not a real threat.

If your browser will not let you keep the file, lower its Safe Browsing level, download, then turn it back up if you like:

1. Paste this into your browser's address bar and press Enter:
   - Chrome / Brave / Opera: `chrome://settings/security`
   - Edge: `edge://settings/privacy`
2. Find **Safe Browsing** (Edge calls it **Microsoft Defender SmartScreen**).
3. Switch it from **Enhanced protection** to **Standard protection**, or temporarily turn it **Off** / to **No protection**.
4. Re-download `GWToolbox.exe` from [gwtoolbox.com](https://gwtoolbox.com/).

You may still see a "keep anyway" prompt on the download — that one you *can* dismiss to keep the file. Once you have `GWToolbox.exe`, you can set Safe Browsing back to its previous level.

> **Note:** managed browsers (a work or school computer, or an enterprise antivirus that locks browser settings) can grey this out entirely. If so, download on a personal device, or ask whoever manages the machine to allow it.

## Antivirus exclusions

Toolbox has to inject into `Gw.exe` and read its memory, which looks like the behaviour antivirus software is built to stop — so antivirus (including Windows Defender) frequently flags it as a **false positive** and blocks it. The fix is to add an **exclusion** for your GWToolbox folder.

**Windows Defender (Windows Security):**

1. Open **Windows Security** (press Start, type *Windows Security*, press Enter).
2. Go to **Virus & threat protection**.
3. Under **Virus & threat protection settings**, click **Manage settings**.
4. Scroll to **Exclusions** and click **Add or remove exclusions**.
5. Click **Add an exclusion → Folder**.
6. Choose your GWToolbox folder: `%USERPROFILE%\Documents\GWToolboxpp`.

You can do the same in one step from PowerShell:

```powershell
Add-MpPreference -ExclusionPath ([Environment]::GetFolderPath('MyDocuments') + '\GWToolboxpp')
```

If you use a **third-party antivirus** (Avast, AVG, Bitdefender, Kaspersky, Malwarebytes, Norton, etc.), add the same folder — and ideally `Gw.exe` and `GWToolbox.exe` — to *its* exclusion/allow list. The wording differs per product, but every one of them has an "exclusions", "allow list", or "exceptions" screen.

After adding the exclusion, re-download `GWToolbox.exe` (your antivirus may already have quarantined it) and launch again.

## Controlled Folder Access

**Controlled Folder Access** is a Windows ransomware-protection feature. When it is on, it blocks programs it does not recognise from writing to protected folders — and your **Documents** folder is protected by default. Because Toolbox keeps its settings and crash dumps in `Documents\GWToolboxpp`, Controlled Folder Access can stop Toolbox from saving settings or **writing a crash dump**, and it can block reads in a way that looks like a crash.

An antivirus exclusion alone does **not** cover this — Controlled Folder Access is a separate setting, and you have to allow Guild Wars and Toolbox through it explicitly:

1. Open **Windows Security**.
2. Go to **Virus & threat protection**.
3. Under **Ransomware protection**, click **Manage ransomware protection**.
4. Make sure you can see **Controlled folder access**. If it is **On** and you want to keep it on, click **Allow an app through Controlled folder access**.
5. Click **Add an allowed app → Browse all apps**.
6. Add **`Gw.exe`** and **`GWToolbox.exe`**.

You can do the same from an **administrator** PowerShell window — allow both apps through Controlled Folder Access (replace the `Gw.exe` path with your own Guild Wars install), or turn the feature off entirely:

```powershell
# Allow Guild Wars and Toolbox to write to protected folders
Add-MpPreference -ControlledFolderAccessAllowedApplications 'C:\Path\To\Gw.exe'
Add-MpPreference -ControlledFolderAccessAllowedApplications ([Environment]::GetFolderPath('MyDocuments') + '\GWToolboxpp\GWToolbox.exe')

# ...or turn Controlled Folder Access off completely
Set-MpPreference -EnableControlledFolderAccess Disabled
```

If you do not rely on Controlled Folder Access, you can instead turn it **Off** on the same screen.

## Smart App Control

**Smart App Control** is a Windows 11 security feature that blocks apps it does not recognise as trusted — and because Toolbox is a small, frequently-updated tool that is not signed by a big publisher, Smart App Control will silently block `GWToolbox.exe` from running at all. If you have worked through the steps above and Toolbox still will not start, this is the likely cause.

1. Open **Windows Security**.
2. Go to **App & browser control**.
3. Under **Smart App Control**, click **Smart App Control settings**.
4. Set Smart App Control to **Off**.

> **Important:** Once you turn Smart App Control **off**, Windows will not let you turn it back on without **resetting or reinstalling Windows**. This is by design — only disable it if you are comfortable with that trade-off. (Smart App Control also has an "evaluation" mode that turns itself off automatically over time.)

After disabling it, re-download `GWToolbox.exe` from [gwtoolbox.com](https://gwtoolbox.com/) and launch again.

## Toolbox can't read Guild Wars' memory

If the launcher reports **"Couldn't find character name RVA"** or that it **found Guild Wars but couldn't read its memory**, security software is almost always blocking Toolbox from reading the `Gw.exe` process — not a Toolbox bug.

- Add an [antivirus exclusion](#antivirus-exclusions) for your GWToolbox folder, and allow `Gw.exe`/`GWToolbox.exe` if your antivirus has an app allow list.
- If you have [Controlled Folder Access](#controlled-folder-access) on, allow Guild Wars and Toolbox through it.
- Then re-launch. If it still fails *after* you have done both, and you are on the latest version, report it — at that point it may be a genuine signature break from a Guild Wars update.

If antivirus is intercepting the injection itself, excluding the two processes from scanning (in an **administrator** PowerShell window) can also help:

```powershell
Add-MpPreference -ExclusionProcess 'Gw.exe','GWToolbox.exe'
```

## Crash dump errors

When Guild Wars crashes, Toolbox tries to write a **crash dump** (`.dmp`) into `Documents\GWToolboxpp\<ComputerName>\crashes`. If a popup says Toolbox **couldn't find your Documents folder**, **couldn't create the crash file**, or that the dump **disappeared after being written**, something is blocking that folder:

- This is most often [Controlled Folder Access](#controlled-folder-access) protecting your Documents folder — allow Guild Wars and Toolbox through it.
- It can also be [antivirus](#antivirus-exclusions) quarantining the dump as it is written — add an exclusion for the GWToolbox folder.

The popup also shows the **original error** that caused the crash. If you report the issue, include that text (or a screenshot of the whole popup) so the team can see the root cause even when no dump file could be saved.

Once a dump is written, see the FAQ for [how to send a crash dump to the team](/docs/faq/#gwtoolbox-just-crashed-how-do-i-send-a-crash-dump-to-the-team).

## Blocked or quarantined files

If the launcher says a file such as `GWToolboxdll.dll`, `gwca.dll`, or `gMod.dll` **"did exist, but now it doesn't"**, or a plugin/texture mod fails to load, antivirus has quarantined or deleted that file:

- Add an [antivirus exclusion](#antivirus-exclusions) for the GWToolbox folder so the files are not removed again.
- Re-download `GWToolbox.exe` from [gwtoolbox.com](https://gwtoolbox.com/) and re-launch so the missing files are restored.

If Windows Defender quarantined the files, you can restore them from an **administrator** PowerShell window instead of re-downloading (add the exclusion above first, or they may be quarantined again):

```powershell
Restore-MpThreat
```

When Windows Defender blocks one of these files, recent Toolbox versions read the Defender event log and show you the exact block reason in the error popup — include that text when you ask for help.

## Missing images in Toolbox

If item, armor, skill, or map images show up **blank** in Toolbox — for example in the Armory, the skills list, or item tooltips — and Toolbox logs a message about not finding image data in `Gw.dat`, the cause is almost always an **incomplete local game install**, not a Toolbox bug.

Guild Wars can run from a partial `Gw.dat` and **stream** the rest of its data from ArenaNet's servers on demand as you play — the **Steam** release does this by default, so a fresh install only has the data you have actually seen in-game. Toolbox reads images straight from that `Gw.dat` on disk: whatever the game hasn't downloaded yet simply isn't there for Toolbox to load, which is why images you own or have seen appear while others stay blank.

The fix is to download **all** game data once, with the `-image` command-line option. Guild Wars will fetch the complete `Gw.dat` (several GB — it can take a while), after which every image is available to both the game and Toolbox.

**Steam:**

1. In your Steam **Library**, right-click **Guild Wars** and choose **Properties**.
2. On the **General** tab, find **Launch Options**.
3. Enter `-image` in the box.
4. **Launch** Guild Wars and leave it running until the download completes (there is a progress bar; it may take a long time on a slow connection).
5. Close Guild Wars, reopen **Properties → Launch Options**, and **remove** `-image` so it doesn't re-check every launch.

**Standalone client (`Gw.exe`):**

- Add `-image` to the end of your shortcut's **Target** (right-click the shortcut → **Properties**), e.g. `"C:\Path\To\Gw.exe" -image`, launch it, let it finish, then remove the `-image` again — or just run it once from a command prompt: `Gw.exe -image`.

Once the download has finished, restart Guild Wars (and Toolbox) and the images will load. If a specific image is *still* missing after a full `-image` download, that one may be genuinely absent from your client version — report it on the [Discord](https://discord.gg/ZpKzer4dK9).

## Still stuck?

If you have worked through the sections above and Toolbox still will not run:

- Note the **exact** error message (a screenshot is ideal).
- Confirm your **Toolbox version** and that you re-downloaded the latest `GWToolbox.exe`.
- Describe what you were doing when it happened, and whether any other game tools/overlays were running.

Then ask on the [Discord](https://discord.gg/ZpKzer4dK9) or open an issue on the [bug tracker](https://github.com/gwdevhub/GWToolboxpp/issues). See also the [FAQ](/docs/faq/) for more specific situations.

[back](./)
