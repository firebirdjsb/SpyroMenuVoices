#include "audio_engine.hpp"
#include "menu_event_hook.hpp"
#include "menu_voice_api.hpp"

#include <Windows.h>

namespace {
DWORD WINAPI AudioBootstrap(void* parameter) noexcept {
    const auto module = static_cast<HMODULE>(parameter);
    voice_audio::Initialize(module);
    menu_event_hook::Install(module);
    return 0;
}
}

extern "C" __declspec(dllexport) bool __cdecl SpyroMenuVoices_PlayCue(int cue) noexcept {
    return voice_audio::Play(cue);
}

extern "C" __declspec(dllexport) int __cdecl SpyroMenuVoices_GetState() noexcept {
    return voice_audio::State();
}

extern "C" __declspec(dllexport) int __cdecl SpyroMenuVoices_GetHookState() noexcept {
    return menu_event_hook::State();
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        if (HANDLE thread = CreateThread(nullptr, 0, AudioBootstrap, instance, 0, nullptr)) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
