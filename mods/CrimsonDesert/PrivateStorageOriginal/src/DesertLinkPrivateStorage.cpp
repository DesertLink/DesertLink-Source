#include <Windows.h>
#include <TlHelp32.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace dlps
{
    static HMODULE g_self = nullptr;
    static uintptr_t g_base = 0;
    static size_t g_imageSize = 0;

    static FILE* g_log = nullptr;
    static CRITICAL_SECTION g_logCs;
    static bool g_logCsInit = false;

    static std::atomic<bool> g_stop{false};
    static std::atomic<bool> g_ready{false};
    static std::atomic<bool> g_open{false};
    static std::atomic<bool> g_toggleRequest{false};
    static std::atomic<bool> g_closeRequest{false};

    static uintptr_t g_phaseManager = 0;
    static uintptr_t g_stageManager = 0;

    static constexpr DWORD kExpectedTimeDateStamp = 0x6AB28F00;
    static constexpr DWORD kExpectedSizeOfImage = 0x173AB000;
    static constexpr uint32_t kPlayerActor = 0xA0100001;
    static constexpr uint8_t kPhaseIngameMenu = 0x0E;
    static constexpr uint8_t kScreenIngame = 0x10;

    struct Resolved
    {
        uintptr_t eventPost = 0;
        uintptr_t requestPhase = 0;
        uintptr_t inputBlockSet = 0;
        uintptr_t modeSwitch = 0;
        uintptr_t eventManagerGlobal = 0;
        uintptr_t actorManagerGlobal = 0;
        uintptr_t eventWrapVtable = 0;
        uintptr_t stageManagerVtable = 0;
        unsigned phaseScreenOffset = 0;
        unsigned eventWrapOffset = 0;
    };

    static Resolved G{};

    static void Log(const char* level, const char* fmt, ...)
    {
        if (!g_logCsInit) return;
        EnterCriticalSection(&g_logCs);

        SYSTEMTIME st{};
        GetLocalTime(&st);
        if (g_log)
            std::fprintf(g_log, "[%02u:%02u:%02u.%03u] [%s] ",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, level);

        va_list ap;
        va_start(ap, fmt);
        if (g_log) std::vfprintf(g_log, fmt, ap);
        va_end(ap);

        if (g_log)
        {
            std::fputc('\n', g_log);
            std::fflush(g_log);
        }
        LeaveCriticalSection(&g_logCs);
    }

    static bool Readable(uintptr_t p, size_t n)
    {
        if (p < 0x10000 || n == 0) return false;
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(p), &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        const uintptr_t end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        return p + n <= end;
    }

    static bool Executable(uintptr_t p, size_t n)
    {
        if (!Readable(p, n)) return false;
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(p), &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
        const DWORD x = mbi.Protect & 0xFF;
        return x == PAGE_EXECUTE || x == PAGE_EXECUTE_READ ||
               x == PAGE_EXECUTE_READWRITE || x == PAGE_EXECUTE_WRITECOPY;
    }

    template <typename T>
    static bool Read(uintptr_t p, T& out)
    {
        if (!Readable(p, sizeof(T))) return false;
        __try
        {
            out = *reinterpret_cast<volatile T*>(p);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static bool ReadCString(uintptr_t p, char* out, size_t cap)
    {
        if (!out || cap < 2 || p < 0x10000) return false;
        __try
        {
            size_t i = 0;
            for (; i + 1 < cap; ++i)
            {
                const char c = *reinterpret_cast<volatile char*>(p + i);
                if (c == 0) break;
                if (static_cast<unsigned char>(c) < 0x20 ||
                    static_cast<unsigned char>(c) > 0x7E)
                    return false;
                out[i] = c;
            }
            out[i] = 0;
            return i > 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    struct Pattern
    {
        uint8_t bytes[160]{};
        uint8_t fixed[160]{};
        size_t count = 0;
    };

    static int Hex(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    static bool ParsePattern(const char* text, Pattern& out)
    {
        out = {};
        for (size_t i = 0; text && text[i];)
        {
            while (text[i] == ' ' || text[i] == '\t') ++i;
            if (!text[i]) break;
            if (out.count >= sizeof(out.bytes)) return false;

            if (text[i] == '?')
            {
                out.fixed[out.count] = 0;
                out.bytes[out.count++] = 0;
                ++i;
                if (text[i] == '?') ++i;
                continue;
            }

            const int hi = Hex(text[i]);
            const int lo = Hex(text[i + 1]);
            if (hi < 0 || lo < 0) return false;

            out.bytes[out.count] = static_cast<uint8_t>((hi << 4) | lo);
            out.fixed[out.count] = 1;
            ++out.count;
            i += 2;
        }
        return out.count != 0;
    }

    static bool Match(uintptr_t p, const Pattern& pat)
    {
        if (!Readable(p, pat.count)) return false;
        __try
        {
            const uint8_t* b = reinterpret_cast<const uint8_t*>(p);
            for (size_t i = 0; i < pat.count; ++i)
                if (pat.fixed[i] && b[i] != pat.bytes[i])
                    return false;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static uintptr_t FindUnique(const char* name, const char* spec)
    {
        Pattern pat{};
        if (!ParsePattern(spec, pat)) return 0;

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(g_base + dos->e_lfanew);
        const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);

        uintptr_t hit = 0;
        unsigned count = 0;

        for (unsigned si = 0; si < nt->FileHeader.NumberOfSections; ++si)
        {
            if (!(sec[si].Characteristics & IMAGE_SCN_MEM_READ)) continue;
            uintptr_t begin = g_base + sec[si].VirtualAddress;
            uintptr_t end = begin + sec[si].Misc.VirtualSize;
            if (end > g_base + g_imageSize) end = g_base + g_imageSize;

            for (uintptr_t p = begin; p + pat.count <= end; ++p)
            {
                if (!Match(p, pat)) continue;
                if (!hit) hit = p;
                ++count;
                if (count > 1) break;
            }
            if (count > 1) break;
        }

        if (count != 1)
        {
            Log("error", "%s pattern count=%u", name, count);
            return 0;
        }

        Log("info", "%s +0x%llX", name,
            static_cast<unsigned long long>(hit - g_base));
        return hit;
    }

    static uintptr_t RipTarget(uintptr_t instruction, unsigned length)
    {
        int32_t disp = 0;
        if (!Read(instruction + length - 4, disp)) return 0;
        return instruction + length + static_cast<int64_t>(disp);
    }

    static bool SectionRange(unsigned index, uintptr_t& begin, uintptr_t& end, DWORD& ch)
    {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(g_base + dos->e_lfanew);
        if (index >= nt->FileHeader.NumberOfSections) return false;
        const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt) + index;
        begin = g_base + sec->VirtualAddress;
        end = begin + sec->Misc.VirtualSize;
        if (end > g_base + g_imageSize) end = g_base + g_imageSize;
        ch = sec->Characteristics;
        return begin < end;
    }

    static uintptr_t FindAscii(const char* needle)
    {
        const size_t n = std::strlen(needle) + 1;
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(g_base + dos->e_lfanew);

        for (unsigned si = 0; si < nt->FileHeader.NumberOfSections; ++si)
        {
            uintptr_t begin = 0, end = 0;
            DWORD ch = 0;
            if (!SectionRange(si, begin, end, ch) || !(ch & IMAGE_SCN_MEM_READ)) continue;

            for (uintptr_t p = begin; p + n <= end; ++p)
            {
                if (!Readable(p, n)) continue;
                __try
                {
                    if (std::memcmp(reinterpret_cast<const void*>(p), needle, n) == 0)
                        return p;
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                }
            }
        }
        return 0;
    }

    static bool ValidCol(uintptr_t col)
    {
        if (col < g_base || col + 24 > g_base + g_imageSize) return false;
        uint32_t sig = 0, self = 0;
        if (!Read(col + 0x00, sig) || !Read(col + 0x14, self)) return false;
        return sig == 1 && g_base + self == col;
    }

    static uintptr_t FindVtable(const char* decorated)
    {
        const uintptr_t name = FindAscii(decorated);
        if (!name || name < g_base + 0x10)
        {
            Log("error", "RTTI name missing: %s", decorated);
            return 0;
        }

        const uintptr_t typeDescriptor = name - 0x10;
        const uint32_t typeRva = static_cast<uint32_t>(typeDescriptor - g_base);

        uintptr_t found = 0;
        unsigned foundCount = 0;

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(g_base + dos->e_lfanew);

        for (unsigned si = 0; si < nt->FileHeader.NumberOfSections; ++si)
        {
            uintptr_t begin = 0, end = 0;
            DWORD ch = 0;
            if (!SectionRange(si, begin, end, ch) || !(ch & IMAGE_SCN_MEM_READ)) continue;

            for (uintptr_t col = (begin + 3) & ~uintptr_t(3); col + 24 <= end; col += 4)
            {
                uint32_t tr = 0;
                if (!Read(col + 0x0C, tr) || tr != typeRva || !ValidCol(col)) continue;

                for (unsigned sj = 0; sj < nt->FileHeader.NumberOfSections; ++sj)
                {
                    uintptr_t b2 = 0, e2 = 0;
                    DWORD ch2 = 0;
                    if (!SectionRange(sj, b2, e2, ch2) || !(ch2 & IMAGE_SCN_MEM_READ)) continue;

                    for (uintptr_t p = (b2 + 7) & ~uintptr_t(7); p + 16 <= e2; p += 8)
                    {
                        uintptr_t q = 0;
                        if (!Read(p, q) || q != col) continue;
                        const uintptr_t vt = p + 8;
                        uintptr_t first = 0;
                        if (!Read(vt, first) || !Executable(first, 1)) continue;

                        if (!found) found = vt;
                        ++foundCount;
                    }
                }
            }
        }

        if (foundCount != 1)
        {
            Log("error", "RTTI vtable count=%u for %s", foundCount, decorated);
            return 0;
        }

        Log("info", "vtable %s +0x%llX", decorated,
            static_cast<unsigned long long>(found - g_base));
        return found;
    }

    static bool FindNamedWrapOffset(unsigned& out)
    {
        static const char* kPattern =
            "48 8D 8E ?? ?? ?? ?? E8 ?? ?? ?? ?? 4C 89 78 10 "
            "41 B8 01 00 00 00 48 8D 15 ?? ?? ?? ??";

        Pattern pat{};
        if (!ParsePattern(kPattern, pat)) return false;

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(g_base + dos->e_lfanew);

        unsigned named = 0;
        uint32_t slot = 0;

        for (unsigned si = 0; si < nt->FileHeader.NumberOfSections; ++si)
        {
            uintptr_t begin = 0, end = 0;
            DWORD ch = 0;
            if (!SectionRange(si, begin, end, ch) || !(ch & IMAGE_SCN_MEM_READ)) continue;

            for (uintptr_t p = begin; p + pat.count <= end; ++p)
            {
                if (!Match(p, pat)) continue;
                const uintptr_t namePtr = RipTarget(p + 22, 7);
                char name[64]{};
                if (!ReadCString(namePtr, name, sizeof(name))) continue;
                if (std::strcmp(name, "StageChartUIControl") != 0) continue;

                uint32_t candidate = 0;
                if (!Read(p + 3, candidate)) continue;
                slot = candidate;
                ++named;
            }
        }

        if (named != 1 || slot < 0x100 || slot > 0x4000 || (slot & 7))
        {
            Log("error", "StageChartUIControl wrap offset unresolved (hits=%u slot=0x%X)", named, slot);
            return false;
        }

        out = slot;
        Log("info", "StageChartUIControl wrap offset 0x%X", out);
        return true;
    }

    static uintptr_t FunctionEntry(uintptr_t inside)
    {
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(inside, &imageBase, nullptr);
        if (!rf) return 0;
        return static_cast<uintptr_t>(imageBase + rf->BeginAddress);
    }

    static bool Resolve()
    {
        G.eventPost = FindUnique("EventPost",
            "4C 8B DC 49 89 5B 18 49 89 73 20 89 54 24 10 57 41 56 41 57 "
            "48 81 EC 90 00 00 00 4D 8B F1 4D 8B F8 8B FA 48 8B F1 80 79 1A 00");

        G.requestPhase = FindUnique("RequestPhase",
            "48 89 5C 24 10 48 89 6C 24 20 56 57 41 56 48 83 EC 30 "
            "41 0F B6 F0 0F B6 EA 4C 8B F1 41 B0 01 48 8B 11 48 8D 4C 24 20 "
            "E8 ?? ?? ?? ??");

        G.inputBlockSet = FindUnique("InputBlockSet",
            "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 20 44 88 44 24 18 "
            "57 41 54 41 55 41 56 41 57 48 83 EC 40 41 0F B6 E8 4C 8B FA");

        const uintptr_t eventMgrSite = FindUnique("EventManagerGlobalSite",
            "48 8B 1D ?? ?? ?? ?? 48 89 5C 24 78 48 8B 03 48 8B CB FF 50 08 "
            "90 0F B7 06 66 89 44 24 60");
        if (eventMgrSite) G.eventManagerGlobal = RipTarget(eventMgrSite, 7);

        const uintptr_t actorMgrSite = FindUnique("ActorManagerGlobalSite",
            "48 8B 0D ?? ?? ?? ?? 48 8B 49 58 E8 ?? ?? ?? ?? 90 "
            "40 38 74 24 40 40 0F 94 C5 48 8D 05 ?? ?? ?? ??");
        if (actorMgrSite) G.actorManagerGlobal = RipTarget(actorMgrSite, 7);

        const uintptr_t modeBody = FindUnique("ModeSwitchBody",
            "48 8D AC 24 F0 FE FF FF 48 81 EC 10 02 00 00 "
            "48 8B D9 48 8B 41 08 48 8B 50 78 4C 8B ?? 4D 85");
        if (modeBody)
        {
            const uintptr_t entry = FunctionEntry(modeBody);
            if (entry && modeBody >= entry && modeBody - entry <= 0x40)
            {
                G.modeSwitch = entry;
                Log("info", "ModeSwitch +0x%llX",
                    static_cast<unsigned long long>(entry - g_base));
            }
            else
            {
                Log("error", "ModeSwitch function entry validation failed");
            }
        }

        const uintptr_t screenRead = FindUnique("PhaseScreenByteRead",
            "44 0F B6 43 ?? 0F B6 53 28");
        if (screenRead)
        {
            uint8_t off = 0;
            if (Read(screenRead + 4, off) && off > 0x28 && off < 0x40)
            {
                G.phaseScreenOffset = off;
                Log("info", "phase screen offset 0x%02X", off);
            }
        }

        G.eventWrapVtable = FindVtable(
            ".?AV?$UIEventWrap@$$CBVActorKey@pa@@AEBVstaticstringA@2@AEBV32@AEBVSequencerStageId@2@AEBVStageChartUIControlCommandData@2@XXXX@pa@@");

        G.stageManagerVtable = FindVtable(
            ".?AVClientSequencerStageManager@pa@@");

        FindNamedWrapOffset(G.eventWrapOffset);

        const bool ok =
            G.eventPost && G.requestPhase && G.inputBlockSet && G.modeSwitch &&
            G.eventManagerGlobal && G.actorManagerGlobal &&
            G.eventWrapVtable && G.stageManagerVtable &&
            G.phaseScreenOffset && G.eventWrapOffset;

        if (!ok) Log("error", "required native targets did not resolve");
        return ok;
    }

    struct Detour
    {
        uintptr_t target = 0;
        unsigned stolen = 0;
        uint8_t original[32]{};
        void* trampoline = nullptr;
    };

    static Detour g_modeDetour{};

    static unsigned PrologueInstructionLength(const uint8_t* p)
    {
        if (!p) return 0;

        if (p[0] == 0x53 || p[0] == 0x55 || p[0] == 0x56 || p[0] == 0x57)
            return 1;

        if (p[0] == 0x40 &&
            (p[1] == 0x53 || p[1] == 0x55 || p[1] == 0x56 || p[1] == 0x57))
            return 2;

        if (p[0] == 0x41 &&
            (p[1] == 0x54 || p[1] == 0x55 || p[1] == 0x56 || p[1] == 0x57))
            return 2;

        if (p[0] == 0x48 && p[1] == 0x89 &&
            (p[2] == 0x5C || p[2] == 0x6C || p[2] == 0x74 || p[2] == 0x7C) &&
            p[3] == 0x24)
            return 5;

        if (p[0] == 0x4C && p[1] == 0x89 &&
            (p[2] == 0x64 || p[2] == 0x6C || p[2] == 0x74 || p[2] == 0x7C) &&
            p[3] == 0x24)
            return 5;

        if (p[0] == 0x48 && p[1] == 0x83 && p[2] == 0xEC)
            return 4;

        if (p[0] == 0x48 && p[1] == 0x81 && p[2] == 0xEC)
            return 7;

        if (p[0] == 0x48 && p[1] == 0x8B &&
            (p[2] == 0xEC || p[2] == 0xD9 || p[2] == 0xF1 || p[2] == 0xF9))
            return 3;

        if (p[0] == 0x4C && p[1] == 0x8B &&
            (p[2] == 0xE1 || p[2] == 0xE9 || p[2] == 0xF1 || p[2] == 0xF9))
            return 3;

        if (p[0] == 0x48 && p[1] == 0x8D && p[2] == 0x6C && p[3] == 0x24)
            return 5;

        if (p[0] == 0x48 && p[1] == 0x8D && p[2] == 0xAC && p[3] == 0x24)
            return 8;

        return 0;
    }

    static unsigned SafeStolenLength(uintptr_t target)
    {
        if (!Executable(target, 24)) return 0;

        unsigned total = 0;
        while (total < 12)
        {
            const auto* p = reinterpret_cast<const uint8_t*>(target + total);
            const unsigned n = PrologueInstructionLength(p);
            if (!n || total + n > 31) return 0;
            total += n;
        }
        return total;
    }

    struct SuspendedThread
    {
        HANDLE h = nullptr;
        DWORD id = 0;
    };

    static bool PatchBytes(uintptr_t target, const void* bytes, unsigned count)
    {
        DWORD oldProtect = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(target), count, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        SuspendedThread threads[256]{};
        unsigned threadCount = 0;
        const DWORD selfTid = GetCurrentThreadId();
        const DWORD pid = GetCurrentProcessId();

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap != INVALID_HANDLE_VALUE)
        {
            THREADENTRY32 te{};
            te.dwSize = sizeof(te);
            if (Thread32First(snap, &te))
            {
                do
                {
                    if (te.th32OwnerProcessID != pid ||
                        te.th32ThreadID == selfTid ||
                        threadCount >= 256)
                        continue;

                    HANDLE h = OpenThread(
                        THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT,
                        FALSE, te.th32ThreadID);
                    if (!h) continue;

                    if (SuspendThread(h) == DWORD(-1))
                    {
                        CloseHandle(h);
                        continue;
                    }

                    CONTEXT ctx{};
                    ctx.ContextFlags = CONTEXT_CONTROL;
                    if (GetThreadContext(h, &ctx) &&
                        ctx.Rip >= target && ctx.Rip < target + count)
                    {
                        ResumeThread(h);
                        CloseHandle(h);

                        for (unsigned i = 0; i < threadCount; ++i)
                        {
                            ResumeThread(threads[i].h);
                            CloseHandle(threads[i].h);
                        }
                        CloseHandle(snap);
                        VirtualProtect(reinterpret_cast<void*>(target), count, oldProtect, &oldProtect);
                        Log("error", "refusing patch: a thread is inside target prologue");
                        return false;
                    }

                    threads[threadCount++] = { h, te.th32ThreadID };
                }
                while (Thread32Next(snap, &te));
            }
            CloseHandle(snap);
        }

        std::memcpy(reinterpret_cast<void*>(target), bytes, count);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(target), count);

        for (unsigned i = 0; i < threadCount; ++i)
        {
            ResumeThread(threads[i].h);
            CloseHandle(threads[i].h);
        }

        VirtualProtect(reinterpret_cast<void*>(target), count, oldProtect, &oldProtect);
        return true;
    }

    static bool InstallDetour(uintptr_t target, void* detour, Detour& out)
    {
        const unsigned stolen = SafeStolenLength(target);
        if (!stolen)
        {
            Log("error", "unsupported ModeSwitch prologue; no hook installed");
            return false;
        }

        uint8_t* tramp = static_cast<uint8_t*>(
            VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!tramp) return false;

        std::memcpy(tramp, reinterpret_cast<const void*>(target), stolen);

        uint8_t* j = tramp + stolen;
        j[0] = 0x48;
        j[1] = 0xB8;
        const uintptr_t back = target + stolen;
        std::memcpy(j + 2, &back, sizeof(back));
        j[10] = 0xFF;
        j[11] = 0xE0;

        uint8_t patch[32]{};
        for (unsigned i = 0; i < stolen; ++i) patch[i] = 0x90;
        patch[0] = 0x48;
        patch[1] = 0xB8;
        const uintptr_t dst = reinterpret_cast<uintptr_t>(detour);
        std::memcpy(patch + 2, &dst, sizeof(dst));
        patch[10] = 0xFF;
        patch[11] = 0xE0;

        out.target = target;
        out.stolen = stolen;
        out.trampoline = tramp;
        std::memcpy(out.original, reinterpret_cast<const void*>(target), stolen);

        if (!PatchBytes(target, patch, stolen))
        {
            VirtualFree(tramp, 0, MEM_RELEASE);
            out = {};
            return false;
        }

        Log("info", "ModeSwitch detour installed (+0x%llX, %u bytes)",
            static_cast<unsigned long long>(target - g_base), stolen);
        return true;
    }

    static void RemoveDetour(Detour& d)
    {
        if (!d.target || !d.stolen) return;
        PatchBytes(d.target, d.original, d.stolen);
        d = {};
    }

    struct StaticNode
    {
        const char* ptr;
        uint32_t len;
        uint32_t hash;
        int32_t refs;
        uint8_t flag;
        uint8_t pad[3];
        char data[192];
    };

    static StaticNode* MakeNode(const char* text)
    {
        if (!text) return nullptr;
        const size_t n = std::strlen(text);
        if (n >= sizeof(StaticNode::data)) return nullptr;

        StaticNode* node = static_cast<StaticNode*>(
            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(StaticNode)));
        if (!node) return nullptr;

        std::memcpy(node->data, text, n + 1);
        node->ptr = node->data;
        node->len = static_cast<uint32_t>(n);
        node->hash = 0xFFFFFFFFu;
        node->refs = -1;
        return node;
    }

    struct CommandArg
    {
        uint8_t type;
        uint8_t pad[7];
        uint64_t value;
    };

    struct CommandSub
    {
        uint8_t kind;
        uint8_t pad[7];
        CommandArg* args;
        uint32_t count;
        uint32_t capacity;
    };

    struct CommandData
    {
        uint8_t kind;
        uint8_t pad[7];
        CommandSub* subs;
        uint32_t count;
        uint32_t capacity;
    };

    using EventPostFn = void(__fastcall*)(
        uintptr_t, uint32_t, uintptr_t*, uintptr_t*, uint64_t*, CommandData*);
    using RequestPhaseFn = uintptr_t(__fastcall*)(uintptr_t, uint32_t, uint32_t);
    using InputBlockFn = void(__fastcall*)(uintptr_t, uint64_t*, uint32_t);
    using ModeSwitchFn = uintptr_t(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);

    static StaticNode* nView = nullptr;
    static StaticNode* nSelector = nullptr;
    static StaticNode* nIcon = nullptr;
    static StaticNode* nTitleHash = nullptr;
    static StaticNode* nModalHash = nullptr;
    static StaticNode* nInventoryCmd = nullptr;
    static StaticNode* nTitleCmd = nullptr;

    static uint64_t g_stageId = 920000000;
    static uint64_t g_activeStageId = 0;
    static uintptr_t g_blockManager = 0;
    static uint64_t g_blockKey = 0;
    static uint32_t g_openFrames = 0;

    static uintptr_t EventWrap()
    {
        uintptr_t mgr = 0, wrap = 0, vt = 0;
        if (!Read(G.eventManagerGlobal, mgr) ||
            !Read(mgr + G.eventWrapOffset, wrap) ||
            !Read(wrap, vt))
            return 0;
        return vt == G.eventWrapVtable ? wrap : 0;
    }

    static bool CallEventPost(
        uint8_t kind, uint64_t stageId, StaticNode* idNode,
        CommandSub* extras, uint32_t extraCount)
    {
        const uintptr_t wrap = EventWrap();
        if (!wrap || !idNode) return false;

        CommandArg idArg{};
        idArg.type = 5;
        idArg.value = reinterpret_cast<uint64_t>(idNode);

        CommandSub subs[4]{};
        subs[0].kind = 0;
        subs[0].args = &idArg;
        subs[0].count = 1;
        subs[0].capacity = 1;

        if (extraCount > 3) extraCount = 3;
        for (uint32_t i = 0; i < extraCount; ++i)
            subs[1 + i] = extras[i];

        CommandData data{};
        data.kind = kind;
        data.subs = subs;
        data.count = 1 + extraCount;
        data.capacity = data.count;

        uintptr_t view = reinterpret_cast<uintptr_t>(nView);
        uintptr_t selector = reinterpret_cast<uintptr_t>(nSelector);
        uint64_t id = stageId;

        __try
        {
            reinterpret_cast<EventPostFn>(G.eventPost)(
                wrap, kPlayerActor, &view, &selector, &id, &data);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static bool CallRequestPhase(uintptr_t pm, bool on)
    {
        __try
        {
            reinterpret_cast<RequestPhaseFn>(G.requestPhase)(
                pm, kPhaseIngameMenu, on ? 1u : 0u);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static bool CallInputBlock(uintptr_t mgr, uint64_t key, uint32_t mode)
    {
        __try
        {
            reinterpret_cast<InputBlockFn>(G.inputBlockSet)(mgr, &key, mode);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static uintptr_t ResolveStageManager()
    {
        uintptr_t vt = 0;
        if (g_stageManager &&
            Read(g_stageManager, vt) &&
            vt == G.stageManagerVtable)
            return g_stageManager;

        uintptr_t root = 0, mgr = 0, user = 0, controlled = 0;
        if (!Read(G.actorManagerGlobal, root) ||
            !Read(root + 0x30, mgr) ||
            !Read(mgr + 0x58, user))
            return 0;

        Read(user + 0xD8, controlled);
        const uintptr_t owners[2] = { user, controlled };

        for (uintptr_t owner : owners)
        {
            if (!owner) continue;

            uintptr_t components = 0;
            Read(owner + 0x68, components);
            const uintptr_t tables[2] = { owner, components };

            for (uintptr_t table : tables)
            {
                if (!table) continue;
                for (unsigned off = 0; off < 0x800; off += 8)
                {
                    uintptr_t candidate = 0;
                    if (!Read(table + off, candidate) ||
                        !Read(candidate, vt) ||
                        vt != G.stageManagerVtable)
                        continue;

                    g_stageManager = candidate;
                    return candidate;
                }
            }
        }
        return 0;
    }

    static bool InFreePlay(uintptr_t pm, uint8_t* outScreen = nullptr)
    {
        uint8_t mode = 0, screen = 0, blocked = 0;
        uint32_t queued = 0;
        uintptr_t process = 0;

        if (!Read(pm + 0x28, mode) ||
            !Read(pm + G.phaseScreenOffset, screen) ||
            !Read(pm + 0x70, queued) ||
            !Read(pm + 0x10, process))
            return false;

        Read(process + 0xA0, blocked);
        if (outScreen) *outScreen = screen;

        return mode == 4 &&
               screen == kScreenIngame &&
               queued == 0 &&
               process != 0 &&
               blocked == 0;
    }

    static bool OpenStorage(uintptr_t pm)
    {
        if (g_open.load()) return true;
        if (!InFreePlay(pm))
        {
            Log("note", "open ignored: not in normal free play");
            return false;
        }

        const uintptr_t stageManager = ResolveStageManager();
        if (!stageManager)
        {
            Log("error", "ClientSequencerStageManager not found");
            return false;
        }

        if (!EventWrap())
        {
            Log("error", "StageChartUIControl event wrap unavailable");
            return false;
        }

        const uint64_t id = ++g_stageId;
        char idText[32]{};
        std::snprintf(idText, sizeof(idText), "%llu",
            static_cast<unsigned long long>(id));
        StaticNode* idNode = MakeNode(idText);
        if (!idNode)
        {
            Log("error", "stage id node allocation failed");
            return false;
        }

        g_activeStageId = id;
        g_openFrames = 0;
        g_open = true;

        if (CallInputBlock(stageManager, id, 2))
        {
            g_blockManager = stageManager;
            g_blockKey = id;
        }
        else
        {
            Log("error", "input block request failed");
        }

        if (!CallRequestPhase(pm, true))
            Log("error", "menu phase request failed");

        bool ok = true;

        ok = CallEventPost(0x12, id, idNode, nullptr, 0) && ok;

        CommandArg iconArg{};
        iconArg.type = 1;
        iconArg.value = reinterpret_cast<uint64_t>(nIcon);

        CommandArg titleArg{};
        titleArg.type = 9;
        titleArg.value = reinterpret_cast<uint64_t>(nTitleHash);

        CommandSub header[2]{};
        header[0].kind = 0x0B;
        header[0].args = &iconArg;
        header[0].count = header[0].capacity = 1;
        header[1].kind = 0x08;
        header[1].args = &titleArg;
        header[1].count = header[1].capacity = 1;
        ok = CallEventPost(0x14, id, idNode, header, 2) && ok;

        CommandArg modalArg{};
        modalArg.type = 9;
        modalArg.value = reinterpret_cast<uint64_t>(nModalHash);

        CommandSub modal{};
        modal.kind = 0x07;
        modal.args = &modalArg;
        modal.count = modal.capacity = 1;
        ok = CallEventPost(0x0D, id, idNode, &modal, 1) && ok;

        CommandArg inventoryArg{};
        inventoryArg.type = 5;
        inventoryArg.value = reinterpret_cast<uint64_t>(nInventoryCmd);

        CommandArg titleCmdArg{};
        titleCmdArg.type = 5;
        titleCmdArg.value = reinterpret_cast<uint64_t>(nTitleCmd);

        CommandSub commands[2]{};
        commands[0].kind = 0x0E;
        commands[0].args = &inventoryArg;
        commands[0].count = commands[0].capacity = 1;
        commands[1].kind = 0x0E;
        commands[1].args = &titleCmdArg;
        commands[1].count = commands[1].capacity = 1;
        ok = CallEventPost(0x15, id, idNode, commands, 2) && ok;

        ok = CallEventPost(0x0E, id, idNode, nullptr, 0) && ok;

        if (!ok)
            Log("error", "one or more StageChart packets failed");
        else
            Log("note", "Private Storage requested (stage %llu)",
                static_cast<unsigned long long>(id));

        return ok;
    }

    static void ForgetOpenState()
    {
        g_open = false;
        g_activeStageId = 0;
        g_openFrames = 0;
        g_blockManager = 0;
        g_blockKey = 0;
    }

    static void CloseStorage(uintptr_t pm)
    {
        if (!g_open.load()) return;

        char idText[32]{};
        std::snprintf(idText, sizeof(idText), "%llu",
            static_cast<unsigned long long>(g_activeStageId));
        StaticNode* idNode = MakeNode(idText);

        if (idNode)
            CallEventPost(0x0F, g_activeStageId, idNode, nullptr, 0);

        CallRequestPhase(pm, false);

        if (g_blockManager)
            CallInputBlock(g_blockManager, g_blockKey, 0);

        Log("note", "Private Storage close requested");
        ForgetOpenState();
    }

    static void Tick(uintptr_t pm)
    {
        g_phaseManager = pm;

        if (g_toggleRequest.exchange(false))
        {
            if (g_open.load()) CloseStorage(pm);
            else OpenStorage(pm);
        }

        if (g_closeRequest.exchange(false))
            CloseStorage(pm);

        if (!g_open.load()) return;

        ++g_openFrames;

        uint8_t mode = 0, screen = 0;
        if (!Read(pm + 0x28, mode) ||
            !Read(pm + G.phaseScreenOffset, screen))
            return;

        if (mode != 4)
        {
            ForgetOpenState();
            return;
        }

        if (g_openFrames == 180 && screen == kScreenIngame)
        {
            Log("error", "warehouse UI did not enter menu phase");
            CloseStorage(pm);
            return;
        }

        if (g_openFrames > 10 && screen == kScreenIngame)
        {
            if (g_blockManager)
                CallInputBlock(g_blockManager, g_blockKey, 0);
            ForgetOpenState();
        }
    }

    static uintptr_t __fastcall ModeSwitchHook(
        uintptr_t pm, uintptr_t rdx, uintptr_t r8, uintptr_t r9)
    {
        const auto original = reinterpret_cast<ModeSwitchFn>(g_modeDetour.trampoline);
        const uintptr_t result = original(pm, rdx, r8, r9);
        Tick(pm);
        return result;
    }

    struct WindowHook
    {
        HWND hwnd = nullptr;
        WNDPROC oldProc = nullptr;
    };

    static WindowHook g_windows[4]{};
    static LONG g_windowCount = 0;

    static bool IsGameClass(HWND hwnd)
    {
        wchar_t cls[128]{};
        if (!GetClassNameW(hwnd, cls, 128)) return false;
        return std::wcscmp(cls, L"Root") == 0 ||
               std::wcscmp(cls, L"WindowsLauncherClassName") == 0;
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) &&
            wp == VK_F1 &&
            (GetKeyState(VK_CONTROL) & 0x8000))
        {
            g_toggleRequest = true;
            return 0;
        }

        if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) &&
            wp == VK_ESCAPE &&
            g_open.load())
        {
            g_closeRequest = true;
            return 0;
        }

        WNDPROC oldProc = nullptr;
        const LONG count = InterlockedCompareExchange(&g_windowCount, 0, 0);
        for (LONG i = 0; i < count; ++i)
        {
            if (g_windows[i].hwnd == hwnd)
            {
                oldProc = g_windows[i].oldProc;
                break;
            }
        }

        return oldProc ? CallWindowProcW(oldProc, hwnd, msg, wp, lp)
                       : DefWindowProcW(hwnd, msg, wp, lp);
    }

    static BOOL CALLBACK EnumGameWindows(HWND hwnd, LPARAM)
    {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != GetCurrentProcessId()) return TRUE;
        if (!IsGameClass(hwnd)) return TRUE;

        LONG index = InterlockedCompareExchange(&g_windowCount, 0, 0);
        if (index >= 4) return FALSE;

        SetLastError(0);
        const LONG_PTR prev = SetWindowLongPtrW(
            hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProc));
        if (!prev && GetLastError() != 0)
            return TRUE;

        g_windows[index].hwnd = hwnd;
        g_windows[index].oldProc = reinterpret_cast<WNDPROC>(prev);
        InterlockedIncrement(&g_windowCount);
        Log("info", "subclassed game window");
        return TRUE;
    }

    static void RestoreWindows()
    {
        const LONG count = InterlockedCompareExchange(&g_windowCount, 0, 0);
        for (LONG i = 0; i < count; ++i)
        {
            if (g_windows[i].hwnd && g_windows[i].oldProc && IsWindow(g_windows[i].hwnd))
                SetWindowLongPtrW(
                    g_windows[i].hwnd, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(g_windows[i].oldProc));
        }
        InterlockedExchange(&g_windowCount, 0);
        std::memset(g_windows, 0, sizeof(g_windows));
    }

    static bool AllocateNodes()
    {
        nView = MakeNode("WareHouseView");
        nSelector = MakeNode("selector-stagechart-self");
        nIcon = MakeNode("cd_icon_map_bank");
        nTitleHash = MakeNode("1494912655");
        nModalHash = MakeNode("197270237");
        nInventoryCmd = MakeNode(
            "SetInventory(Character,Focus,True,Default;CampWareHouse,Focus,True,Default)");
        nTitleCmd = MakeNode(
            "SetWareHouseInventoryName(UI_WareHouse_CampStroage)");

        return nView && nSelector && nIcon && nTitleHash &&
               nModalHash && nInventoryCmd && nTitleCmd;
    }

    static bool ExactBuild()
    {
        if (!g_base) return false;

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;

        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(g_base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

        return nt->FileHeader.TimeDateStamp == kExpectedTimeDateStamp &&
               nt->OptionalHeader.SizeOfImage == kExpectedSizeOfImage;
    }

    static void InitLog()
    {
        InitializeCriticalSection(&g_logCs);
        g_logCsInit = true;

        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(g_self, path, MAX_PATH);
        wchar_t* slash = std::wcsrchr(path, L'\\');
        if (slash) *(slash + 1) = 0;
        std::wcscat_s(path, L"DesertLinkPrivateStorage.log");
        _wfopen_s(&g_log, path, L"w");
    }

    static DWORD WINAPI Worker(LPVOID)
    {
        Sleep(8000);

        if (!ExactBuild())
        {
            Log("error", "unsupported Crimson Desert build; expected 2.03.02 / 1.0.0.2976");
            return 0;
        }

        if (!Resolve())
            return 0;

        if (!AllocateNodes())
        {
            Log("error", "static-string allocation failed");
            return 0;
        }

        if (!InstallDetour(G.modeSwitch, reinterpret_cast<void*>(&ModeSwitchHook), g_modeDetour))
            return 0;

        EnumWindows(&EnumGameWindows, 0);
        if (InterlockedCompareExchange(&g_windowCount, 0, 0) == 0)
        {
            Log("error", "no Crimson Desert game window found");
            RemoveDetour(g_modeDetour);
            return 0;
        }

        g_ready = true;
        Log("note", "READY - Ctrl+F1 toggles Private Storage");
        return 0;
    }

    static void Shutdown(bool processTerminating)
    {
        g_stop = true;
        g_ready = false;

        if (!processTerminating)
        {
            RestoreWindows();
            RemoveDetour(g_modeDetour);
        }

        if (g_log)
        {
            std::fclose(g_log);
            g_log = nullptr;
        }

        if (g_logCsInit)
        {
            DeleteCriticalSection(&g_logCs);
            g_logCsInit = false;
        }
    }
}

extern "C"
{
    __declspec(dllexport) int DLPS_ApiVersion()
    {
        return 1;
    }

    __declspec(dllexport) int DLPS_Ready()
    {
        return dlps::g_ready.load() ? 1 : 0;
    }

    __declspec(dllexport) int DLPS_IsOpen()
    {
        return dlps::g_open.load() ? 1 : 0;
    }

    __declspec(dllexport) int DLPS_TogglePrivateStorage()
    {
        if (!dlps::g_ready.load()) return 0;
        dlps::g_toggleRequest = true;
        return 1;
    }

    __declspec(dllexport) int DLPS_OpenPrivateStorage()
    {
        if (!dlps::g_ready.load()) return 0;
        if (!dlps::g_open.load()) dlps::g_toggleRequest = true;
        return 1;
    }

    __declspec(dllexport) int DLPS_ClosePrivateStorage()
    {
        if (!dlps::g_ready.load()) return 0;
        dlps::g_closeRequest = true;
        return 1;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        dlps::g_self = module;

        dlps::g_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if (!dlps::g_base) return TRUE;

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(dlps::g_base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return TRUE;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
            dlps::g_base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return TRUE;

        dlps::g_imageSize = nt->OptionalHeader.SizeOfImage;
        if (dlps::g_imageSize < 64ull * 1024ull * 1024ull)
            return TRUE;

        dlps::InitLog();
        dlps::Log("note", "DesertLink Private Storage original runtime starting");

        HANDLE h = CreateThread(nullptr, 0, &dlps::Worker, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        dlps::Shutdown(reserved != nullptr);
    }

    return TRUE;
}
