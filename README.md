# Grand Theft Auto III - Archipelago

**Claude never says a word, so the multiworld does the talking.**

An [Archipelago](https://archipelago.gg) multiworld integration for classic **Grand Theft Auto III**: passing missions sends checks, and the items you receive unlock the missions you can play.

> **Alpha.** A run plays from the first mission to the goal, but a new session starts the story again: play a run in one session for now.

## How it plays

- **Checks**: the 65 story missions, and 8 side missions open from the start on their island (the four RC Toyz missions, the three 4x4 challenges, Multistorey Mayhem).
- **Items**:
  - what unlocks each story mission: a locked mission's marker stays hidden until then;
  - a license for nine weapons (Uzi, shotgun, AK-47, M-16, sniper rifle, rocket launcher, flamethrower, Molotov cocktail, grenade): shops show the weapon out of stock until you receive it; the pistol and the body armour are sold as in the game;
  - $10000, the only extra item, which fills the rest of the pool.
- **Excluded locations** (`exclude_locations`): each one takes the place of a license, from the grenade's back, and gets a $10000; that weapon is then sold as in the game. 9 at most in a room of GTA III alone.
- **Between sessions**: GTA3 Client keeps your money, health, armour and weapons in a save file, one per room; a new game started with Run Archipelago gets them back, with the items received meanwhile. Missions passed do not come back yet.
- **Any order**: the mod starts an unlocked mission's marker when the story has not, and lifts the story's locks that only hold for one order (Salvatore's pause between Called a Meeting and Chaperone, his lock between his missions).
- **Islands**: Staunton Island opens with Last Requests, Shoreside Vale with A Drop in the Ocean, as in the game.
- **Missions the story closes**: Last Requests closes the Mafia's missions left in Portland, Waka-Gashira Wipeout! Kenji's, Grand Theft Aero Two-Faced Tanner's; their checks are sent with the mission that closes them.
- **Goal**: pass The Exchange, Catalina's final mission.

### Options

- `mission_order`: `ordered` (default), each contact gives a progressive item and their missions come in story order; `free`, each mission has its own unlock and plays in any order.
- `need_money`: `false` (default), Bomb Da Base: Act II and The Exchange start without paying; `true`, they ask for $100000 and $500000, as in the game. The logic counts no money either way.
- `death_link`: `false` (default) or `true`, Archipelago's DeathLink: when you are wasted, every DeathLink player of the room dies, and when one of them dies, you are wasted.
- `arrest_is_death`: `true` (default) or `false`, with `death_link` on: being busted counts as a death too.
- `start_inventory_from_pool`: Archipelago's own, like `Uzi License: 1`: you start with these items, and each one leaves a $10000 in the pool.

## Installation

- **You need**:
  - **GTA III for PC, version 1.0** (for the Steam copy, see [Steam version](#steam-version));
  - [Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases) 0.6.7 or newer;
  - [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader), 32-bit: its [`Ultimate-ASI-Loader.zip`](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest/download/Ultimate-ASI-Loader.zip), not the `_x64` one.
- **Release files**:
  - `GTA3Archipelago.asi`: the game plugin;
  - `gta3.apworld`: the GTA III world and its client, GTA3 Client;
  - `LICENSE-MinHook.txt`: the license of MinHook, which the plugin contains.
- Use the plugin and the APWorld of the same release, and generate a new room for each new version.

### Setup

1. Put the `dinput8.dll` of [Ultimate-ASI-Loader.zip](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/latest/download/Ultimate-ASI-Loader.zip) next to `gta3.exe`.
2. Put `GTA3Archipelago.asi` in a `scripts` folder next to `gta3.exe`, created if there is none.
3. Double-click `gta3.apworld` to install it in Archipelago (or copy it into Archipelago's `custom_worlds` folder).
4. Write your player settings in a YAML file, for example `GTA3.yaml`, and give it to whoever generates the room:
   ```yaml
   name: YourName
   game: Grand Theft Auto III
   Grand Theft Auto III:
     mission_order: free
     need_money: false
     death_link: false
   ```
5. Once the room is up, open **GTA3 Client** from the Archipelago Launcher and connect to the room with your slot name.
6. Start GTA III: the main menu's first button reads **Run Archipelago** while GTA3 Client is connected (**Not connected** otherwise) and starts a new game.
7. Keep GTA3 Client open while you play; after a reconnection, the missions passed are sent again.

## Steam version

The Steam `gta3.exe` is newer than 1.0: downgrade it once, before installing the mod.

1. In Steam, right-click **Grand Theft Auto III**, then **Manage** → **Browse local files**.
2. Copy `gta3.exe` somewhere safe, for example as `gta3.exe.steam`.
3. Replace `gta3.exe` with the 1.0 executable, following the [TJGM GTA III downgrade guide](https://tjgm.dev/guides/gtaiii/How-to-Downgrade-EVERY-Version-of-GTA-III/).
4. Follow the [installation](#installation) steps.
5. Copy `SilentPatchIII.asi` and `SilentPatchIII.ini` from [SilentPatch](https://github.com/CookiePLMonster/SilentPatch) next to `gta3.exe`; without it, the 1.0 executable asks for the CD.

- If Steam verifies the game files, it puts its own `gta3.exe` back: redo step 3.
- The Rockstar Games Launcher copy works the same way, once the game folder is moved out of `Program Files`.

## Building

- **You need** Visual Studio 2022 with the C++ x86 tools, Archipelago 0.6.7, and two libraries outside this repository, each found through an environment variable:
  - [MinHook](https://github.com/TsudaKageyu/minhook), commit `d94c64d` (v1.3.4 + 14), `MINHOOK_DIR`: compiled into the ASI;
  - [plugin-sdk](https://github.com/DK22Pac/plugin-sdk), commit `15f15b6`, `PLUGIN_SDK_DIR`: its GTA III library (`Plugin_III.lib`) is compiled into the ASI.

```powershell
git clone https://github.com/TsudaKageyu/minhook.git
git -C minhook checkout d94c64d32ea37bc4f5ee47d580709f70c6fb6080
git clone https://github.com/DK22Pac/plugin-sdk.git
git -C plugin-sdk checkout 15f15b60bbf74c106e1b496ff92c98764abf4605
setx MINHOOK_DIR "C:\path\to\minhook"
setx PLUGIN_SDK_DIR "C:\path\to\plugin-sdk"
```

- **First build**: it also compiles `Plugin_III.lib` with plugin-sdk's own tools (premake and MSBuild, about a minute). After checking out another plugin-sdk commit, delete `%PLUGIN_SDK_DIR%\output\lib\Plugin_III.lib` so the next build makes it again.
- **Dev build**: adds an autopilot that skips the start-up videos, starts a game and plays the test missions by itself (F8 pauses it), and cheat keys: F3 fails the running mission, F5 teleports to the mission blip, F6 kills the character the mission marks, F7 gives a car, F9 writes the position to the log. Never ship a dev build.
- **VS Code**: with the libraries cloned next to this repository, `GTAPIII.code-workspace` opens the three folders side by side.

### Tests

- `python apworld\tools\autotest.py` plays a test run with nothing started by hand: it generates a room from `apworld\players`, hosts it on port 38300, opens GTA3 Client and the game (with a dev build installed), lets the autopilot play, and sums up the game's log. Close GTA3 Client first, and leave the game alone until the run ends.
  - `--shop`: nobody plays; the player walks onto the Uzi of Portland's Ammu-Nation before and after the room sends its license, to check that the shop sells it only with the license.
  - `--player`: two sessions in a row; the second, with a new client and a new game, must get back the first one's money, health, armour and weapons, skip the items already applied, and apply those received in between.
- `python apworld\tools\check_client.py` checks GTA3 Client's lines to the mod and its save file, with stubs in place of Archipelago and no game.
- `python apworld\tools\check_generation.py` generates a room for every combination of the options (and a few with other games) and checks what each one holds, once the apworld is installed.

## Credits

This mod stands on the shoulders of the GTA modding community. Thank you to:

- **Dmitry K. ([DK22Pac](https://github.com/DK22Pac)), fastman92, LINK/2012 and every contributor of [plugin-sdk](https://github.com/DK22Pac/plugin-sdk)** (zlib license). Their years of reverse engineering mapped GTA III's code and memory; this mod is built on their GTA III library, which gives it the game's addresses, structures and functions.
- **Tsuda Kageyu, for [MinHook](https://github.com/TsudaKageyu/minhook)** (BSD 2-Clause license, shipped as `LICENSE-MinHook.txt` with each release), which hooks the game's functions.
