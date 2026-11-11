# Grand Theft Auto III — Archipelago

**Claude never says a word, so the multiworld does the talking.**

An [Archipelago](https://archipelago.gg) multiworld integration for classic **Grand Theft Auto III**: passing missions sends checks, and the items you receive unlock the missions you can play.

> **Alpha.** A run plays from the first mission to the goal, but saving and loading a game is not supported yet: play a run in one session for now.

## How it plays

- **Checks**: every story mission (65), and 8 side missions open from the start on their island: the four RC Toyz missions, the three 4x4 challenges and Multistorey Mayhem.
- **Items**: what unlocks each story mission, and $10000 cash for the rest. A locked mission's marker is hidden in the game; once unlocked, its marker shows on the radar and on the ground.
- **Any order**: the mod starts the marker of an unlocked mission itself when the story has not, one mission per marker at a time, and takes off the story's locks that only hold for some order (the pause between Salvatore's Called a Meeting and Chaperone, and Salvatore's lock between his missions).
- **Islands**: they open as in the game. Staunton Island once Last Requests is passed, Shoreside Vale once A Drop in the Ocean is.
- **Missions the story closes**: once Last Requests is passed, the Mafia's missions left in Portland can never be played; Waka-Gashira Wipeout! does the same to Kenji's, Grand Theft Aero to Two-Faced Tanner. Their checks are sent with the mission that closes them.
- **Goal**: pass The Exchange, Catalina's final mission.

### Options

| Option | Values | What it does |
| --- | --- | --- |
| `mission_order` | `ordered` (default), `free` | `ordered`: each contact gives a progressive item, and their missions come in story order. `free`: every mission has its own unlock and can be played in any order. |
| `need_money` | `false` (default), `true` | `true`: Bomb Da Base: Act II asks for $100000 and The Exchange for $500000, as in the game. `false`: both start without paying. The logic counts no money either way. |
| `death_link` | `false` (default), `true` | Archipelago's DeathLink: when you are wasted, every DeathLink player of the room dies, and when one of them dies, you are wasted. |
| `arrest_is_death` | `true` (default), `false` | With `death_link` on: being busted counts as a death too. |

## Installation

You need **GTA III for PC, version 1.0** (see [Steam version](#steam-version) for the Steam copy), [Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases) 0.6.7 or newer, and an ASI loader such as [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader).

### Release files

| File | Use |
| --- | --- |
| `GTA3Archipelago.asi` | The game plugin. |
| `gta3.apworld` | The GTA III world and its client, GTA3 Client, for Archipelago. |
| `LICENSE-MinHook.txt` | The license of MinHook, which the plugin contains. |

Use the plugin and the APWorld of the same release, and generate a new room for each new version.

### Setup

1. Put the ASI loader (`dinput8.dll` from Ultimate ASI Loader) next to `gta3.exe`.
2. Create a `scripts` folder next to `gta3.exe` if there is none, and put `GTA3Archipelago.asi` in it.
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
6. Start GTA III. The main menu's first button reads **Run Archipelago** while GTA3 Client is connected to the room (**Not connected** otherwise); it starts a new game. Keep GTA3 Client open while you play.

The game shows each item you receive; passed missions reach the room through GTA3 Client, and are sent again after a reconnection.

## Steam version

The Steam `gta3.exe` is newer than 1.0, so downgrade it once before installing the mod:

1. In Steam, right-click **Grand Theft Auto III**, then **Manage** → **Browse local files** to open the game folder.
2. Copy `gta3.exe` somewhere safe, for example as `gta3.exe.steam`.
3. Replace `gta3.exe` with the 1.0 executable by following the [TJGM GTA III downgrade guide](https://tjgm.dev/guides/gtaiii/How-to-Downgrade-EVERY-Version-of-GTA-III/).
4. Follow the [installation](#installation) steps above.
5. Copy `SilentPatchIII.asi` and `SilentPatchIII.ini` from [SilentPatch](https://github.com/CookiePLMonster/SilentPatch) next to `gta3.exe`; without it, the 1.0 executable asks for the CD.

If Steam verifies the game files, it puts its own `gta3.exe` back: redo step 3. The Rockstar Games Launcher copy works the same way once the game folder is moved out of `Program Files`.

## Building

You need Visual Studio 2022 with the C++ x86 tools, Archipelago 0.6.7, and two libraries kept outside this repository; each one is found through an environment variable:

| Library | Version | Variable | Use |
| --- | --- | --- | --- |
| [MinHook](https://github.com/TsudaKageyu/minhook) | commit `d94c64d` (v1.3.4 + 14) | `MINHOOK_DIR` | compiled into the ASI |
| [plugin-sdk](https://github.com/DK22Pac/plugin-sdk) | commit `15f15b6` | `PLUGIN_SDK_DIR` | its GTA III library (`Plugin_III.lib`) is compiled into the ASI |

```powershell
git clone https://github.com/TsudaKageyu/minhook.git
git -C minhook checkout d94c64d32ea37bc4f5ee47d580709f70c6fb6080
git clone https://github.com/DK22Pac/plugin-sdk.git
git -C plugin-sdk checkout 15f15b60bbf74c106e1b496ff92c98764abf4605
setx MINHOOK_DIR "C:\path\to\minhook"
setx PLUGIN_SDK_DIR "C:\path\to\plugin-sdk"
```

A dev build of the ASI adds an autopilot that skips the start-up videos, starts a game and plays the test missions by itself (F8 pauses it), with cheat keys: F3 fails the running mission, F5 teleports to the mission blip, F6 kills the character the mission marks, F7 gives a car, F9 writes the position to the log; never ship a dev build. The first build also compiles plugin-sdk's `Plugin_III.lib` with plugin-sdk's own tools (premake and MSBuild, about a minute); after checking out another plugin-sdk commit, delete `%PLUGIN_SDK_DIR%\output\lib\Plugin_III.lib` so the next build makes it again. With the libraries cloned next to this repository, `GTAPIII.code-workspace` opens the three folders side by side in VS Code.

## Credits

This mod stands on the shoulders of the GTA modding community. Thank you to:

- **Dmitry K. ([DK22Pac](https://github.com/DK22Pac)), fastman92, LINK/2012 and every contributor of [plugin-sdk](https://github.com/DK22Pac/plugin-sdk)** (zlib license). Their years of reverse engineering mapped GTA III's code and memory; this mod is built on their GTA III library, which gives it the game's addresses, structures and functions.
- **Tsuda Kageyu, for [MinHook](https://github.com/TsudaKageyu/minhook)** (BSD 2-Clause license, shipped as `LICENSE-MinHook.txt` with each release), which hooks the game's functions.
