#include "audio_engine.hpp"
#include "resource.h"

#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <mutex>
#include <string>
#include <vector>

namespace voice_audio {
namespace {

constexpr int kCueCount = 4;
constexpr ULONGLONG kDuplicateDebounceMs = 150;

enum class EngineState : int {
    NotStarted = 0,
    Initializing = 1,
    Ready = 2,
    FallbackOnly = 3,
};

struct Clip {
    std::vector<char> samples;
    WAVEHDR header{};
    int resourceId{};
};

struct Engine {
    HMODULE module{};
    HWAVEOUT output{};
    WAVEFORMATEX format{};
    std::array<Clip, kCueCount> clips{};
    std::mutex playbackMutex;
    std::mutex logMutex;
    std::atomic<EngineState> state{EngineState::NotStarted};
    std::atomic<int> pendingCue{-1};
    std::atomic<int> lastCue{-1};
    std::atomic<ULONGLONG> lastCueAt{};
    std::wstring logPath;
};

Engine& GetEngine() {
    static Engine engine;
    return engine;
}

int ResourceForCue(int cue) noexcept {
    switch (cue) {
    case 0: return IDR_VOICE_REIGNITED;
    case 1: return IDR_VOICE_SPYRO1;
    case 2: return IDR_VOICE_SPYRO2;
    case 3: return IDR_VOICE_SPYRO3;
    default: return 0;
    }
}

std::uint16_t ReadU16(const std::byte* data) noexcept {
    return static_cast<std::uint16_t>(std::to_integer<unsigned char>(data[0])) |
           (static_cast<std::uint16_t>(std::to_integer<unsigned char>(data[1])) << 8u);
}

std::uint32_t ReadU32(const std::byte* data) noexcept {
    return static_cast<std::uint32_t>(std::to_integer<unsigned char>(data[0])) |
           (static_cast<std::uint32_t>(std::to_integer<unsigned char>(data[1])) << 8u) |
           (static_cast<std::uint32_t>(std::to_integer<unsigned char>(data[2])) << 16u) |
           (static_cast<std::uint32_t>(std::to_integer<unsigned char>(data[3])) << 24u);
}

void SetLogPath(Engine& engine) noexcept {
    wchar_t modulePath[32768]{};
    const DWORD length = GetModuleFileNameW(engine.module, modulePath, static_cast<DWORD>(std::size(modulePath)));
    if (!length || length >= std::size(modulePath)) return;

    std::wstring path(modulePath, length);
    const auto slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) path.resize(slash + 1);
    else path.clear();
    path += L"SpyroMenuVoices.log";
    engine.logPath = std::move(path);
}

void Log(const char* level, const char* message) noexcept {
    auto& engine = GetEngine();
    if (engine.logPath.empty()) return;

    SYSTEMTIME now{};
    GetLocalTime(&now);
    char line[2048]{};
    const int length = std::snprintf(
        line,
        sizeof(line),
        "[%04u-%02u-%02u %02u:%02u:%02u.%03u] [%s] [thread=%lu] %s\r\n",
        now.wYear,
        now.wMonth,
        now.wDay,
        now.wHour,
        now.wMinute,
        now.wSecond,
        now.wMilliseconds,
        level,
        GetCurrentThreadId(),
        message);
    if (length <= 0) return;

    std::lock_guard lock(engine.logMutex);
    HANDLE file = CreateFileW(
        engine.logPath.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) return;

    DWORD written{};
    const DWORD bytes = static_cast<DWORD>(
        (std::min)(static_cast<std::size_t>(length), sizeof(line) - 1));
    WriteFile(file, line, bytes, &written, nullptr);
    CloseHandle(file);
}

bool ParseWaveResource(Engine& engine, int resourceId, Clip& clip, WAVEFORMATEX& parsedFormat) noexcept {
    HRSRC resource = FindResourceW(engine.module, MAKEINTRESOURCEW(resourceId), L"WAVE");
    if (!resource) resource = FindResourceW(engine.module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource) {
        char message[160]{};
        std::snprintf(message, sizeof(message), "resource %d was not found", resourceId);
        Log("ERROR", message);
        return false;
    }

    const DWORD resourceSize = SizeofResource(engine.module, resource);
    HGLOBAL loaded = LoadResource(engine.module, resource);
    const auto* bytes = static_cast<const std::byte*>(loaded ? LockResource(loaded) : nullptr);
    if (!bytes || resourceSize < 12) {
        char message[160]{};
        std::snprintf(message, sizeof(message), "resource %d could not be loaded", resourceId);
        Log("ERROR", message);
        return false;
    }

    if (std::memcmp(bytes, "RIFF", 4) != 0 || std::memcmp(bytes + 8, "WAVE", 4) != 0) {
        char message[160]{};
        std::snprintf(message, sizeof(message), "resource %d is not a RIFF/WAVE file", resourceId);
        Log("ERROR", message);
        return false;
    }

    bool haveFormat = false;
    bool haveData = false;
    std::size_t offset = 12;
    while (offset + 8 <= resourceSize) {
        const auto* chunk = bytes + offset;
        const std::uint32_t chunkSize = ReadU32(chunk + 4);
        const std::size_t dataOffset = offset + 8;
        if (dataOffset > resourceSize || chunkSize > resourceSize - dataOffset) {
            Log("ERROR", "WAV chunk extends beyond the embedded resource");
            return false;
        }

        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            if (chunkSize < 16) {
                Log("ERROR", "WAV fmt chunk is too small");
                return false;
            }
            const auto* format = bytes + dataOffset;
            parsedFormat = {};
            parsedFormat.wFormatTag = ReadU16(format + 0);
            parsedFormat.nChannels = ReadU16(format + 2);
            parsedFormat.nSamplesPerSec = ReadU32(format + 4);
            parsedFormat.nAvgBytesPerSec = ReadU32(format + 8);
            parsedFormat.nBlockAlign = ReadU16(format + 12);
            parsedFormat.wBitsPerSample = ReadU16(format + 14);
            parsedFormat.cbSize = chunkSize >= 18 ? ReadU16(format + 16) : 0;
            haveFormat = true;
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            const auto* sampleBegin = reinterpret_cast<const char*>(bytes + dataOffset);
            clip.samples.assign(sampleBegin, sampleBegin + chunkSize);
            haveData = !clip.samples.empty();
        }

        offset = dataOffset + chunkSize + (chunkSize & 1u);
    }

    if (!haveFormat || !haveData) {
        char message[160]{};
        std::snprintf(message, sizeof(message), "resource %d is missing fmt or data", resourceId);
        Log("ERROR", message);
        return false;
    }

    if (parsedFormat.wFormatTag != WAVE_FORMAT_PCM ||
        parsedFormat.nChannels == 0 ||
        parsedFormat.nSamplesPerSec == 0 ||
        parsedFormat.nBlockAlign == 0 ||
        parsedFormat.wBitsPerSample == 0) {
        char message[192]{};
        std::snprintf(
            message,
            sizeof(message),
            "resource %d has unsupported format tag=%u channels=%u rate=%lu bits=%u",
            resourceId,
            parsedFormat.wFormatTag,
            parsedFormat.nChannels,
            parsedFormat.nSamplesPerSec,
            parsedFormat.wBitsPerSample);
        Log("ERROR", message);
        return false;
    }

    clip.resourceId = resourceId;
    clip.header = {};
    clip.header.lpData = clip.samples.data();
    clip.header.dwBufferLength = static_cast<DWORD>(clip.samples.size());
    return true;
}

bool SameFormat(const WAVEFORMATEX& left, const WAVEFORMATEX& right) noexcept {
    return left.wFormatTag == right.wFormatTag &&
           left.nChannels == right.nChannels &&
           left.nSamplesPerSec == right.nSamplesPerSec &&
           left.nAvgBytesPerSec == right.nAvgBytesPerSec &&
           left.nBlockAlign == right.nBlockAlign &&
           left.wBitsPerSample == right.wBitsPerSample;
}

bool PlayFallback(int cue) noexcept {
    auto& engine = GetEngine();
    const int resource = ResourceForCue(cue);
    if (!engine.module || !resource) return false;

    const bool played = PlaySoundW(
        MAKEINTRESOURCEW(resource),
        engine.module,
        SND_ASYNC | SND_RESOURCE | SND_NODEFAULT) != FALSE;

    char message[160]{};
    std::snprintf(
        message,
        sizeof(message),
        "cue=%d fallback PlaySound result=%s win32=%lu",
        cue,
        played ? "ok" : "failed",
        GetLastError());
    Log(played ? "WARN" : "ERROR", message);
    return played;
}

bool PlayReady(int cue) noexcept {
    auto& engine = GetEngine();
    if (cue < 0 || cue >= kCueCount || !engine.output) return false;

    std::lock_guard lock(engine.playbackMutex);
    MMRESULT result = waveOutReset(engine.output);
    if (result != MMSYSERR_NOERROR) {
        char message[160]{};
        std::snprintf(message, sizeof(message), "waveOutReset failed result=%u cue=%d", result, cue);
        Log("ERROR", message);
        return PlayFallback(cue);
    }

    auto& clip = engine.clips[static_cast<std::size_t>(cue)];
    result = waveOutWrite(engine.output, &clip.header, sizeof(clip.header));
    if (result != MMSYSERR_NOERROR) {
        char message[160]{};
        std::snprintf(message, sizeof(message), "waveOutWrite failed result=%u cue=%d", result, cue);
        Log("ERROR", message);
        return PlayFallback(cue);
    }

    char message[160]{};
    std::snprintf(
        message,
        sizeof(message),
        "cue=%d started bytes=%lu",
        cue,
        clip.header.dwBufferLength);
    Log("INFO", message);
    return true;
}

void CloseOutput(Engine& engine) noexcept {
    if (!engine.output) return;

    waveOutReset(engine.output);
    for (auto& clip : engine.clips) {
        if (clip.header.dwFlags & WHDR_PREPARED) {
            waveOutUnprepareHeader(engine.output, &clip.header, sizeof(clip.header));
        }
    }
    waveOutClose(engine.output);
    engine.output = nullptr;
}

bool InitializeInternal(HMODULE module) noexcept {
    auto& engine = GetEngine();
    engine.module = module;
    SetLogPath(engine);
    Log("INFO", "initializing low-latency embedded WAV engine");

    WAVEFORMATEX reference{};
    for (int cue = 0; cue < kCueCount; ++cue) {
        WAVEFORMATEX current{};
        if (!ParseWaveResource(engine, ResourceForCue(cue), engine.clips[static_cast<std::size_t>(cue)], current)) {
            return false;
        }
        if (cue == 0) reference = current;
        else if (!SameFormat(reference, current)) {
            Log("ERROR", "embedded clips do not share one PCM format");
            return false;
        }
    }
    engine.format = reference;

    MMRESULT result = waveOutOpen(
        &engine.output,
        WAVE_MAPPER,
        &engine.format,
        0,
        0,
        CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
        char message[160]{};
        std::snprintf(message, sizeof(message), "waveOutOpen failed result=%u", result);
        Log("ERROR", message);
        engine.output = nullptr;
        return false;
    }

    for (auto& clip : engine.clips) {
        result = waveOutPrepareHeader(engine.output, &clip.header, sizeof(clip.header));
        if (result != MMSYSERR_NOERROR) {
            char message[160]{};
            std::snprintf(message, sizeof(message), "waveOutPrepareHeader failed result=%u resource=%d", result, clip.resourceId);
            Log("ERROR", message);
            CloseOutput(engine);
            return false;
        }
    }

    char message[192]{};
    std::snprintf(
        message,
        sizeof(message),
        "ready channels=%u rate=%lu bits=%u clips=%d",
        engine.format.nChannels,
        engine.format.nSamplesPerSec,
        engine.format.wBitsPerSample,
        kCueCount);
    Log("INFO", message);
    return true;
}

} // namespace

bool Initialize(HMODULE module) noexcept {
    if (!module) return false;

    auto& engine = GetEngine();
    EngineState expected = EngineState::NotStarted;
    if (!engine.state.compare_exchange_strong(expected, EngineState::Initializing)) {
        return engine.state.load() == EngineState::Ready;
    }

    const bool ready = InitializeInternal(module);
    engine.state.store(ready ? EngineState::Ready : EngineState::FallbackOnly);

    const int pending = engine.pendingCue.exchange(-1);
    if (pending >= 0) {
        if (ready) PlayReady(pending);
        else PlayFallback(pending);
    }
    return ready;
}

bool Play(int cue) noexcept {
    if (cue < 0 || cue >= kCueCount) {
        Log("ERROR", "invalid cue requested");
        return false;
    }

    auto& engine = GetEngine();
    const ULONGLONG now = GetTickCount64();
    const int previous = engine.lastCue.load(std::memory_order_relaxed);
    const ULONGLONG previousAt = engine.lastCueAt.load(std::memory_order_relaxed);
    if (cue == previous && previousAt && now - previousAt < kDuplicateDebounceMs) return true;

    engine.lastCue.store(cue, std::memory_order_relaxed);
    engine.lastCueAt.store(now, std::memory_order_relaxed);

    const EngineState state = engine.state.load(std::memory_order_acquire);
    if (state == EngineState::Ready) return PlayReady(cue);
    if (state == EngineState::FallbackOnly) return PlayFallback(cue);
    if (state == EngineState::Initializing) {
        engine.pendingCue.store(cue, std::memory_order_release);
        Log("INFO", "cue retained while audio device initializes");
        return true;
    }

    // The bootstrap thread normally initializes long before the title menu is
    // interactive. This synchronous path covers an unusual loader that delays
    // or blocks the bootstrap thread.
    if (Initialize(engine.module)) return PlayReady(cue);

    const EngineState afterInitialize = engine.state.load(std::memory_order_acquire);
    if (afterInitialize == EngineState::Ready) return PlayReady(cue);
    if (afterInitialize == EngineState::Initializing) {
        engine.pendingCue.store(cue, std::memory_order_release);
        return true;
    }
    return PlayFallback(cue);
}

int State() noexcept {
    return static_cast<int>(GetEngine().state.load(std::memory_order_acquire));
}

void Shutdown() noexcept {
    auto& engine = GetEngine();
    std::lock_guard lock(engine.playbackMutex);
    if (!engine.output) return;

    CloseOutput(engine);
    engine.state.store(EngineState::FallbackOnly, std::memory_order_release);
    Log("INFO", "audio engine shut down");
}

} // namespace voice_audio
