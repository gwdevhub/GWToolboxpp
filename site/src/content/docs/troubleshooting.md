---
title: "Troubleshooting"
section: meta
---

Most problems launching GWToolbox++, loading its add-ons, or saving a crash dump come down to **security software** — Windows Defender, third-party antivirus, or **Controlled Folder Access** — blocking Toolbox from reading Guild Wars' memory, writing to your Documents folder, or loading a `.dll`. This page explains how to fix those, with direct links you can share.

If you are not sure which section you need, the error message Toolbox showed you links straight to the relevant section below.

## Before you start

Work through these first — they resolve the majority of issues:

1. **Update to the latest version.** Re-download `GWToolbox.exe` from [gwtoolbox.com](https://gwtoolbox.com/) and run it. Old versions are not supported.
2. **Disable plugins and other mods** and test again. Toolbox will not write a crash dump while plugins are loaded.
3. **Launch Guild Wars in windowed mode**, not minimized or fullscreen, before starting Toolbox.
4. **Run as administrator** if Guild Wars itself is running as administrator (for example when started by a launcher).
5. **Avoid other programs that hook Guild Wars** at the same time (TexMod, uMod, multi-client launchers, screen/game overlays).

If you are still stuck after this, the cause is almost always security software — keep reading.

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

If you do not rely on Controlled Folder Access, you can instead turn it **Off** on the same screen.

## Toolbox can't read Guild Wars' memory

If the launcher reports **"Couldn't find character name RVA"** or that it **found Guild Wars but couldn't read its memory**, security software is almost always blocking Toolbox from reading the `Gw.exe` process — not a Toolbox bug.

- Add an [antivirus exclusion](#antivirus-exclusions) for your GWToolbox folder, and allow `Gw.exe`/`GWToolbox.exe` if your antivirus has an app allow list.
- If you have [Controlled Folder Access](#controlled-folder-access) on, allow Guild Wars and Toolbox through it.
- Then re-launch. If it still fails *after* you have done both, and you are on the latest version, report it — at that point it may be a genuine signature break from a Guild Wars update.

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

When Windows Defender blocks one of these files, recent Toolbox versions read the Defender event log and show you the exact block reason in the error popup — include that text when you ask for help.

## Still stuck?

If you have worked through the sections above and Toolbox still will not run:

- Note the **exact** error message (a screenshot is ideal).
- Confirm your **Toolbox version** and that you re-downloaded the latest `GWToolbox.exe`.
- Describe what you were doing when it happened, and whether any other game tools/overlays were running.

Then ask on the [Discord](https://discord.gg/ZpKzer4dK9) or open an issue on the [bug tracker](https://github.com/gwdevhub/GWToolboxpp/issues). See also the [FAQ](/docs/faq/) for more specific situations.

[back](./)
