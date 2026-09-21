#pragma once

#include <Windows.h>

namespace voice_audio {

// Starts resource decoding and opens a dedicated WinMM output device. Safe to
// call more than once; the first caller performs initialization.
bool Initialize(HMODULE module) noexcept;

// Plays cue 0..3. While initialization is still in flight, the newest cue is
// retained and played as soon as the output device is ready.
bool Play(int cue) noexcept;

// 0 = not started, 1 = initializing, 2 = ready, 3 = fallback-only/failed.
int State() noexcept;

// Stops playback and releases WinMM resources. Intended for explicit unload.
void Shutdown() noexcept;

} // namespace voice_audio
