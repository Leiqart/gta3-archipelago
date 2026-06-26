# Grand Theft Auto III

**Here, this rifle should help you pop some heads.**

This project in a mod to integrate the archipelago logic into **Grand Theft Auto III**

**The mod is currently in alpha and is also unstable**

## What does randomization do to this game?

Finishing a mission no longer advances the story on its own: it sends a *check* to the Archipelago server. In return you receive *items* (from your world or other players') that unlock your progression — contacts, islands, weapons, cash. You can only advance in the order the
multiworld decides.

## What is the goal?

Reach and complete **Catalina's final mission** (*The Exchange*).

## What gets shuffled?

- **Locations (checks)** — GTA III missions (per contact: Luigi, Joey, Toni, Asuka, Kenji, Ray,
  Donald Love, Yardies, Diablos, Hoods…) and hidden-package milestones (10, 20, … 100).
- **Items** — contact and island unlocks (Staunton, Shoreside), weapons and armor (*useful*),
  and cash (*filler*).

## Installation

How to install.

### 1. Install the game files

Open the gta3_game_files folder and drag & drop its entire content into your game folder (where GTA3.exe is located).
(Make sure you have version 1.1 of GTA 3; you can easily find it online.)

### 2. Configure your connection

Open AP_connexion.txt (now next to the game .exe) and edit it with your room's info:

ap_autoconnect=1
ap_server=archipelago.gg:12345 ; your room's address and port
ap_slot=YourSlotName ; your player/slot name
ap_password= ; leave empty if there is no password

### 3. Install the APWorld
Double-click gta3.apworld to install it into Archipelago.

### 4. Set up your YAML
Open GTA3.yaml and edit it to your liking:

Set your player name (must match ap_slot above)
Adjust the game options
Then place the YAML in your Archipelago Players folder and generate / host your game as usual.

### 5. Play !

## Repository layout

```
archipelago_client/   apworld + sources (seed generation)
gta_3_mod/            in-game mod: ASI (C++) + native bridge (AP websocket)
release/              complete pack to drop into GTA III
```

## Not included

No Rockstar files are versioned. You need your own **legal copy of classic GTA III**. Not shipped:
the game itself, OpenSSL and the bridge's C++ libs, and build outputs.
