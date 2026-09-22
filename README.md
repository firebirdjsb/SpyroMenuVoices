# Spyro Menu Voices

Native x64 ASI that restores the scrapped title and game-selection voice lines in Spyro Reignited Trilogy.

## Reference behavior

The native menu hook is designed to match the supplied working reference recording:

- Play **“Spyro Reignited Trilogy”** as the trilogy title screen appears.
- Play the currently active game name immediately when the three-game chooser opens.
- Play Spyro 1, Spyro 2, or Spyro 3 immediately when that game becomes active through mouse hover, keyboard, or controller navigation.
- Re-entering the same game button may replay its line; only duplicate event chatter within 150 ms is ignored.
- The embedded WAV is played at its original full level with no added fade or artificial delay.

Runtime tracing of the actual trilogy menu showed that `SetActiveGameIndex` and `SetGameIndex` do not drive hover/navigation. FalconGameplayStatics `GetGameIndex` does: its returned value changes in exact lockstep with the highlighted trilogy tile. The ASI uses both call cadence and active-game divergence as a conservative visibility gate. Startup/profile/save-screen polling is always silent. The trilogy selector is only considered real after a rapid `GetGameIndex` change diverges from `GetActiveGameIndex`, which matches observed selector navigation but not profile loading. Once proven, later `0/1/2` changes announce Spyro 1/2/3, and losing selector polling for about 280 ms marks a return to the root menu and announces “Spyro Reignited Trilogy” exactly once. A validated global `ProcessEvent` hook filters only the Falcon game-index UFunctions, so there is no per-frame name lookup.

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
[MenuEvents] tracking function=GetGameIndex outer=FalconGameplayStatics ...
[MenuEvents] installed global ProcessEvent hook ...
```

The log now reports menu-state transitions as well as selection changes:

```text
[MenuEvents] selector entered gameIndex=2 -> cue=3
cue=3 started ...
[MenuEvents] selector highlight 2 -> 1 -> cue=2
cue=2 started ...
[MenuEvents] selector exited -> root menu trilogy title cue=0
cue=0 started ...
```

During startup/profile/save loading, all voice output is suppressed until the selector is positively proven by a highlighted-game value that differs from the active loaded game. This hotfix intentionally favors silence over a false title cue before the first selector visit.

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
