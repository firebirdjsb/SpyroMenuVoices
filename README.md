# Spyro Menu Voices

Native x64 ASI that restores the scrapped title and game-selection voice lines in Spyro Reignited Trilogy.

## Reference behavior

The native menu hook is designed to match the supplied working reference recording:

- Play **“Spyro Reignited Trilogy”** as the trilogy title screen appears.
- Play the currently active game name immediately when the three-game chooser opens.
- Play Spyro 1, Spyro 2, or Spyro 3 immediately when that game becomes active through mouse hover, keyboard, or controller navigation.
- Re-entering the same game button may replay its line; only duplicate event chatter within 150 ms is ignored.
- The embedded WAV is played at its original full level with no added fade or artificial delay.

The ASI listens to Falcon's native `SetActiveGameIndex` event. Game indices map as `0 = Spyro 1`, `1 = Spyro 2`, and `2 = Spyro 3`; the invalid/root index maps to the Reignited Trilogy title line. The hook is installed on the Falcon blueprint-library default object instead of detouring global `ProcessEvent`, so it can coexist with other ASIs that hook `ProcessEvent`.

## Install

Copy `SpyroMenuVoices.asi` beside:

```text
Falcon\Binaries\Win64\Spyro-Win64-Shipping.exe
```

No loose WAV files are required. All four supplied clips are embedded in the ASI.

## Diagnostics

`SpyroMenuVoices.log` is created beside the ASI. A healthy startup should include:

```text
ready channels=2 rate=48000 bits=16 clips=4
[MenuEvents] found function=SetActiveGameIndex ...
[MenuEvents] installed per-object ProcessEvent bridge ...
```

Each menu event should then show the native game index and selected cue:

```text
[MenuEvents] SetActiveGameIndex index=2 -> cue=3
cue=3 started ...
```

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

The output is `build\Release\SpyroMenuVoices.asi`.

## Exported API

- `SpyroMenuVoices_PlayCue(int cue)`
- `SpyroMenuVoices_GetState()`
- `SpyroMenuVoices_GetHookState()`
