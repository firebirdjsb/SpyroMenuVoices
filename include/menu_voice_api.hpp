#pragma once
#include <cstdint>

namespace menu_voice_api {
using Address=std::uintptr_t;
using Event=void(__cdecl*)(Address imageBase,Address object,Address function,
    void* parameters) noexcept;
// 0=Reignited title, 1=Spyro 1, 2=Spyro 2, 3=Spyro 3.
using PlayCue=bool(__cdecl*)(int cue) noexcept;
}
