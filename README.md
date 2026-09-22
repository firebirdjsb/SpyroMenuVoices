# Spyro Menu Voices

Spyro Menu Voices is a small native ASI for **Spyro Reignited Trilogy** that restores the unused menu voice lines for the trilogy title and the three game choices.

The goal is simple: make the menu behave like the old reference footage without adding a separate launcher, overlay, configuration menu, or dependency on another Spyro mod.

The current release is **v0.6.0**.

## What it restores

When the trilogy title screen appears, the game says:

> Spyro Reignited Trilogy

When the three-game selector is open, moving the highlight announces the game you are on:

- Spyro the Dragon
- Spyro 2: Ripto's Rage
- Spyro: Year of the Dragon

The lines interrupt each other immediately when you move to another game, which is how the original menu behavior sounds in the reference recording. Backing out of the selector returns to the title line once.

The profile/save-loading screen stays silent.

## Why this version is different

Getting the audio to play was the easy part. The awkward part was figuring out which game events actually mean “the title screen is visible” and “this game tile is highlighted.”

Several things that looked correct on paper turned out not to be useful in the real menu.

`SetActiveGameIndex` and `SetGameIndex` do not track normal selector navigation. The useful signal is `FalconGameplayStatics::GetGameIndex`: its return value follows the highlighted trilogy tile.

The title screen uses a different signal. Runtime tracing showed that the reliable root-menu event is `UserWidget::OnAddedToFocusPath` firing on the real `UI_Title_C_*` widget. v0.6.0 binds the trilogy-title line to that UI event instead of trying to guess menu state from delays or loading callbacks.

That matters because earlier test builds could mistake save-screen polling or a pause between inputs for a menu transition. v0.6.0 no longer uses those timing guesses.

## Install

You need the Windows x64 version of Spyro Reignited Trilogy and an ASI loader that loads plugins placed beside the game executable.

1. Download **SpyroMenuVoices.asi** from the latest GitHub Release.
2. Find the game executable:

```text
Falcon\Binaries\Win64\Spyro-Win64-Shipping.exe
```

3. Put `SpyroMenuVoices.asi` in the same folder as `Spyro-Win64-Shipping.exe`.
4. Start the game normally.

There are no loose WAV files to copy. All four voice clips are embedded directly in the ASI.

If your current mod setup already loads other `.asi` files from that folder, you do not need another loader for this mod.

## What you should hear

A normal startup should behave like this:

1. Save/profile loading happens with no restored voice audio.
2. The real trilogy title menu gains focus.
3. “Spyro Reignited Trilogy” plays once.
4. Open the game selector.
5. Moving between the three games immediately plays the matching game name.
6. Press Back.
7. The title menu regains focus and “Spyro Reignited Trilogy” plays once again.

Opening and closing an unrelated popup on the title screen does not keep replaying the title line.

## No Spyro Workshop dependency

This project is completely standalone.

It does **not** require Spyro Workshop, its frame callback, or its `ProcessEvent` bridge. The ASI installs and owns the small amount of runtime hooking it needs.

The only third-party code pulled in at build time is **MinHook 1.3.4**.

## Logs

The ASI creates:

```text
SpyroMenuVoices.log
```

beside the ASI.

A healthy startup includes lines similar to:

```text
ready channels=2 rate=48000 bits=16 clips=4
[MenuEvents] tracking function=GetGameIndex outer=FalconGameplayStatics ...
[MenuEvents] tracking root-title focus function=OnAddedToFocusPath outer=UserWidget ...
[MenuEvents] installed global ProcessEvent hook ...
```

Useful runtime lines look like:

```text
root title focused self=UI_Title_C_1 selectorWasActive=0 -> trilogy title cue=0
cue=0 started ...

selector entered gameIndex=2 -> cue=3
cue=3 started ...

selector highlight 2 -> 1 -> cue=2
cue=2 started ...

root title focused self=UI_Title_C_1 selectorWasActive=1 -> trilogy title cue=0
cue=0 started ...
```

If something does not behave correctly, keep that log. It is the first thing to check.

See [Troubleshooting](docs/TROUBLESHOOTING.md) for the common failure cases.

## Building from source

Requirements:

- Windows x64
- Visual Studio 2022 with the C++ desktop toolchain
- CMake 3.24 or newer
- Internet access during the first configure so CMake can fetch MinHook

Build:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

Output:

```text
build\Release\SpyroMenuVoices.asi
```

The project uses C++20 and the static MSVC runtime.

## Project layout

```text
assets/
    Embedded voice WAV files

include/
    Small exported API definitions

src/
    audio_engine.cpp       Low-latency embedded WAV playback
    dllmain.cpp            ASI bootstrap and exports
    menu_event_hook.cpp    Unreal/Falcon menu detection and ProcessEvent hook
    resources.rc           Embeds the four WAV files

docs/
    Technical notes, troubleshooting, and release notes

.github/workflows/
    CI build and release automation
```

For the full runtime explanation, offsets, state machine, and notes for updating the project for a different game executable, read [Technical Notes](docs/TECHNICAL.md).

## Exported API

The ASI exposes three simple exports:

```cpp
bool SpyroMenuVoices_PlayCue(int cue);
int  SpyroMenuVoices_GetState();
int  SpyroMenuVoices_GetHookState();
```

Cue IDs:

| Cue | Voice |
| --- | --- |
| 0 | Spyro Reignited Trilogy |
| 1 | Spyro the Dragon |
| 2 | Spyro 2: Ripto's Rage |
| 3 | Spyro: Year of the Dragon |

The exports are mainly useful for debugging or for another local tool that wants to test the embedded audio engine. Normal users do not need them.

## Supported executable

v0.6.0 is deliberately strict about the executable it hooks. It checks the known PE timestamp and image size before using hard-coded runtime addresses.

If those values do not match, the hook fails instead of blindly writing into an unknown game build.

Details are in [Technical Notes](docs/TECHNICAL.md).

## Contributing

Bug reports with a `SpyroMenuVoices.log` are useful.

If you want to change the runtime hooks, add support for another executable build, or work on the audio path, read [CONTRIBUTING.md](CONTRIBUTING.md) first. It explains the pieces that are intentionally conservative and the tests worth doing before opening a pull request.

## License and game assets

The source code in this repository is released under the [MIT License](LICENSE).

The Spyro names, game, trademarks, and original voice recordings belong to their respective rights holders. The MIT license for this repository does not relicense third-party game assets.

This is an unofficial fan project and is not affiliated with or endorsed by Activision, Toys for Bob, Insomniac Games, or the other rights holders involved with Spyro.
