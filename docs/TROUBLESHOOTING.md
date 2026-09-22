# Troubleshooting

Most problems with Spyro Menu Voices can be narrowed down quickly with `SpyroMenuVoices.log`.

The log is created in the same folder as the ASI.

## Nothing happens at all

First check that your ASI loader is actually loading the plugin.

The ASI belongs beside:

```text
Falcon\Binaries\Win64\Spyro-Win64-Shipping.exe
```

If `SpyroMenuVoices.log` is never created, the plugin probably was not loaded.

Things to check:

- the file is named `SpyroMenuVoices.asi`;
- it is beside the real shipping executable, not just the launcher;
- your ASI loader is installed and working;
- security software did not quarantine the ASI.

## The log exists, but the hook fails

Look for:

```text
unsupported executable timestamp=...
```

or:

```text
timed out locating GetGameIndex/UI_Title focus signal or hooking global ProcessEvent
```

v0.6.0 only enables the runtime hook on the executable build it was validated against.

That is a safety check. If the game has updated, the offsets need to be revalidated before support should be added.

Please include the whole log in a bug report.

## Audio engine says fallback-only

Normal initialization looks like:

```text
initializing low-latency embedded WAV engine
ready channels=2 rate=48000 bits=16 clips=4
```

If you see `waveOutOpen failed`, `waveOutPrepareHeader failed`, or repeated fallback messages, the dedicated WinMM output path was not available.

The module will attempt `PlaySoundW` as a fallback, but interruption timing may not feel as clean.

Check that Windows has a working default audio output device and restart the game after changing devices.

## Title line plays on the save/profile screen

That should not happen in v0.6.0.

The title cue is only supposed to come from the real `UI_Title_C_*` widget gaining focus.

If you can reproduce it, save the log and include the section from startup through the first title screen.

Do not delete the log until after copying it.

## Game names do not play on the selector

Look for lines such as:

```text
inactive GetGameIndex changed 2 -> 1 active=2 rapidSamples=1
selector proven method=divergence ...
selector entered gameIndex=1 -> cue=2
```

If `GetGameIndex` changes are visible but there is no selector entry, include that section of the log.

If no `GetGameIndex` lines appear at all, the game build or Falcon function layout may have changed.

## A game name is delayed

The low-latency path should begin a new cue immediately.

A normal line looks like:

```text
cue=2 started bytes=...
```

If the log line appears immediately but you hear the sound much later, note your Windows audio device, output format, and any virtual audio software in the bug report.

If the log itself appears late, the problem is on the menu-event side instead.

## The old “Spyro Spyro Spyro” loop

That was caused by an experimental startup timing heuristic in pre-v0.6.0 builds.

Upgrade to v0.6.0 or newer.

The current build does not use that startup/root candidate timer.

## The trilogy title plays between game selections

That was caused by an experimental 280 ms selector-silence watchdog in an older test build.

Upgrade to v0.6.0 or newer.

The current selector state stays latched until the actual title UI regains focus.

## Popup closes and title line plays again

v0.6.0 tracks whether the title line has already been announced for the current title visit.

If a normal popup steals focus and returns it to `UI_Title_C_*`, the extra title cue should be suppressed.

A healthy log can contain:

```text
root title refocused self=UI_Title_C_1 (cue already announced; suppressed)
```

## Making a useful bug report

Please include:

- Spyro Menu Voices version;
- where you got the game build from;
- whether another ASI/mod is installed;
- what you expected;
- what actually happened;
- the complete `SpyroMenuVoices.log` from that run.

For timing bugs, a short video plus the matching log is especially useful.
