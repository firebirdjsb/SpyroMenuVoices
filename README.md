# Spyro Menu Voices

Native x64 audio resource module for Spyro Reignited Trilogy. It embeds the supplied restored title and game-selection voice clips and exposes a small, stable cue API to [Spyro Workshop](https://github.com/firebirdjsb/spyro-workshop).

## Install

Copy `SpyroMenuVoices.asi` beside `Spyro-Win64-Shipping.exe` in `Falcon\Binaries\Win64`. Spyro Workshop detects the module at runtime and calls `SpyroMenuVoices_PlayCue` for the title and the three game-selection buttons.

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

The output is `build\Release\SpyroMenuVoices.asi`. Loading the ASI alone is safe; without Spyro Workshop it remains idle.

## Cue API

`SpyroMenuVoices_PlayCue(int cue)` accepts `0` for the Reignited title and `1` through `3` for Spyro 1, Spyro 2, and Spyro 3. Playback is asynchronous and duplicate UI focus events are debounced.
