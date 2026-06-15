---
title: "Anonymous Analytics"
section: meta
description: "What anonymous gameplay data GWToolbox++ sends, to whom, and how to opt out."
---

GWToolbox++ can send anonymous gameplay data to third-party services to help improve those services for the Guild Wars community. This is **opt-in by default via the "Send anonymous gameplay stats" checkbox** in [Settings](/docs/settings/#toolbox-settings) and can be disabled at any time.

No account name or IP address is ever included in these transmissions. Some transmissions include character names or an account UUID as described below.

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

**When it happens:** While you are in an outpost, Toolbox periodically sends party search listings visible to you (and parties of nearby players) to party.gwtoolbox.com via a WebSocket connection.

**What is sent:**

The WebSocket connection headers include your **account UUID** (`X-Account-Uuid`) and the current Toolbox version as an API key. Your account UUID is a persistent identifier derived from your Guild Wars account; it is not your account name or email address.

Each party listing payload contains:

| Field | Value |
|-------|-------|
| `s` | **Character name** of the party leader |
| `ms` | Party search message (the text the player typed) |
| `t` | Search type (hunting, mission, quest, trade, guild) |
| `p` / `sc` | Primary and secondary profession |
| `ps` | Party size (omitted if 1) |
| `hc` | Hero count |
| `hm` | Hard mode flag |
| `dl` / `dn` | District language and number |
| `l` | Level (omitted if 20) |
| `map_id` | Current map (outpost) |
| `district_region` | Server region |

Character names sent here are the same names publicly visible to all players in the outpost via the in-game party search panel.

**Why it is useful:** This powers the cross-client "party search" feature so players can find groups in the same outpost.
