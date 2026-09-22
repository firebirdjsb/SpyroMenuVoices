# Contributing

Thanks for taking a look at the project.

Spyro Menu Voices is small on purpose, so changes are easiest to review when they keep the same basic shape: one audio module, one menu-event module, and very little work on the hot `ProcessEvent` path.

## Before changing the runtime hook

Read [docs/TECHNICAL.md](docs/TECHNICAL.md).

A few menu signals that looked obvious have already been tested and ruled out. In particular, the Falcon setters are not normal trilogy-selector navigation events, and timing gaps between getter calls are not reliable menu-state transitions.

If you are changing menu detection, please test against real runtime logs instead of assuming an Unreal function name means what the menu is doing.

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

The build should be clean with `/W4`.

## Runtime test checklist

Before opening a pull request that changes menu logic, test all of these:

1. Start the game and let the profile/save stage finish. No restored voice should play there.
2. Reach the trilogy title screen. The trilogy title line should play once.
3. Enter the three-game selector.
4. Move across all three games.
5. Pause for several seconds between two selections and move again.
6. Move away from a game and back to it. Its line should replay.
7. Back out of the selector. The trilogy title line should play once.
8. Open and close a popup on the title screen. The title line should not keep replaying.
9. Start a game and make sure title audio is not triggered during the loading transition.

Please keep the log from the test.

## Performance

Global `ProcessEvent` is busy.

Avoid:

- name lookup on every event;
- object-array scans after installation;
- logging every ProcessEvent call;
- allocations on the normal unrelated-event path;
- periodic menu-state polling if an exact event can be used instead.

The production hook should mostly be pointer comparisons.

## New game builds

If a Spyro update changes the executable, do not simply remove the executable validation.

Revalidate the offsets and menu signals first. The known values and update checklist are in [docs/TECHNICAL.md](docs/TECHNICAL.md).

When adding another supported executable, document how it was identified.

## Audio changes

All four clips are expected to share one PCM format because the audio engine opens one output device and prepares the headers once.

If an audio asset is replaced:

- keep the intended cue mapping;
- make sure all clips still share the same format;
- verify quick selector movement interrupts the old line cleanly;
- do not add a fade or queue that makes the spoken game name feel late.

## Pull requests

A good pull request includes:

- a short explanation of the problem;
- what signal or code path was changed;
- the build result;
- a small runtime log excerpt showing the new behavior;
- any known limitation.

Small focused changes are preferred over large rewrites.

## Style

The project currently uses C++20, simple free functions/modules, atomics for small state, and explicit validation around game memory.

Please match the surrounding code rather than introducing a framework for a small change.

## Third-party assets

The repository's MIT license covers the project source code.

Do not assume that game audio, trademarks, or other extracted game assets are covered by the MIT license.
