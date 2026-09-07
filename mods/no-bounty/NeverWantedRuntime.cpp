// DesertLink NeverWanted v1.1 TEST
// Crimson Desert 2.01.00 / CrimsonDesert.exe 1.0.0.2760 ONLY.
//
// Architecture: hook-free, session-only live data-table patch.
// - WantedInfo: _isBlocked = 1, _increasePrice = 0
// - TribeInfo:  _wantedCrimeType = 0
//
// The offsets below were re-derived from the user's exact 2760 executable and
// live process dump. No request serializer / AI tick hook is installed.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>

namespace {

HMODULE g_self = nullptr;
volatile LONG g_stop = 0;

constexpr std::uint32_t kExpectedImageSize = 0x16F1F000u;

// Exact 2760 resolver/global locations from the live dump.
constexpr std::uintptr_t kTribeResolverRva = 0x00382060ull;
constexpr std::uintptr_t kWantedResolverRva = 0x017C7940ull;
constexpr std::uintptr_t kTribeManagerGlobalRva = 0x06C2A088ull;
constexpr std::uintptr_t kWantedManagerGlobalRva = 0x06C4AC10ull;

// StaticInfoManager2-like manager layout confirmed by both resolvers:
//   +0x08 u32 count
//   +0x58 pointer to qword row-pointer array
constexpr std::uintptr_t kManagerCountOff = 0x08;
constexpr std::uintptr_t kManagerRowsOff = 0x58;

// Runtime row layout, NOT pabgb wire offsets.
// WantedInfo constructor on 2760 allocates 0x28 bytes and initializes:
//   +0x10 _isBlocked, +0x18 _increasePrice (u64), +0x20 _useTargetPrice.
constexpr std::uintptr_t kWantedIsBlockedOff = 0x10;
constexpr std::uintptr_t kWantedIncreasePriceOff = 0x18;
constexpr std::uintptr_t kWantedUseTargetPriceOff = 0x20;

// TribeInfo constructor on 2760 allocates 0x80 bytes. The established
// in-memory field map places _wantedCrimeType at +0x1C; the current 2760
// constructor layout still preserves the +0x10/+0x16/+0x18.. field block.
constexpr std::uintptr_t kTribeIsBlockedOff = 0x10;
constexpr std::uintptr_t kTribeMassLevelOff = 0x16;
constexpr std::uintptr_t kTribeWantedCrimeTypeOff = 0x1C;

FILE* g_log = nullptr;

void OpenLog() {
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(g_self, path, MAX_PATH)) return;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *(slash + 1) = L'\0';
    wcscat_s(path, L"NeverWanted_DesertLink_v1.1_TEST.log");
    _wfopen_s(&g_log, path, L"w, ccs=UTF-8");
}

void Log(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

bool IsUserPtr(std::uintptr_t p) {
    return p >= 0x10000ull && p < 0x0000800000000000ull;
}

bool ReadU8(std::uintptr_t p, std::uint8_t& out) {
    __try { out = *reinterpret_cast<volatile std::uint8_t*>(p); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool ReadU32(std::uintptr_t p, std::uint32_t& out) {
    __try { out = *reinterpret_cast<volatile std::uint32_t*>(p); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool ReadU64(std::uintptr_t p, std::uint64_t& out) {
    __try { out = *reinterpret_cast<volatile std::uint64_t*>(p); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool ReadPtr(std::uintptr_t p, std::uintptr_t& out) {
    std::uint64_t v = 0;
    if (!ReadU64(p, v)) return false;
    out = static_cast<std::uintptr_t>(v);
    return true;
}

bool WriteU8(std::uintptr_t p, std::uint8_t v) {
    __try { *reinterpret_cast<volatile std::uint8_t*>(p) = v; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool WriteU64(std::uintptr_t p, std::uint64_t v) {
    __try { *reinterpret_cast<volatile std::uint64_t*>(p) = v; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool VerifyBytes(std::uintptr_t p, const std::uint8_t* sig, std::size_t n) {
    __try {
        for (std::size_t i = 0; i < n; ++i)
            if (*reinterpret_cast<volatile std::uint8_t*>(p + i) != sig[i]) return false;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

struct Manager {
    std::uintptr_t ptr = 0;
    std::uintptr_t rows = 0;
    std::uint32_t count = 0;
};

bool SnapshotManager(std::uintptr_t base, std::uintptr_t globalRva,
                     std::uint32_t maxCount, Manager& m) {
    m = {};
    if (!ReadPtr(base + globalRva, m.ptr) || !IsUserPtr(m.ptr)) return false;
    if (!ReadU32(m.ptr + kManagerCountOff, m.count)) return false;
    if (m.count == 0 || m.count > maxCount) return false;
    if (!ReadPtr(m.ptr + kManagerRowsOff, m.rows) || !IsUserPtr(m.rows)) return false;
    return true;
}

struct PatchStats {
    std::uint32_t tribeSeen = 0;
    std::uint32_t tribeChanged = 0;
    std::uint32_t tribeAlreadyZero = 0;
    std::uint32_t wantedSeen = 0;
    std::uint32_t wantedBlocked = 0;
    std::uint32_t wantedPriceZeroed = 0;
};

void PatchTribe(const Manager& m, PatchStats& s) {
    for (std::uint32_t i = 0; i < m.count; ++i) {
        std::uintptr_t row = 0;
        if (!ReadPtr(m.rows + static_cast<std::uintptr_t>(i) * 8ull, row) || !IsUserPtr(row)) continue;

        std::uint8_t isBlocked = 0, massLevel = 0, crimeType = 0;
        if (!ReadU8(row + kTribeIsBlockedOff, isBlocked) || isBlocked > 1) continue;
        if (!ReadU8(row + kTribeMassLevelOff, massLevel) || massLevel > 0x40) continue;
        if (!ReadU8(row + kTribeWantedCrimeTypeOff, crimeType)) continue;

        ++s.tribeSeen;
        if (crimeType == 0) {
            ++s.tribeAlreadyZero;
            continue;
        }
        if (WriteU8(row + kTribeWantedCrimeTypeOff, 0)) ++s.tribeChanged;
    }
}

void PatchWanted(const Manager& m, PatchStats& s) {
    for (std::uint32_t i = 0; i < m.count; ++i) {
        std::uintptr_t row = 0;
        if (!ReadPtr(m.rows + static_cast<std::uintptr_t>(i) * 8ull, row) || !IsUserPtr(row)) continue;

        std::uint8_t blocked = 0, useTargetPrice = 0;
        std::uint64_t price = 0;
        if (!ReadU8(row + kWantedIsBlockedOff, blocked) || blocked > 1) continue;
        if (!ReadU8(row + kWantedUseTargetPriceOff, useTargetPrice) || useTargetPrice > 1) continue;
        if (!ReadU64(row + kWantedIncreasePriceOff, price)) continue;

        ++s.wantedSeen;
        if (blocked == 0 && WriteU8(row + kWantedIsBlockedOff, 1)) ++s.wantedBlocked;
        if (price != 0 && WriteU64(row + kWantedIncreasePriceOff, 0)) ++s.wantedPriceZeroed;
    }
}

DWORD WINAPI Worker(void*) {
    OpenLog();
    Log("NeverWanted DesertLink v1.1 TEST");
    Log("Target: Crimson Desert 2.01.00 / EXE 1.0.0.2760");
    Log("Hook-free live table patch: WantedInfo block+price, TribeInfo wantedCrimeType.");

    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!base) { Log("STATUS REFUSED: game module not found"); return 0; }

    bool imageOk = false;
    __try {
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        imageOk = dos->e_magic == IMAGE_DOS_SIGNATURE && nt->Signature == IMAGE_NT_SIGNATURE &&
                  nt->OptionalHeader.SizeOfImage == kExpectedImageSize;
    } __except (EXCEPTION_EXECUTE_HANDLER) { imageOk = false; }
    if (!imageOk) { Log("STATUS REFUSED: SizeOfImage/build mismatch"); return 0; }

    // Exact first 19 bytes of both current resolver clones. Only RIP displacement differs,
    // so verify the stable prologue/key-load shape around it.
    static const std::uint8_t kResolverPrefix[] = {
        0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C,0x24,0x18,
        0x56,0x57,0x41,0x56,0x48,0x83,0xEC,0x50,0x0F,0xB7,0x39
    };
    if (!VerifyBytes(base + kTribeResolverRva, kResolverPrefix, sizeof(kResolverPrefix)) ||
        !VerifyBytes(base + kWantedResolverRva, kResolverPrefix, sizeof(kResolverPrefix))) {
        Log("STATUS REFUSED: resolver signature mismatch");
        return 0;
    }

    Log("BUILD VERIFIED: SizeOfImage=0x%08X tribeResolver=0x%llX wantedResolver=0x%llX",
        kExpectedImageSize,
        static_cast<unsigned long long>(kTribeResolverRva),
        static_cast<unsigned long long>(kWantedResolverRva));

    std::uint32_t lastTribeSeen = 0xFFFFFFFFu, lastWantedSeen = 0xFFFFFFFFu;
    bool activeLogged = false;

    while (InterlockedCompareExchange(&g_stop, 0, 0) == 0) {
        Manager tribe{}, wanted{};
        const bool tribeOk = SnapshotManager(base, kTribeManagerGlobalRva, 4096, tribe);
        const bool wantedOk = SnapshotManager(base, kWantedManagerGlobalRva, 1024, wanted);

        PatchStats st{};
        if (tribeOk) PatchTribe(tribe, st);
        if (wantedOk) PatchWanted(wanted, st);

        if (tribeOk && wantedOk && st.tribeSeen > 0 && st.wantedSeen > 0 && !activeLogged) {
            Log("STATUS ACTIVE");
            Log("TribeInfo manager=0x%llX count=%u rows=0x%llX",
                static_cast<unsigned long long>(tribe.ptr), tribe.count,
                static_cast<unsigned long long>(tribe.rows));
            Log("WantedInfo manager=0x%llX count=%u rows=0x%llX",
                static_cast<unsigned long long>(wanted.ptr), wanted.count,
                static_cast<unsigned long long>(wanted.rows));
            activeLogged = true;
        }

        if (st.tribeSeen != lastTribeSeen || st.wantedSeen != lastWantedSeen ||
            st.tribeChanged || st.wantedBlocked || st.wantedPriceZeroed) {
            Log("PATCH tribeSeen=%u tribeChanged=%u tribeAlreadyZero=%u wantedSeen=%u wantedBlocked=%u wantedPriceZeroed=%u",
                st.tribeSeen, st.tribeChanged, st.tribeAlreadyZero,
                st.wantedSeen, st.wantedBlocked, st.wantedPriceZeroed);
            lastTribeSeen = st.tribeSeen;
            lastWantedSeen = st.wantedSeen;
        }

        Sleep(100);
    }

    Log("STOP");
    if (g_log) { fclose(g_log); g_log = nullptr; }
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = hModule;
        DisableThreadLibraryCalls(hModule);
        HANDLE h = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    } else if (reason == DLL_PROCESS_DETACH) {
        InterlockedExchange(&g_stop, 1);
    }
    return TRUE;
}
