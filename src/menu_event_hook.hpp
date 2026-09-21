#pragma once

#include <Windows.h>

namespace menu_event_hook {

// Installs a per-object ProcessEvent vtable bridge on the Falcon blueprint
// library that owns SetActiveGameIndex. This avoids colliding with other ASIs
// that may already detour the global ProcessEvent implementation.
bool Install(HMODULE module) noexcept;

// 0 = not started, 1 = searching, 2 = installed, 3 = failed.
int State() noexcept;

} // namespace menu_event_hook
