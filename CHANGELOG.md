# Changelog

## v0.6.0 — 2026-09-21

First fully runtime-validated release.

### Added

- Restored “Spyro Reignited Trilogy” title voice.
- Restored spoken names for all three trilogy game selections.
- Standalone global `ProcessEvent` hook; no Spyro Workshop dependency.
- Low-latency embedded WAV playback using WinMM `waveOut`.
- Immediate interruption when the user changes selection.
- Embedded audio resources; no loose WAV files required.
- Runtime logging to `SpyroMenuVoices.log`.
- Strict executable signature validation.
- Public cue/state exports for testing and integration.
- GitHub Actions x64 build.

### Runtime behavior fixes made during development

- Replaced passive/manual cue triggering with real menu events.
- Rejected `SetActiveGameIndex` and `SetGameIndex` as selector-hover signals after runtime testing.
- Switched selector tracking to the verified `GetGameIndex` return value.
- Added `GetActiveGameIndex` context so startup/profile restoration is not mistaken for selector navigation.
- Removed a startup timing heuristic that could repeatedly restart the trilogy title line on the save screen.
- Removed a 280 ms silence watchdog that could mistake normal pauses between selector inputs for leaving the menu.
- Replaced the generic `OnGameLoadCompleteCallback` diagnostic candidate with the real title UI focus event.
- Bound the title cue to `UserWidget.OnAddedToFocusPath` on `UI_Title_C_*`.
- Added title-visit state so popup focus changes do not repeatedly replay the title cue.

### Build

- C++20 / MSVC x64.
- CMake 3.24+.
- MinHook 1.3.4 fetched and hash-verified at configure time.
