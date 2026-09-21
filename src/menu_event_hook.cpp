#include "menu_event_hook.hpp"
#include "audio_engine.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace menu_event_hook {
namespace {

constexpr std::uintptr_t kGObjectsOffset = 0x03562340;
constexpr std::uintptr_t kAppendStringOffset = 0x005E4C60;
constexpr std::uint32_t kExpectedTimestamp = 1558415778;
constexpr std::uint32_t kExpectedImageSize = 61046784;
constexpr std::size_t kUObjectNameOffset = 0x18;
constexpr std::size_t kUObjectOuterOffset = 0x20;
constexpr std::size_t kUClassDefaultObjectOffset = 0xF8;
constexpr std::size_t kProcessEventIndex = 0x3F;
constexpr std::size_t kVtableSlotsToClone = 128;
constexpr int kSearchAttempts = 120;
constexpr DWORD kSearchDelayMs = 250;

enum class HookState : int {
    NotStarted = 0,
    Searching = 1,
    Installed = 2,
    Failed = 3,
};

struct FNameRaw {
    std::int32_t comparisonIndex{};
    std::int32_t number{};
};
static_assert(sizeof(FNameRaw) == 0x8);

template <typename T>
struct TArrayRaw {
    T* data{};
    std::int32_t num{};
    std::int32_t max{};
};
static_assert(sizeof(TArrayRaw<void*>) == 0x10);

using FStringRaw = TArrayRaw<wchar_t>;

struct FUObjectItemRaw {
    void* object{};
    std::uint8_t pad[0x10]{};
};
static_assert(sizeof(FUObjectItemRaw) == 0x18);

struct TUObjectArrayRaw {
    FUObjectItemRaw* objects{};
    std::int32_t maxElements{};
    std::int32_t numElements{};
};
static_assert(sizeof(TUObjectArrayRaw) == 0x10);

struct SetActiveGameIndexParams {
    const void* worldContextObject{};
    std::int32_t index{};
    std::uint8_t pad[0x4]{};
};
static_assert(sizeof(SetActiveGameIndexParams) == 0x10);

using AppendStringFn = void(*)(const FNameRaw*, FStringRaw&);
using ProcessEventFn = void(__fastcall*)(void*, void*, void*);

struct HookContext {
    HMODULE module{};
    std::uintptr_t imageBase{};
    TUObjectArrayRaw* objects{};
    AppendStringFn appendString{};
    void* targetFunction{};
    void* targetClass{};
    void* targetDefaultObject{};
    void** originalVtable{};
    void** clonedVtable{};
    ProcessEventFn nextProcessEvent{};
    std::wstring logPath;
    std::mutex logMutex;
    std::atomic<HookState> state{HookState::NotStarted};
};

HookContext& Context() {
    static HookContext context;
    return context;
}

bool RegionHasAccess(DWORD protect) noexcept {
    if ((protect & PAGE_GUARD) != 0 || (protect & PAGE_NOACCESS) != 0) return false;
    const DWORD base = protect & 0xFF;
    return base == PAGE_READONLY || base == PAGE_READWRITE || base == PAGE_WRITECOPY ||
           base == PAGE_EXECUTE || base == PAGE_EXECUTE_READ ||
           base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

bool IsReadable(const void* pointer, std::size_t bytes = 1) noexcept {
    if (!pointer || bytes == 0) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(pointer, &info, sizeof(info)) == 0 ||
        info.State != MEM_COMMIT || !RegionHasAccess(info.Protect)) return false;
    const auto start = reinterpret_cast<std::uintptr_t>(pointer);
    const auto regionStart = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    const auto regionEnd = regionStart + info.RegionSize;
    return start >= regionStart && start + bytes >= start && start + bytes <= regionEnd;
}

bool IsExecutable(const void* pointer) noexcept {
    if (!pointer) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(pointer, &info, sizeof(info)) == 0 || info.State != MEM_COMMIT) return false;
    if ((info.Protect & PAGE_GUARD) != 0 || (info.Protect & PAGE_NOACCESS) != 0) return false;
    const DWORD base = info.Protect & 0xFF;
    return base == PAGE_EXECUTE || base == PAGE_EXECUTE_READ ||
           base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

void SetLogPath(HMODULE module) noexcept {
    auto& context = Context();
    wchar_t modulePath[32768]{};
    const DWORD length = GetModuleFileNameW(module, modulePath, static_cast<DWORD>(std::size(modulePath)));
    if (!length || length >= std::size(modulePath)) return;
    context.logPath.assign(modulePath, length);
    const auto slash = context.logPath.find_last_of(L"\\/");
    if (slash != std::wstring::npos) context.logPath.resize(slash + 1);
    else context.logPath.clear();
    context.logPath += L"SpyroMenuVoices.log";
}

void Log(const char* level, const char* message) noexcept {
    auto& context = Context();
    if (context.logPath.empty()) return;

    SYSTEMTIME now{};
    GetLocalTime(&now);
    char line[2048]{};
    const int length = std::snprintf(
        line,
        sizeof(line),
        "[%04u-%02u-%02u %02u:%02u:%02u.%03u] [%s] [MenuEvents] %s\r\n",
        now.wYear,
        now.wMonth,
        now.wDay,
        now.wHour,
        now.wMinute,
        now.wSecond,
        now.wMilliseconds,
        level,
        message);
    if (length <= 0) return;

    std::lock_guard lock(context.logMutex);
    HANDLE file = CreateFileW(
        context.logPath.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written{};
    const DWORD count = static_cast<DWORD>((std::min)(static_cast<std::size_t>(length), sizeof(line) - 1));
    WriteFile(file, line, count, &written, nullptr);
    CloseHandle(file);
}

std::string Normalize(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const unsigned char character : value) {
        if (std::isalnum(character)) normalized.push_back(static_cast<char>(std::tolower(character)));
    }
    return normalized;
}

std::string ObjectName(void* object) noexcept {
    auto& context = Context();
    if (!context.appendString || !IsReadable(object, kUObjectNameOffset + sizeof(FNameRaw))) return {};

    std::array<wchar_t, 1024> buffer{};
    FStringRaw string{buffer.data(), 0, static_cast<std::int32_t>(buffer.size())};
    const auto* name = reinterpret_cast<const FNameRaw*>(
        static_cast<const std::uint8_t*>(object) + kUObjectNameOffset);

#if defined(_MSC_VER)
    __try {
#endif
        context.appendString(name, string);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return {};
    }
#endif

    if (string.data != buffer.data() || string.num <= 0 || string.num > static_cast<std::int32_t>(buffer.size())) return {};
    const int sourceLength = string.data[string.num - 1] == L'\0' ? string.num - 1 : string.num;
    if (sourceLength <= 0) return {};

    const int required = WideCharToMultiByte(CP_UTF8, 0, string.data, sourceLength, nullptr, 0, nullptr, nullptr);
    if (required <= 0) return {};
    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, string.data, sourceLength, result.data(), required, nullptr, nullptr);
    return result;
}

bool ValidateExecutable() noexcept {
    auto& context = Context();
    auto* executable = GetModuleHandleW(nullptr);
    if (!executable) {
        Log("ERROR", "GetModuleHandleW(nullptr) failed");
        return false;
    }

    context.imageBase = reinterpret_cast<std::uintptr_t>(executable);
    if (!IsReadable(executable, sizeof(IMAGE_DOS_HEADER))) {
        Log("ERROR", "executable DOS header is unreadable");
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(executable);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        Log("ERROR", "executable DOS signature mismatch");
        return false;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(context.imageBase + dos->e_lfanew);
    if (!IsReadable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) {
        Log("ERROR", "executable NT headers are invalid");
        return false;
    }
    if (nt->FileHeader.TimeDateStamp != kExpectedTimestamp ||
        nt->OptionalHeader.SizeOfImage != kExpectedImageSize) {
        char message[256]{};
        std::snprintf(
            message,
            sizeof(message),
            "unsupported executable timestamp=%u imageSize=%u",
            nt->FileHeader.TimeDateStamp,
            nt->OptionalHeader.SizeOfImage);
        Log("ERROR", message);
        return false;
    }

    context.objects = reinterpret_cast<TUObjectArrayRaw*>(context.imageBase + kGObjectsOffset);
    context.appendString = reinterpret_cast<AppendStringFn>(context.imageBase + kAppendStringOffset);
    return IsExecutable(reinterpret_cast<void*>(context.appendString));
}

bool ObjectArrayReady() noexcept {
    auto& context = Context();
    if (!IsReadable(context.objects, sizeof(TUObjectArrayRaw))) return false;
    const auto snapshot = *context.objects;
    if (!snapshot.objects || snapshot.numElements < 1000 || snapshot.numElements > 4000000 ||
        snapshot.maxElements < snapshot.numElements || snapshot.maxElements > 4000000) return false;
    return IsReadable(snapshot.objects, static_cast<std::size_t>(snapshot.numElements) * sizeof(FUObjectItemRaw));
}

bool FindSetActiveGameIndex() noexcept {
    auto& context = Context();
    if (!ObjectArrayReady()) return false;

    const auto snapshot = *context.objects;
    std::vector<std::string> candidates;
    candidates.reserve(16);

    for (std::int32_t index = 0; index < snapshot.numElements; ++index) {
        void* object = snapshot.objects[index].object;
        if (!object) continue;
        const std::string name = ObjectName(object);
        if (name.empty()) continue;
        const std::string normalized = Normalize(name);
        if (normalized.find("gameindex") != std::string::npos && candidates.size() < 16) {
            candidates.push_back(name);
        }
        if (normalized != "setactivegameindex") continue;

        if (!IsReadable(object, kUObjectOuterOffset + sizeof(void*))) continue;
        void* outer = *reinterpret_cast<void**>(static_cast<std::uint8_t*>(object) + kUObjectOuterOffset);
        if (!IsReadable(outer, kUClassDefaultObjectOffset + sizeof(void*))) continue;
        void* defaultObject = *reinterpret_cast<void**>(static_cast<std::uint8_t*>(outer) + kUClassDefaultObjectOffset);
        if (!IsReadable(defaultObject, sizeof(void*))) continue;

        context.targetFunction = object;
        context.targetClass = outer;
        context.targetDefaultObject = defaultObject;

        const std::string outerName = ObjectName(outer);
        char message[512]{};
        std::snprintf(
            message,
            sizeof(message),
            "found function=%s outer=%s function=%p defaultObject=%p",
            name.c_str(),
            outerName.empty() ? "<unknown>" : outerName.c_str(),
            object,
            defaultObject);
        Log("INFO", message);
        return true;
    }

    if (!candidates.empty()) {
        std::string message = "SetActiveGameIndex not found; candidates=";
        for (const auto& candidate : candidates) {
            if (message.size() > 1500) break;
            message += candidate;
            message += ',';
        }
        Log("WARN", message.c_str());
    }
    return false;
}

int CueForGameIndex(std::int32_t index) noexcept {
    if (index >= 0 && index <= 2) return index + 1;
    // The trilogy title/front-end uses the invalid active-game state when the
    // player backs out of the three-game chooser.
    return 0;
}

void __fastcall ProcessEventProxy(void* self, void* function, void* parameters) noexcept {
    auto& context = Context();
    const ProcessEventFn next = context.nextProcessEvent;
    if (!next) return;

    const bool isSelectionEvent = function == context.targetFunction;
    std::int32_t gameIndex = -999;
    if (isSelectionEvent && IsReadable(parameters, sizeof(SetActiveGameIndexParams))) {
        gameIndex = static_cast<SetActiveGameIndexParams*>(parameters)->index;
    }

    next(self, function, parameters);

    if (!isSelectionEvent || gameIndex == -999) return;
    const int cue = CueForGameIndex(gameIndex);
    char message[192]{};
    std::snprintf(message, sizeof(message), "SetActiveGameIndex index=%d -> cue=%d", gameIndex, cue);
    Log("INFO", message);
    voice_audio::Play(cue);
}

bool InstallVtableBridge() noexcept {
    auto& context = Context();
    if (!context.targetDefaultObject || !IsReadable(context.targetDefaultObject, sizeof(void*))) return false;

    auto*** objectVtablePointer = reinterpret_cast<void***>(context.targetDefaultObject);
    void** original = *objectVtablePointer;
    if (!IsReadable(original, kVtableSlotsToClone * sizeof(void*))) {
        Log("ERROR", "target default-object vtable is unreadable");
        return false;
    }
    if (!IsExecutable(original[kProcessEventIndex])) {
        Log("ERROR", "target ProcessEvent vtable slot is not executable");
        return false;
    }

    auto** clone = static_cast<void**>(VirtualAlloc(
        nullptr,
        kVtableSlotsToClone * sizeof(void*),
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE));
    if (!clone) {
        Log("ERROR", "VirtualAlloc failed for cloned vtable");
        return false;
    }
    std::memcpy(clone, original, kVtableSlotsToClone * sizeof(void*));

    context.originalVtable = original;
    context.clonedVtable = clone;
    context.nextProcessEvent = reinterpret_cast<ProcessEventFn>(original[kProcessEventIndex]);
    clone[kProcessEventIndex] = reinterpret_cast<void*>(&ProcessEventProxy);

    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(objectVtablePointer), clone);
    if (*objectVtablePointer != clone) {
        Log("ERROR", "failed to assign cloned default-object vtable");
        VirtualFree(clone, 0, MEM_RELEASE);
        context.clonedVtable = nullptr;
        context.nextProcessEvent = nullptr;
        return false;
    }

    char message[256]{};
    std::snprintf(
        message,
        sizeof(message),
        "installed per-object ProcessEvent bridge object=%p next=%p",
        context.targetDefaultObject,
        reinterpret_cast<void*>(context.nextProcessEvent));
    Log("INFO", message);
    return true;
}

} // namespace

bool Install(HMODULE module) noexcept {
    if (!module) return false;
    auto& context = Context();
    HookState expected = HookState::NotStarted;
    if (!context.state.compare_exchange_strong(expected, HookState::Searching)) {
        return context.state.load() == HookState::Installed;
    }

    context.module = module;
    SetLogPath(module);
    Log("INFO", "searching for native Falcon menu-selection event");

    if (!ValidateExecutable()) {
        context.state.store(HookState::Failed);
        return false;
    }

    for (int attempt = 0; attempt < kSearchAttempts; ++attempt) {
        if (FindSetActiveGameIndex() && InstallVtableBridge()) {
            context.state.store(HookState::Installed);
            return true;
        }
        Sleep(kSearchDelayMs);
    }

    Log("ERROR", "timed out locating or hooking SetActiveGameIndex");
    context.state.store(HookState::Failed);
    return false;
}

int State() noexcept {
    return static_cast<int>(Context().state.load(std::memory_order_acquire));
}

} // namespace menu_event_hook
