# Grand Theft Auto III

**Here, this rifle should help you pop some heads.**

This project in a mod to integrate the archipelago logic into **Grand Theft Auto III**

**The mod is currently in alpha and is also unstable**

## What does randomization do to this game?

Finishing a mission no longer advances the story on its own: it sends a *check* to the Archipelago server. In return you receive *items* (from your world or other players') that unlock your progression — contacts, islands, weapons, cash. You can only advance in the order the
multiworld decides.

## What is the goal?

Reach and complete **Catalina's final mission** (the *Victory* event).

## What gets shuffled?

- **Locations (checks)** — GTA III missions (per contact: Luigi, Joey, Toni, Asuka, Kenji, Ray,
  Donald Love, Yardies, Diablos, Hoods…) and hidden-package milestones (10, 20, … 100).
- **Items** — contact and island unlocks (Staunton, Shoreside), weapons and armor (*useful*),
  and cash (*filler*).

## Installation

Easiest path: the **ready-to-use pack** in [`release/GTA3-Archipelago/`](release/GTA3-Archipelago/).
Copy the contents of `gta3_game_files/` into your GTA III folder, edit `AP_connexion.txt`, then
generate your seed with `archipelago/gta3.apworld` + `GTA3.yaml`.

## Repository layout

```
archipelago_client/   apworld + sources (seed generation)
gta_3_mod/            in-game mod: ASI (C++) + native bridge (AP websocket)
release/              complete pack to drop into GTA III
```

## Not included

No Rockstar files are versioned. You need your own **legal copy of classic GTA III**. Not shipped:
the game itself, OpenSSL and the bridge's C++ libs, and build outputs.
