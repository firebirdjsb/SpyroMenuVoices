#pragma once

namespace menu_voice_api {

// 0 = Reignited Trilogy title, 1 = Spyro the Dragon,
// 2 = Spyro 2: Ripto's Rage, 3 = Spyro: Year of the Dragon.
using PlayCue = bool(__cdecl*)(int cue) noexcept;

// 0 = not started, 1 = initializing, 2 = ready, 3 = fallback-only/failed.
using GetState = int(__cdecl*)() noexcept;

// 0 = not started, 1 = searching for SetActiveGameIndex,
// 2 = native menu hook installed, 3 = hook failed.
using GetHookState = int(__cdecl*)() noexcept;

} // namespace menu_voice_api
