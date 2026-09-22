#pragma once

#include <Windows.h>

namespace menu_event_hook {

// Validates the known executable, resolves the small set of Falcon/UserWidget
// UFunctions used for menu state, and installs the filtered global ProcessEvent
// hook. Safe to call more than once; only the first caller performs setup.
bool Install(HMODULE module) noexcept;

// 0 = not started, 1 = searching, 2 = installed, 3 = failed.
int State() noexcept;

} // namespace menu_event_hook
