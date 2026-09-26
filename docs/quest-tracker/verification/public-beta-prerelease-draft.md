# Public beta prerelease draft (not published)

Status: **draft for human review only**.  
Do **not** create a GitHub tag/Release, upload binaries, merge, or announce until the human owner decides.

Prepared against gate evidence dated **2026-09-26** and docs branch work on top of `master` @ `c2280eb1e2cbb40dac04992b37883e0347db2ecb` (confirm HEAD at publish time).

## Suggested tag and title

| Field | Suggestion |
|-------|------------|
| Tag | `8.34_Beta2+quest` |
| Release title | `8.34_Beta2+quest — Quest Tracker ↔ Tyrian Wayfarer progress bridge (fork only)` |
| Release type | **Prerelease** (never mark as latest official Toolbox) |

Clarify in the Release description first line: **Unofficial fork build. Not supported by gwdevhub/GWToolboxpp.**

## Short release notes (draft)

```text
Unofficial vinogitz/GWToolboxpp RelWithDebInfo build for Quest Tracker → Tyrian Wayfarer Contract v1 beta.

Includes:
- Observational Quest Progress Contract v1 export (disappearance ≠ completion)
- In-Toolbox “Data for Tyrian Wayfarer” help
- Fork updater: official GitHub DLL only via explicit Official DLL confirm; prefer Settings → Updater → Do not check for updates when testing

Version labels:
- Settings UI: 8.34 +quest Beta2
- Contract producer.version: 8.34 (Beta2 / +quest are not in that JSON field)

Verified on 2026-09-26 (redacted): QuestProgressTests 1198/0; Pre-Searing export → Wayfarer web import (3 tracked / 0 completed / Life journey); same-file re-import no duplicates; /localdll RelWithDebInfo DLL still loaded after full Gw.exe restart.

Install: see docs/quest-tracker/fork-beta-install.md
Export/import honesty: docs/quest-tracker/toolbox-wayfarer-export-guide.md
Gate checklist: docs/quest-tracker/verification/public-beta-gate.md

Source zip/tarball from GitHub is NOT a drop-in binary.
```

## Files to ship (binaries — outside git)

Publish as Release **assets** (or a private tester channel), **not** as git commits:

| Asset | Notes |
|-------|--------|
| `GWToolboxdll.dll` | RelWithDebInfo fork build used for the gate |
| `GWToolboxdll.pdb.gz` | Optional; RelWithDebInfo compressed PDB if distributing for crash triage |
| `GWToolbox.exe` | Optional; only if testers lack a matching launcher. Prefer documenting “place DLL next to your existing launcher” when possible |

Do **not** ship: raw Contract JSON exports, character names/keys, screenshots that show them, or unrelated plugins.

## SHA256 verification procedure

On the machine that built the gate DLL (2026-09-26):

```powershell
Get-FileHash path\to\GWToolboxdll.dll -Algorithm SHA256
```

Expected for that verified build:

```text
FA23EEBA1CAD8B3E258837CF3EF4BB7C3A5478ECDD8B12D7A2216247A7116577
```

Before publishing a **new** binary, recompute SHA256 after a clean RelWithDebInfo `GWToolboxdll` build and put the **new** hash in the Release body. Do not reuse the hash above if the bits changed.

Testers should run the same `Get-FileHash` (or `certutil -hashfile GWToolboxdll.dll SHA256`) on the downloaded DLL and compare to the Release notes.

## Open release decisions (human)

- [ ] Confirm tag string (`8.34_Beta2+quest` vs another spelling)
- [ ] Confirm whether to attach `GWToolbox.exe` / PDB or DLL-only
- [ ] Confirm tester audience (private vs public prerelease)
- [ ] Confirm Wayfarer consumer build readiness (separate repo)
- [ ] Confirm no official branding / no gwtoolbox.com homepage link as “download Toolbox”
- [ ] After publish: update this draft’s “published” status and pin SHA256 of the **shipped** asset
