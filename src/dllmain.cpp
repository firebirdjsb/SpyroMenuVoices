#include "menu_voice_api.hpp"
#include "resource.h"
#include <Windows.h>
#include <mmsystem.h>
#include <atomic>

namespace {
HMODULE module{};
std::atomic<int> lastCue{-1};
std::atomic<ULONGLONG> lastCueAt{};

int resourceForCue(int cue) noexcept {
    switch (cue) {
    case 0: return IDR_VOICE_REIGNITED;
    case 1: return IDR_VOICE_SPYRO1;
    case 2: return IDR_VOICE_SPYRO2;
    case 3: return IDR_VOICE_SPYRO3;
    default: return 0;
    }
}
}

extern "C" __declspec(dllexport) bool __cdecl SpyroMenuVoices_PlayCue(int cue) noexcept {
    const auto resource=resourceForCue(cue);
    if (!module||!resource) return false;

    const auto now=GetTickCount64();
    const auto previous=lastCue.load(std::memory_order_relaxed);
    const auto previousAt=lastCueAt.load(std::memory_order_relaxed);
    const auto debounce=cue==0?1000ull:750ull;
    if (cue==previous&&previousAt&&now-previousAt<debounce) return true;

    const bool played=PlaySoundW(MAKEINTRESOURCEW(resource),module,
        SND_ASYNC|SND_RESOURCE|SND_NODEFAULT)!=FALSE;
    if (played) {
        lastCue.store(cue,std::memory_order_relaxed);
        lastCueAt.store(now,std::memory_order_relaxed);
    }
    return played;
}

BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if (reason==DLL_PROCESS_ATTACH) {
        module=instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
