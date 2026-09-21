# Spyro Menu Voices

Native x64 audio resource module for Spyro Reignited Trilogy. It embeds the restored title and game-selection voice clips and exposes a small cue API for another ASI, such as Spyro Workshop or a title-menu hook, to call.

## Important: the module needs a caller

`SpyroMenuVoices.asi` is an audio provider, not a title-menu detector. Loading it by itself does not know when the Reignited title or one of the three game buttons becomes selected. The caller must resolve and invoke `SpyroMenuVoices_PlayCue` at the actual UI focus/activation event.

The module now writes `SpyroMenuVoices.log` beside the ASI. A successful startup contains `ready ... clips=4`, and each accepted cue contains `cue=N started`. If no cue lines appear, the title-menu caller is missing or is firing the event too late.

## Install

Copy `SpyroMenuVoices.asi` beside `Spyro-Win64-Shipping.exe` in `Falcon\Binaries\Win64`, together with the ASI that detects the title/game-selection UI events and calls this module.

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

The output is `build\Release\SpyroMenuVoices.asi`.

## Cue API

```cpp
using PlayCue = bool(__cdecl*)(int cue) noexcept;
using GetState = int(__cdecl*)() noexcept;
```

Exports:

- `SpyroMenuVoices_PlayCue(0)` — “Spyro Reignited Trilogy”
- `SpyroMenuVoices_PlayCue(1)` — “Spyro the Dragon”
- `SpyroMenuVoices_PlayCue(2)` — “Spyro 2: Ripto's Rage”
- `SpyroMenuVoices_PlayCue(3)` — “Spyro: Year of the Dragon”
- `SpyroMenuVoices_GetState()` — `0` not started, `1` initializing, `2` ready, `3` fallback-only/failed

Playback uses a preloaded, dedicated WinMM `waveOut` device rather than process-global `PlaySound`. New cues interrupt the previous cue immediately, and only duplicate events within 150 ms are ignored.
