# Grand Theft Auto III — Archipelago

**Claude never says a word, so the multiworld does the talking.**

An [Archipelago](https://archipelago.gg) multiworld integration for classic **Grand Theft Auto III**: missions and hidden packages send checks, and the items you receive unlock your progress.

> **Alpha, not playable yet.**

## How it plays

- **Checks**: story missions, side missions (RC, 4x4, Mayhem, Meat Factory) and one check every 10 hidden packages.
- **Items**: mission unlocks, the Staunton and Shoreside crossings, weapons and armor; cash fills the rest. Weapons replace the vanilla hidden-package rewards.
- **Goal**: finish Catalina's final mission.
- `ordered_chains` (on by default) keeps each contact's missions in story order; turn it off to shuffle them.

## Installation

### Release files

[Download 0.3.0-alpha.1](https://github.com/Leiqart/gta3-archipelago/releases/tag/v0.3.0-alpha.1)

| File | Use |
| --- | --- |
| `GTA3-Archipelago-0.3.0-alpha.1.zip` | Complete pack: game plugin, APWorld, YAML and installation instructions. |
| `gta3.apworld` | Install or update the GTA3 world and client in Archipelago; also included in the ZIP. |
| `GTA3.yaml` | Player configuration template; also included in the ZIP. |

The ZIP is ready to extract and copy; players do not need to build anything. Use the APWorld and plugin from the same release, and generate a new room for version 0.3.0.

### Setup

You need **GTA III for PC, version 1.0**, [Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases) 0.6.6 or newer, and an ASI loader such as [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader).

1. Put the ASI loader (`dinput8.dll`) next to `gta3.exe`.
2. Extract the release ZIP, then copy the contents of `gta3_game_files/` next to `gta3.exe`; `GTA3Archipelago.asi` goes into `scripts/`.
3. Double-click `archipelago/gta3.apworld` from the ZIP (or the separate `gta3.apworld` download) to install it in Archipelago.
4. Set your slot name and options in `archipelago/GTA3.yaml` from the ZIP (or the separate `GTA3.yaml` download), then generate or join a room.
5. Open **GTA3 Client** from the Archipelago Launcher and connect to the room.
6. Start GTA III, choose **Run Archipelago** in the main menu, and keep GTA3 Client open.

Manual saves are disabled during a run; your Archipelago progress is kept in `scripts/GTA3Archipelago-state/`. Full restoration of the GTA world after restarting is not implemented yet.

## Steam version

The Steam `gta3.exe` is newer than 1.0, so downgrade it once before installing the mod:

1. In Steam, right-click **Grand Theft Auto III**, then **Manage** → **Browse local files** to open the game folder.
2. Copy `gta3.exe` somewhere safe, for example as `gta3.exe.steam`.
3. Replace `gta3.exe` with the 1.0 executable by following the [TJGM GTA III downgrade guide](https://tjgm.dev/guides/gtaiii/How-to-Downgrade-EVERY-Version-of-GTA-III/).
4. Follow the [installation](#installation) steps above.
5. Copy `SilentPatchIII.asi` and `SilentPatchIII.ini` from [SilentPatch](https://github.com/CookiePLMonster/SilentPatch) next to `gta3.exe`; without it, the 1.0 executable asks for the CD.

If Steam verifies the game files, it puts its own `gta3.exe` back: redo step 3. The Rockstar Games Launcher copy works the same way once the game folder is moved out of `Program Files`.
