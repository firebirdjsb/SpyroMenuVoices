# Technical Notes

This document is the developer-side explanation of Spyro Menu Voices v0.6.0.

It covers the pieces that took the most runtime testing to get right: menu detection, Unreal object discovery, audio playback, executable validation, and why some earlier approaches were removed.

## Design goals

The project intentionally stays small.

It should:

- work as a standalone x64 ASI;
- not depend on Spyro Workshop or another mod's hook;
- avoid doing expensive Unreal name work every frame;
- react immediately when the highlighted game changes;
- keep the save/profile screen silent;
- interrupt the previous voice line when a new selection is made;
- fail safely on an unknown executable instead of applying known offsets blindly;
- leave enough logging behind to diagnose a broken game update.

There is no ImGui UI, configuration system, background polling loop, or per-frame feature manager.

## Startup

`DllMain` does almost nothing directly.

On `DLL_PROCESS_ATTACH` it:

1. disables thread attach/detach notifications;
2. creates a bootstrap thread;
3. returns to the loader.

The bootstrap thread initializes the audio engine first, then installs the menu hook.

This keeps loader-lock work to a minimum and gives the menu hook time to wait for the game's Unreal object array to become usable.

## Executable validation

Before the runtime addresses are used, the ASI validates the main executable.

v0.6.0 expects:

```text
PE timestamp: 1558415778
Image size:   61046784
```

Known RVAs:

```text
GObjects:            0x03562340
FName::AppendString: 0x005E4C60
ProcessEvent:        0x007688B0
```

The validated `ProcessEvent` vtable index used while researching the game is `0x3F`, although the production implementation hooks the known global `ProcessEvent` address with MinHook.

If the PE timestamp or image size differs, the module refuses to install the menu hook.

That is intentional. A game update should require a new validation pass rather than silently reusing stale offsets.

## Unreal object discovery

The project uses the game's `GObjects` array to locate a very small set of UFunction objects by name.

The important functions are:

- `FalconGameplayStatics.GetGameIndex`
- `FalconGameplayStatics.GetActiveGameIndex`
- `FalconGameplayStatics.SetGameIndex` (diagnostic only)
- `FalconGameplayStatics.SetActiveGameIndex` (diagnostic only)
- `UserWidget.OnAddedToFocusPath`

Name conversion uses the known `FName::AppendString` address.

The object search waits for a plausible object array and retries for up to roughly 30 seconds:

```text
120 attempts × 250 ms
```

Once the target UFunction pointers have been found, the hot path uses pointer comparisons. It does not repeatedly scan object names during normal menu navigation.

## ProcessEvent hook

MinHook 1.3.4 is used to detour the validated global `ProcessEvent` implementation.

The proxy compares the incoming UFunction pointer against the handful of discovered target pointers. Unrelated events are immediately forwarded to the original function.

This is important for overhead: the hook sees a very busy Unreal function, but the normal path is only a few pointer comparisons and a trampoline call.

The project originally experimented with a per-object/CDO vtable bridge. That was abandoned because the relevant menu calls were not arriving through that object in a useful way. The production code uses the validated global hook and exact function-pointer filtering instead.

## How game selection is detected

The early assumption was that one of the Falcon setters would change when the user moved between the three game tiles.

Runtime tracing proved otherwise.

`SetGameIndex` and `SetActiveGameIndex` are not the normal hover/navigation signal.

The useful function is:

```text
FalconGameplayStatics.GetGameIndex
```

After the original `ProcessEvent` call returns, the ASI reads the integer return value from the verified parameter layout.

Mapping:

```text
0 -> Spyro the Dragon
1 -> Spyro 2: Ripto's Rage
2 -> Spyro: Year of the Dragon
```

### Why GetActiveGameIndex is still tracked

`GetActiveGameIndex` describes the broader active/current game context.

During startup/profile loading, `GetGameIndex` can change because the game is restoring the previously used title. In that phase the active game and game index move together.

On the real selector, the highlighted `GetGameIndex` can move independently from `GetActiveGameIndex`.

That divergence became a strong proof that the user is actually navigating the trilogy selector.

There is also a rapid-call fallback for the case where the selector opens while already highlighting the current active game.

Once the selector is proven, its state stays latched. Normal pauses between user inputs are not treated as selector exits.

This was an important fix: an earlier prototype used a 280 ms silence timeout and wrongly announced the trilogy title between normal game-selection inputs.

## How the title screen is detected

The title line is not inferred from `GetGameIndex`.

Expanded runtime tracing showed a reliable UI event:

```text
UserWidget.OnAddedToFocusPath
```

When that event fires on an object whose name begins with:

```text
UI_Title_C
```

the real trilogy title UI has gained focus.

v0.6.0 uses that directly.

### Title announcement state

The project keeps a small amount of state to avoid duplicate title audio.

On the first real title focus:

- cue 0 plays;
- the title visit is marked announced.

When the game selector is entered:

- selector state becomes active;
- the title announcement is re-armed.

When `UI_Title_C_*` gains focus after that:

- selector state is cleared;
- cue 0 plays once.

If a separate confirmation/question popup temporarily steals focus and later returns focus to the same title screen:

- the title was already announced for that visit;
- cue 0 is suppressed.

This is much safer than the earlier timing or load-callback approaches.

## Save/profile screen behavior

The save/profile screen is intentionally silent.

During that phase the game may call the Falcon game-index getters at intervals and restore the last active title. Those calls are logged for diagnosis but are not enough to trigger a voice.

There is no “wait N milliseconds and assume title screen” timer in v0.6.0.

There is also no “GetGameIndex stopped being called, therefore the selector closed” timer.

Both ideas were tested and removed because the real runtime behavior disproved them.

## Audio engine

All four WAV files are Windows resources compiled into the ASI.

The runtime audio engine uses WinMM `waveOut` rather than starting a new high-level sound request for every cue.

Initialization:

1. locate each embedded WAVE resource;
2. parse the RIFF chunks;
3. verify PCM format;
4. verify all four clips use the same format;
5. copy sample data into persistent buffers;
6. open one `waveOut` device;
7. prepare all four `WAVEHDR` structures once.

The current embedded files are:

```text
PCM
2 channels
48000 Hz
16-bit
```

### Immediate interruption

When a new cue starts, the engine calls:

```cpp
waveOutReset(...)
waveOutWrite(...)
```

This stops the previous line immediately and starts the new one.

That is what makes quick movement across the game selector sound natural instead of queueing old game names behind the new selection.

### Duplicate suppression

The audio engine ignores the same cue if it is requested again within 150 ms.

That is only there to remove duplicate event chatter. It is intentionally short enough that moving away from a game and back to it can replay the voice normally.

### Fallback

If the dedicated `waveOut` path cannot initialize or a write fails, the module falls back to:

```text
PlaySoundW(SND_ASYNC | SND_RESOURCE | SND_NODEFAULT)
```

The fallback is logged.

## Embedded cue table

```text
Cue 0  spyro_vo_reignited.wav
Cue 1  spyro_vo_spyro1.wav
Cue 2  spyro_vo_spyro2_US.wav
Cue 3  spyro_vo_spyro3.wav
```

The resource compiler embeds them through `src/resources.rc`.

No external WAV path is used at runtime.

## Threading

The module uses:

- one bootstrap thread created at attach time;
- WinMM's output device for playback;
- atomics for small hook/audio state;
- mutexes around audio output operations and log writes.

There is no permanent worker thread polling menu state in v0.6.0.

Older test builds had a watchdog thread for timing-based selector exit detection. That was removed with the timing heuristic.

## Logging

Both the audio engine and menu hook write to:

```text
SpyroMenuVoices.log
```

beside the ASI.

The file is opened for append for each log write and shared for read/write so it can be inspected while the game is running.

Useful categories include:

- audio initialization;
- embedded WAV parsing errors;
- hook installation;
- discovered Falcon functions;
- active/index state changes;
- selector entry and highlight movement;
- root title focus;
- fallback audio use.

## Exported API

The ASI exports:

```cpp
bool __cdecl SpyroMenuVoices_PlayCue(int cue) noexcept;
int  __cdecl SpyroMenuVoices_GetState() noexcept;
int  __cdecl SpyroMenuVoices_GetHookState() noexcept;
```

Audio state:

```text
0 = not started
1 = initializing
2 = ready
3 = fallback only / initialization failed
```

Hook state:

```text
0 = not started
1 = searching
2 = installed
3 = failed
```

## Build configuration

The project requires:

- Windows
- x64
- MSVC
- C++20
- CMake 3.24+

CMake fetches MinHook v1.3.4 and verifies its archive SHA-256.

The MSVC runtime is linked statically.

Release output:

```text
build\Release\SpyroMenuVoices.asi
```

## CI

`.github/workflows/build.yml` builds the project on `windows-2025` for every push and pull request.

The workflow:

1. checks out the repository;
2. configures an x64 CMake build;
3. builds Release;
4. uploads `SpyroMenuVoices.asi` as an Actions artifact.

The release workflow builds the default branch again before publishing a release, creates a SHA-256 checksum, and attaches both files to the GitHub Release.

## Updating for another game build

Do not start by changing one offset until the old build “sort of works.”

For a new executable:

1. identify the new PE timestamp and image size;
2. revalidate `GObjects`;
3. revalidate `FName::AppendString`;
4. revalidate global `ProcessEvent`;
5. make sure the expected UObject/FName layouts still match;
6. confirm `GetGameIndex` and `GetActiveGameIndex` still resolve under `FalconGameplayStatics`;
7. confirm `OnAddedToFocusPath` still resolves;
8. verify the root title object is still named `UI_Title_C_*`;
9. run the complete menu test before changing the executable whitelist.

The complete menu test should include:

- startup/profile loading with no restored voice;
- initial title focus;
- opening the selector;
- moving 3 -> 2 -> 1 -> 3 with pauses between inputs;
- moving away and back to the same game;
- backing out to title;
- opening and closing a popup on the title screen;
- starting a game to make sure the title cue is not incorrectly triggered during gameplay loading.

## What not to reintroduce

A few approaches have already been disproved by runtime testing:

- treating `SetActiveGameIndex` as the selector-hover event;
- treating `SetGameIndex` as the selector-hover event;
- playing cue 0 on the first `GetGameIndex` observation;
- deciding the save screen has ended based on sparse getter timing;
- deciding the selector has closed because `GetGameIndex` was quiet for a few hundred milliseconds;
- binding cue 0 to the generic `OnGameLoadCompleteCallback`.

If one of these ideas is revisited for a different game build, validate it against a real runtime trace first.
