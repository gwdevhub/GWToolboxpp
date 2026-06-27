---
title: "Privacy Policy"
section: meta
description: "What data GWToolbox++ collects, what it does not, and how to opt out."
---

GWToolbox++ is free, open-source software that runs locally on your computer. It does not require an account, and it does not collect personal information such as your name, email address, or IP address. This page explains the limited data the software does send, when it is sent, and how to turn it off.

## What we do not collect

- No account name, email address, or password.
- No IP address logging by GWToolbox++.
- No advertising or third-party tracking.
- The GWToolbox++ website (gwtoolbox.com) uses no analytics, cookies, or tracking scripts.

## Anonymous gameplay data (opt-in toggle)

GWToolbox++ can send **anonymous gameplay data** to third-party services to power community features. This is controlled by the **"Send anonymous gameplay stats"** checkbox in **Settings → Toolbox Settings** and can be disabled at any time, which stops all such transmissions immediately.

No account name or IP address is ever included. Some transmissions include character names (which are already publicly visible in-game) or a persistent **account UUID** that is derived from your Guild Wars account and cannot be reversed into your account name or email.

The two services involved are:

- **gwmarket.net** — when you whisper a seller/buyer from the Market Browser, an anonymous record of the item, price, and order type is sent so the service can track completed trades.
- **party.gwtoolbox.com** — while you are in an outpost, the party-search listings already visible to you are relayed so other players can find groups across clients.

A full field-by-field breakdown of exactly what is sent, and when, is on the [Anonymous Analytics](/docs/analytics/) page.

## Update checks

The launcher and the Toolbox DLL contact GitHub's public release API to check for and download updates. These are ordinary web requests to GitHub; GitHub may log them under [its own privacy policy](https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement). GWToolbox++ sends no additional data with these requests.

## Crash dumps

When GWToolbox++ crashes it may write a crash dump file to your local `Documents\GWToolboxpp` folder. These files stay on your computer and are **not** uploaded automatically. You may choose to share one with the developers (for example, on Discord or GitHub) when reporting a bug.

## Contact

Questions about this policy can be raised on our [GitHub issue tracker](https://github.com/gwdevhub/GWToolboxpp/issues) or [Discord](https://discord.gg/pGS5pFn).
