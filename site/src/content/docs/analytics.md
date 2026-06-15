---
title: "Anonymous Analytics"
section: meta
description: "What anonymous gameplay data GWToolbox++ sends, to whom, and how to opt out."
---

GWToolbox++ can send anonymous gameplay data to third-party services to help improve those services for the Guild Wars community. This is **opt-in by default via the "Send anonymous gameplay stats" checkbox** in [Settings](/docs/settings/#toolbox-settings) and can be disabled at any time.

No account name, character name, IP address, or any other personally identifying information is ever included in these transmissions.

## Opting out

Open **Settings → Toolbox Settings** and uncheck **Send anonymous gameplay stats**. All analytics transmissions described below will stop immediately.

## Data sent and when

### gwmarket.net — purchase analytics

**When it happens:** You click the **Whisper** button on a listing in the [Market Browser](/docs/market_browser/) and then send a whisper to that same player within 60 seconds.

**What is sent:** A single HTTP POST to `https://gwmarket.net/api/shop/purchase/` containing:

| Field | Value |
|-------|-------|
| `name` | Item name (e.g. `"Glob of Ectoplasm"`) |
| `orderType` | `0` for a sell order, `1` for a buy order |
| `price.type` | Currency: `0` = Platinum, `1` = Ecto, `2` = Zkeys, `3` = Arms |
| `price.price` | Quoted price |
| `price.quantity` | Quoted quantity |

**Why it is useful:** gwmarket.net uses this to track which listings result in completed trades, helping it surface more accurate pricing data for the community.

### party.gwtoolbox.com — outpost party information

**When it happens:** While you are in an outpost, Toolbox periodically shares your party composition with party.gwtoolbox.com.

**What is sent:** Party size and outpost location — no character or account names.

**Why it is useful:** This powers the cross-client "party search" feature so players can find groups in the same outpost.
