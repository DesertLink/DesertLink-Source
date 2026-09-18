// DesertLinkCore.asi - Crimson Desert 2.02.00 teleport bridge
// Target: CrimsonDesert.exe 1.0.0.2850, Steam build 25246367
// Local IPC only. No external process access and no remote networking.

extern "C" {

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed int i32;
typedef signed long long i64;
typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned short WORD;
typedef unsigned long long SIZE_T;
typedef void* HANDLE;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef const char* LPCSTR;
typedef const wchar_t* LPCWSTR;
typedef wchar_t* LPWSTR;

typedef DWORD (__stdcall *ThreadProc)(LPVOID);

#define WINAPI __stdcall
#define DLL_PROCESS_ATTACH 1
#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE ((HANDLE)(i64)-1)

#define PAGE_NOACCESS 0x01
#define PAGE_READONLY 0x02
#define PAGE_READWRITE 0x04
#define PAGE_WRITECOPY 0x08
#define PAGE_EXECUTE 0x10
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_WRITECOPY 0x80
#define PAGE_GUARD 0x100
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_RELEASE 0x8000

#define PIPE_ACCESS_DUPLEX 0x00000003
#define PIPE_TYPE_BYTE 0x00000000
#define PIPE_READMODE_BYTE 0x00000000
#define PIPE_WAIT 0x00000000
#define ERROR_PIPE_CONNECTED 535

struct MEMORY_BASIC_INFORMATION_X64 {
    LPVOID BaseAddress;
    LPVOID AllocationBase;
    DWORD AllocationProtect;
    WORD PartitionId;
    WORD __pad0;
    SIZE_T RegionSize;
    DWORD State;
    DWORD Protect;
    DWORD Type;
    DWORD __pad1;
};

struct VS_FIXEDFILEINFO_X {
    DWORD dwSignature;
    DWORD dwStrucVersion;
    DWORD dwFileVersionMS;
    DWORD dwFileVersionLS;
    DWORD dwProductVersionMS;
    DWORD dwProductVersionLS;
    DWORD dwFileFlagsMask;
    DWORD dwFileFlags;
    DWORD dwFileOS;
    DWORD dwFileType;
    DWORD dwFileSubtype;
    DWORD dwFileDateMS;
    DWORD dwFileDateLS;
};

__declspec(dllimport) HANDLE WINAPI CreateThread(LPVOID, SIZE_T, ThreadProc, LPVOID, DWORD, DWORD*);
__declspec(dllimport) BOOL WINAPI CloseHandle(HANDLE);
__declspec(dllimport) void WINAPI Sleep(DWORD);
__declspec(dllimport) LPVOID WINAPI VirtualAlloc(LPVOID, SIZE_T, DWORD, DWORD);
__declspec(dllimport) BOOL WINAPI VirtualFree(LPVOID, SIZE_T, DWORD);
__declspec(dllimport) BOOL WINAPI VirtualProtect(LPVOID, SIZE_T, DWORD, DWORD*);
__declspec(dllimport) SIZE_T WINAPI VirtualQuery(LPCVOID, MEMORY_BASIC_INFORMATION_X64*, SIZE_T);
__declspec(dllimport) BOOL WINAPI FlushInstructionCache(HANDLE, LPCVOID, SIZE_T);
__declspec(dllimport) HANDLE WINAPI CreateNamedPipeA(LPCSTR, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPVOID);
__declspec(dllimport) BOOL WINAPI ConnectNamedPipe(HANDLE, LPVOID);
__declspec(dllimport) BOOL WINAPI DisconnectNamedPipe(HANDLE);
__declspec(dllimport) BOOL WINAPI ReadFile(HANDLE, LPVOID, DWORD, DWORD*, LPVOID);
__declspec(dllimport) BOOL WINAPI WriteFile(HANDLE, LPCVOID, DWORD, DWORD*, LPVOID);
__declspec(dllimport) DWORD WINAPI GetLastError();
__declspec(dllimport) DWORD WINAPI GetModuleFileNameW(HANDLE, LPWSTR, DWORD);

__declspec(dllimport) DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR, DWORD*);
__declspec(dllimport) BOOL WINAPI GetFileVersionInfoW(LPCWSTR, DWORD, DWORD, LPVOID);
__declspec(dllimport) BOOL WINAPI VerQueryValueW(LPCVOID, LPCWSTR, LPVOID*, unsigned int*);

void* memcpy(void* d, const void* s, SIZE_T n) {
    u8* dd = (u8*)d;
    const u8* ss = (const u8*)s;
    for (SIZE_T i = 0; i < n; ++i) dd[i] = ss[i];
    return d;
}

void* memset(void* d, int v, SIZE_T n) {
    u8* dd = (u8*)d;
    for (SIZE_T i = 0; i < n; ++i) dd[i] = (u8)v;
    return d;
}

int memcmp(const void* a, const void* b, SIZE_T n) {
    const u8* aa = (const u8*)a;
    const u8* bb = (const u8*)b;
    for (SIZE_T i = 0; i < n; ++i) {
        if (aa[i] != bb[i]) return (int)aa[i] - (int)bb[i];
    }
    return 0;
}

int _fltused = 0;

}

static const char* PIPE_NAME = "\\\\.\\pipe\\DesertLinkCore-v1";
static const u32 TARGET_TIMESTAMP = 0x6AA22ABB;
static const u32 TARGET_IMAGE_SIZE = 0x16B0E000;
static const u16 TARGET_VERSION[4] = { 1, 0, 0, 2850 };

static const u8 HOOK_SIGNATURE[8] = {
    0x0F, 0x28, 0xC6, 0xF3, 0x45, 0x0F, 0x5C, 0xC8
};

static const u8 DIRECT_ORIGINAL[17] = {
    0x41, 0x0F, 0x58, 0x45, 0x00,
    0x41, 0x0F, 0x11, 0x45, 0x00,
    0x48, 0x8B, 0xBB, 0xF8, 0x00, 0x00, 0x00
};

static const u8 FF25[6] = { 0xFF, 0x25, 0, 0, 0, 0 };

static u8* g_moduleBase = 0;
static u32 g_moduleSize = 0;
static u32 g_timestamp = 0;
static u8* g_hookAddress = 0;
static u8* g_patchAddress = 0;
static u8* g_cave = 0;
static volatile u64* g_capture = 0;
static u64 g_downstream = 0;
static bool g_supportedBuild = false;
static bool g_hookInstalled = false;
static int g_mode = 0; // 1 = coexist with FF25 hook, 2 = vanilla/direct
static char g_message[192] = "Waiting for physics hook";

static HANDLE CurrentProcessPseudoHandle() { return (HANDLE)(i64)-1; }

static int slen(const char* s) {
    int n = 0;
    if (s) while (s[n]) ++n;
    return n;
}

static bool starts(const char* s, const char* p) {
    int i = 0;
    while (p[i]) {
        if (s[i] != p[i]) return false;
        ++i;
    }
    return true;
}

static bool bytesEq(const u8* a, const u8* b, int n) {
    for (int i = 0; i < n; ++i) if (a[i] != b[i]) return false;
    return true;
}

static u64 readU64(const u8* p) {
    u64 v = 0;
    for (int i = 0; i < 8; ++i) v |= ((u64)p[i]) << (i * 8);
    return v;
}

static void writeU64(u8* p, u64 v) {
    for (int i = 0; i < 8; ++i) p[i] = (u8)(v >> (i * 8));
}

static void setMessage(const char* s) {
    if (!s) s = "";
    int i = 0;
    for (; i < 191 && s[i]; ++i) g_message[i] = s[i];
    g_message[i] = 0;
}

static bool isExecutableProtect(DWORD p) {
    p &= 0xFF;
    return p == PAGE_EXECUTE || p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
}

static void getExeImage() {
    extern unsigned __int64 __readgsqword(unsigned long);
    #pragma intrinsic(__readgsqword)

    u8* peb = (u8*)__readgsqword(0x60);
    if (!peb) return;
    u8* ldr = *(u8**)(peb + 0x18);
    if (!ldr) return;
    u8* head = ldr + 0x10;
    u8* first = *(u8**)head;
    if (!first || first == head) return;

    g_moduleBase = *(u8**)(first + 0x30);
    if (!g_moduleBase) return;
    if (g_moduleBase[0] != 'M' || g_moduleBase[1] != 'Z') { g_moduleBase = 0; return; }

    u32 peoff = *(u32*)(g_moduleBase + 0x3C);
    u8* nt = g_moduleBase + peoff;
    if (nt[0] != 'P' || nt[1] != 'E' || nt[2] != 0 || nt[3] != 0) { g_moduleBase = 0; return; }

    g_timestamp = *(u32*)(nt + 8);
    u8* opt = nt + 24;
    g_moduleSize = *(u32*)(opt + 0x38);
}

static bool getExeVersion(u16 out[4]) {
    wchar_t path[1024];
    memset(path, 0, sizeof(path));
    DWORD chars = GetModuleFileNameW(0, path, 1023);
    if (!chars || chars >= 1023) return false;

    DWORD dummy = 0;
    DWORD bytes = GetFileVersionInfoSizeW(path, &dummy);
    if (!bytes || bytes > 65536) return false;

    u8* buffer = (u8*)VirtualAlloc(0, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buffer) return false;

    bool ok = false;
    if (GetFileVersionInfoW(path, 0, bytes, buffer)) {
        LPVOID ptr = 0;
        unsigned int len = 0;
        static const wchar_t ROOT[] = L"\\";
        if (VerQueryValueW(buffer, ROOT, &ptr, &len) && ptr && len >= sizeof(VS_FIXEDFILEINFO_X)) {
            VS_FIXEDFILEINFO_X* v = (VS_FIXEDFILEINFO_X*)ptr;
            if (v->dwSignature == 0xFEEF04BD) {
                out[0] = (u16)(v->dwFileVersionMS >> 16);
                out[1] = (u16)(v->dwFileVersionMS & 0xFFFF);
                out[2] = (u16)(v->dwFileVersionLS >> 16);
                out[3] = (u16)(v->dwFileVersionLS & 0xFFFF);
                ok = true;
            }
        }
    }

    VirtualFree(buffer, 0, MEM_RELEASE);
    return ok;
}

static bool validateBuild() {
    if (!g_moduleBase) getExeImage();
    if (!g_moduleBase || g_moduleSize != TARGET_IMAGE_SIZE || g_timestamp != TARGET_TIMESTAMP) {
        g_supportedBuild = false;
        setMessage("Unsupported Crimson Desert build");
        return false;
    }

    u16 version[4] = {};
    if (!getExeVersion(version)) {
        g_supportedBuild = false;
        setMessage("Could not verify Crimson Desert version");
        return false;
    }

    for (int i = 0; i < 4; ++i) {
        if (version[i] != TARGET_VERSION[i]) {
            g_supportedBuild = false;
            setMessage("Unsupported Crimson Desert version");
            return false;
        }
    }

    g_supportedBuild = true;
    return true;
}

static bool ptrWritable(void* p, SIZE_T bytes) {
    if (!p || (u64)p < 0x10000ULL) return false;
    MEMORY_BASIC_INFORMATION_X64 mbi;
    memset(&mbi, 0, sizeof(mbi));
    SIZE_T got = VirtualQuery(p, &mbi, sizeof(mbi));
    if (got < sizeof(mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if ((mbi.Protect & PAGE_GUARD) || (mbi.Protect & PAGE_NOACCESS)) return false;
    u64 start = (u64)p;
    u64 end = start + (u64)bytes;
    u64 regionEnd = (u64)mbi.BaseAddress + (u64)mbi.RegionSize;
    return end >= start && end <= regionEnd;
}

static bool patchBytes(u8* address, const u8* data, SIZE_T len) {
    DWORD oldp = 0;
    if (!VirtualProtect(address, len, PAGE_EXECUTE_READWRITE, &oldp)) return false;
    memcpy(address, data, len);
    DWORD ignored = 0;
    VirtualProtect(address, len, oldp, &ignored);
    FlushInstructionCache(CurrentProcessPseudoHandle(), address, len);
    return true;
}

static bool patchPointer(u8* address, u64 value) {
    DWORD oldp = 0;
    if (!VirtualProtect(address, 8, PAGE_EXECUTE_READWRITE, &oldp)) return false;
    writeU64(address, value);
    DWORD ignored = 0;
    VirtualProtect(address, 8, oldp, &ignored);
    FlushInstructionCache(CurrentProcessPseudoHandle(), address, 8);
    return true;
}

static void writeAbsJump(u8* out, u64 target) {
    out[0] = 0xFF;
    out[1] = 0x25;
    out[2] = out[3] = out[4] = out[5] = 0;
    writeU64(out + 6, target);
}

static int buildCaptureThenJump(u8* out, u64 capture, u64 next) {
    int n = 0;
    out[n++] = 0x50;                                      // push rax
    out[n++] = 0x48; out[n++] = 0xB8;                    // mov rax, imm64
    writeU64(out + n, capture); n += 8;
    out[n++] = 0x4C; out[n++] = 0x89; out[n++] = 0x28;    // mov [rax], r13
    out[n++] = 0x58;                                      // pop rax
    writeAbsJump(out + n, next); n += 14;
    return n;
}

static int buildDirectCapture(u8* out, u64 capture, u8* returnAddress) {
    int n = 0;
    out[n++] = 0x50;
    out[n++] = 0x48; out[n++] = 0xB8;
    writeU64(out + n, capture); n += 8;
    out[n++] = 0x4C; out[n++] = 0x89; out[n++] = 0x28;
    out[n++] = 0x58;
    memcpy(out + n, DIRECT_ORIGINAL, sizeof(DIRECT_ORIGINAL)); n += (int)sizeof(DIRECT_ORIGINAL);
    writeAbsJump(out + n, (u64)returnAddress); n += 14;
    return n;
}

static u8* scanHookSignature(int* count) {
    *count = 0;
    u8* first = 0;
    if (!g_moduleBase || g_moduleSize < 64) return 0;

    u8* moduleEnd = g_moduleBase + g_moduleSize;
    u8* cursor = g_moduleBase;

    while (cursor < moduleEnd) {
        MEMORY_BASIC_INFORMATION_X64 mbi;
        memset(&mbi, 0, sizeof(mbi));
        if (VirtualQuery(cursor, &mbi, sizeof(mbi)) < sizeof(mbi)) break;

        u8* regionStart = (u8*)mbi.BaseAddress;
        u8* regionEnd = regionStart + mbi.RegionSize;
        if (regionEnd > moduleEnd) regionEnd = moduleEnd;

        if (mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_GUARD) && !(mbi.Protect & PAGE_NOACCESS) && isExecutableProtect(mbi.Protect)) {
            u8* start = regionStart < g_moduleBase ? g_moduleBase : regionStart;
            if (regionEnd > start + sizeof(HOOK_SIGNATURE)) {
                for (u8* p = start; p + 8 + 17 <= regionEnd; ++p) {
                    if (bytesEq(p, HOOK_SIGNATURE, 8)) {
                        ++(*count);
                        if (!first) first = p;
                        if (*count > 1) return first;
                    }
                }
            }
        }

        if (regionEnd <= cursor) break;
        cursor = regionEnd;
    }

    return first;
}

static bool resolveLayout(int* mode, u64* downstream) {
    *mode = 0;
    *downstream = 0;

    int matches = 0;
    u8* p = scanHookSignature(&matches);
    if (!p || matches != 1) {
        setMessage(matches == 0 ? "Physics hook signature not found" : "Physics hook signature is ambiguous");
        return false;
    }

    g_hookAddress = p;

    if (bytesEq(p + 8, FF25, 6)) {
        u64 target = readU64(p + 14);
        if (target <= 0x10000ULL) {
            setMessage("Existing physics hook target is invalid");
            return false;
        }
        *mode = 1;
        *downstream = target;
        return true;
    }

    if (bytesEq(p + 8, DIRECT_ORIGINAL, (int)sizeof(DIRECT_ORIGINAL))) {
        *mode = 2;
        return true;
    }

    setMessage("Unsupported physics hook layout");
    return false;
}

static bool hookStillOurs() {
    if (!g_hookInstalled || !g_patchAddress || !g_cave) return false;
    if (!bytesEq(g_patchAddress, FF25, 6)) return false;
    return readU64(g_patchAddress + 6) == (u64)g_cave;
}

static bool physicsReady() {
    if (!g_hookInstalled || !g_capture) return false;
    u64 p = *g_capture;
    return p > 0x10000ULL && ptrWritable((void*)p, 12);
}

static bool installHook() {
    if (!validateBuild()) return false;

    if (g_hookInstalled && hookStillOurs()) {
        setMessage(physicsReady() ? "Ready - DesertLink teleport" : "Waiting for physics hook");
        return true;
    }

    int mode = 0;
    u64 downstream = 0;
    if (!resolveLayout(&mode, &downstream)) return false;

    u8* cave = (u8*)VirtualAlloc(0, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!cave) {
        setMessage("Could not allocate teleport trampoline");
        return false;
    }
    memset(cave, 0, 0x1000);

    volatile u64* capture = (volatile u64*)(cave + 0x100);
    *capture = 0;

    u8 code[96];
    memset(code, 0x90, sizeof(code));
    int codeLen = 0;
    bool ok = false;

    if (mode == 1) {
        g_patchAddress = g_hookAddress + 8;
        codeLen = buildCaptureThenJump(code, (u64)capture, downstream);
        memcpy(cave, code, codeLen);
        FlushInstructionCache(CurrentProcessPseudoHandle(), cave, codeLen);
        ok = patchPointer(g_patchAddress + 6, (u64)cave);
    } else if (mode == 2) {
        g_patchAddress = g_hookAddress + 8;
        codeLen = buildDirectCapture(code, (u64)capture, g_patchAddress + sizeof(DIRECT_ORIGINAL));
        memcpy(cave, code, codeLen);
        FlushInstructionCache(CurrentProcessPseudoHandle(), cave, codeLen);
        u8 patch[17];
        memset(patch, 0x90, sizeof(patch));
        writeAbsJump(patch, (u64)cave);
        ok = patchBytes(g_patchAddress, patch, sizeof(patch));
    }

    if (!ok) {
        VirtualFree(cave, 0, MEM_RELEASE);
        g_patchAddress = 0;
        setMessage("Failed to install safe teleport hook");
        return false;
    }

    g_cave = cave;
    g_capture = capture;
    g_downstream = downstream;
    g_mode = mode;
    g_hookInstalled = true;
    setMessage("Waiting for physics hook");
    return true;
}

static bool finiteD(double d) {
    return d == d && d > -100000000.0 && d < 100000000.0;
}

static bool teleport(double cx, double cy, double cz, double tx, double ty, double tz, const char** error) {
    if (!installHook()) { *error = g_message; return false; }
    if (!physicsReady()) { *error = "Physics pointer not ready - move character and try again"; return false; }
    if (!finiteD(cx) || !finiteD(cy) || !finiteD(cz) || !finiteD(tx) || !finiteD(ty) || !finiteD(tz)) {
        *error = "Invalid teleport coordinates";
        return false;
    }

    u64 pp = *g_capture;
    if (!ptrWritable((void*)pp, 12)) {
        *error = "Captured physics vector is no longer writable";
        return false;
    }

    float* p = (float*)pp;
    float lx = p[0], ly = p[1], lz = p[2];
    if (!(lx == lx) || !(ly == ly) || !(lz == lz)) {
        *error = "Captured physics vector is invalid";
        return false;
    }

    double ox = cx - (double)lx;
    double oy = cy - (double)ly;
    double oz = cz - (double)lz;
    p[0] = (float)(tx - ox);
    p[1] = (float)(ty - oy);
    p[2] = (float)(tz - oz);

    setMessage("Ready - DesertLink teleport");
    *error = "";
    return true;
}

struct Buf { char* p; int cap; int n; };
static void bch(Buf* b, char c) { if (b->n < b->cap - 1) b->p[b->n++] = c; }
static void bstr(Buf* b, const char* s) { if (s) for (int i = 0; s[i]; ++i) bch(b, s[i]); }
static void bbool(Buf* b, bool v) { bch(b, v ? '1' : '0'); }
static void bend(Buf* b) { if (b->n < b->cap) b->p[b->n] = 0; }

static const char* modeName() {
    if (g_mode == 1) return "coexist";
    if (g_mode == 2) return "direct";
    return "none";
}

static void writeLine(HANDLE h, const char* s) {
    DWORD wrote = 0;
    WriteFile(h, s, (DWORD)slen(s), &wrote, 0);
}

static void sendStatus(HANDLE h) {
    installHook();
    char out[512];
    Buf b = { out, 512, 0 };
    bstr(&b, "S|");
    bbool(&b, g_hookInstalled);
    bch(&b, '|');
    bbool(&b, physicsReady());
    bch(&b, '|');
    bbool(&b, g_supportedBuild);
    bch(&b, '|');
    bstr(&b, modeName());
    bch(&b, '|');
    bstr(&b, g_message);
    bch(&b, '\n');
    bend(&b);
    writeLine(h, out);
}

static bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static const char* skipSpace(const char* p) { while (*p && isSpace(*p)) ++p; return p; }

static bool parseDouble(const char** pp, double* out) {
    const char* p = skipSpace(*pp);
    bool neg = false;
    if (*p == '-' || *p == '+') { neg = *p == '-'; ++p; }
    bool any = false;
    double v = 0.0;
    while (*p >= '0' && *p <= '9') { any = true; v = v * 10.0 + (*p - '0'); ++p; }
    if (*p == '.') {
        ++p;
        double place = 0.1;
        while (*p >= '0' && *p <= '9') { any = true; v += (*p - '0') * place; place *= 0.1; ++p; }
    }
    if (!any) return false;
    if (*p == 'e' || *p == 'E') {
        ++p;
        bool eneg = false;
        if (*p == '-' || *p == '+') { eneg = *p == '-'; ++p; }
        int e = 0;
        bool eany = false;
        while (*p >= '0' && *p <= '9') { eany = true; e = e * 10 + (*p - '0'); if (e > 308) e = 308; ++p; }
        if (!eany) return false;
        double m = 1.0;
        for (int i = 0; i < e; ++i) m *= 10.0;
        if (eneg) v /= m; else v *= m;
    }
    *out = neg ? -v : v;
    *pp = p;
    return true;
}

static void handleCommand(HANDLE h, char* line) {
    if (starts(line, "ENSURE") || starts(line, "STATUS")) { sendStatus(h); return; }
    if (starts(line, "PING")) { writeLine(h, "PONG\n"); return; }

    if (starts(line, "TELEPORT")) {
        const char* p = line + 8;
        double v[6];
        for (int i = 0; i < 6; ++i) {
            if (!parseDouble(&p, &v[i])) {
                writeLine(h, "T|0|Invalid teleport command\n");
                return;
            }
        }

        const char* err = "";
        bool ok = teleport(v[0], v[1], v[2], v[3], v[4], v[5], &err);
        char out[384];
        Buf b = { out, 384, 0 };
        bstr(&b, "T|"); bbool(&b, ok); bch(&b, '|'); bstr(&b, err); bch(&b, '\n'); bend(&b);
        writeLine(h, out);
        sendStatus(h);
        return;
    }

    writeLine(h, "E|Unknown command\n");
}

static DWORD WINAPI CoreThread(LPVOID) {
    getExeImage();
    if (!validateBuild()) return 0;

    // Allow other ASI plugins to finish their startup before resolving the physics site.
    Sleep(1500);
    installHook();

    for (;;) {
        HANDLE pipe = CreateNamedPipeA(
            PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,
            4096,
            4096,
            0,
            0
        );
        if (pipe == INVALID_HANDLE_VALUE) { Sleep(1000); continue; }

        BOOL connected = ConnectNamedPipe(pipe, 0);
        if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(pipe);
            Sleep(250);
            continue;
        }

        writeLine(pipe, "HELLO|1.2\n");
        sendStatus(pipe);

        char readbuf[1024];
        char line[1024];
        int lineLen = 0;

        for (;;) {
            DWORD got = 0;
            BOOL ok = ReadFile(pipe, readbuf, sizeof(readbuf), &got, 0);
            if (!ok || got == 0) break;

            for (DWORD i = 0; i < got; ++i) {
                char c = readbuf[i];
                if (c == '\n') {
                    line[lineLen] = 0;
                    if (lineLen > 0) handleCommand(pipe, line);
                    lineLen = 0;
                } else if (c != '\r' && lineLen < (int)sizeof(line) - 1) {
                    line[lineLen++] = c;
                }
            }
        }

        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

extern "C" __declspec(dllexport) int DesertLinkCoreVersion() { return 120; }

extern "C" BOOL WINAPI DllMain(HANDLE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DWORD tid = 0;
        HANDLE h = CreateThread(0, 0, CoreThread, 0, 0, &tid);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
