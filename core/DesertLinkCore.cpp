// DesertLinkCore.asi - Crimson Desert 2.01.x in-process teleport bridge
// Clean architecture: no PowerShell, no OpenProcess, no WriteProcessMemory,
// no external process injection. Loaded by the user's existing ASI loader.
// IPC: local Windows named pipe \\.\pipe\DesertLinkCore-v1

extern "C" {

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;
typedef signed int     i32;
typedef signed long long i64;
typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned short WORD;
typedef unsigned long long SIZE_T;
typedef void* HANDLE;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef const char* LPCSTR;

#define WINAPI __stdcall
#define DLL_PROCESS_ATTACH 1
#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE ((HANDLE)(i64)-1)

#define PAGE_NOACCESS 0x01
#define PAGE_GUARD 0x100
#define PAGE_EXECUTE_READWRITE 0x40
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

typedef DWORD (WINAPI *ThreadProc)(LPVOID);

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

// The pseudo-handle -1 is GetCurrentProcess().
static HANDLE CurrentProcessPseudoHandle() { return (HANDLE)(i64)-1; }

void* memcpy(void* d, const void* s, SIZE_T n) {
    u8* dd=(u8*)d; const u8* ss=(const u8*)s;
    for (SIZE_T i=0;i<n;i++) dd[i]=ss[i];
    return d;
}
void* memset(void* d, int v, SIZE_T n) {
    u8* dd=(u8*)d; for (SIZE_T i=0;i<n;i++) dd[i]=(u8)v; return d;
}
int memcmp(const void* a, const void* b, SIZE_T n) {
    const u8* aa=(const u8*)a; const u8* bb=(const u8*)b;
    for (SIZE_T i=0;i<n;i++) { if (aa[i]!=bb[i]) return (int)aa[i]-(int)bb[i]; }
    return 0;
}

int _fltused = 0;

}

static const char* PIPE_NAME = "\\\\.\\pipe\\DesertLinkCore-v1";
static const u64 PreferredHookRva = 0x418FD8FULL;

static const u8 HookSignature[8] = { 0x0F,0x28,0xC6,0xF3,0x45,0x0F,0x5C,0xC8 };
static const u8 DirectOriginal[17] = {
    0x41,0x0F,0x58,0x45,0x00,
    0x41,0x0F,0x11,0x45,0x00,
    0x48,0x8B,0xBB,0xF8,0x00,0x00,0x00
};
static const u8 FF25[6] = {0xFF,0x25,0,0,0,0};

static u8* g_moduleBase = 0;
static u32 g_moduleSize = 0;
static u8* g_hookAddress = 0;
static u8* g_patchAddress = 0;
static u8* g_cave = 0;
static volatile u64* g_capture = 0;
static u64 g_downstream = 0;
static u64 g_chainTarget = 0;
static bool g_hookInstalled = false;
static bool g_supportedBuild = false;
static int g_mode = 0; // 1 telemetry, 2 direct, 3 chain-e9, 4 chain-ff25
static char g_message[160] = "Waiting for 2.01 physics hook";

static void setMessage(const char* s) {
    if (!s) s="";
    int i=0; for (; i<159 && s[i]; ++i) g_message[i]=s[i];
    g_message[i]=0;
}

static int slen(const char* s) { int n=0; if(s) while(s[n]) ++n; return n; }
static bool starts(const char* s, const char* p) { int i=0; while(p[i]) { if(s[i]!=p[i]) return false; ++i; } return true; }
static bool bytesEq(const u8* a, const u8* b, int n) { for(int i=0;i<n;i++) if(a[i]!=b[i]) return false; return true; }
static u64 readU64(const u8* p) { u64 v=0; for(int i=0;i<8;i++) v |= ((u64)p[i]) << (i*8); return v; }
static i32 readI32(const u8* p) { u32 v=(u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24); return (i32)v; }
static void writeU64(u8* p, u64 v) { for(int i=0;i<8;i++) p[i]=(u8)(v>>(i*8)); }
static void writeI32(u8* p, i32 v) { u32 x=(u32)v; p[0]=(u8)x;p[1]=(u8)(x>>8);p[2]=(u8)(x>>16);p[3]=(u8)(x>>24); }

static void getExeImage() {
    // The first module in the PEB loader list is the executable. Instead of
    // enumerating modules, DllMain's host image can be found via the PEB.
    // We avoid process-opening APIs entirely.
    extern unsigned __int64 __readgsqword(unsigned long);
    #pragma intrinsic(__readgsqword)
    u8* peb=(u8*)__readgsqword(0x60);
    if (!peb) return;
    u8* ldr=*(u8**)(peb+0x18);
    if (!ldr) return;
    u8* head=ldr+0x10; // InLoadOrderModuleList
    u8* first=*(u8**)head;
    if (!first || first==head) return;
    u8* entry=first; // InLoadOrderLinks is first field
    g_moduleBase=*(u8**)(entry+0x30);
    if (!g_moduleBase) return;
    if (g_moduleBase[0]!='M' || g_moduleBase[1]!='Z') { g_moduleBase=0; return; }
    u32 peoff=*(u32*)(g_moduleBase+0x3C);
    u8* nt=g_moduleBase+peoff;
    if (nt[0]!='P'||nt[1]!='E'||nt[2]!=0||nt[3]!=0) { g_moduleBase=0; return; }
    u8* opt=nt+24;
    g_moduleSize=*(u32*)(opt+0x38);
}

static bool ptrWritable(void* p, SIZE_T bytes) {
    if (!p || (u64)p < 0x10000ULL) return false;
    MEMORY_BASIC_INFORMATION_X64 mbi;
    memset(&mbi,0,sizeof(mbi));
    SIZE_T got=VirtualQuery(p,&mbi,sizeof(mbi));
    if (got < sizeof(mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if ((mbi.Protect & PAGE_GUARD) || (mbi.Protect & PAGE_NOACCESS)) return false;
    u64 start=(u64)p, end=start+(u64)bytes;
    u64 regionEnd=(u64)mbi.BaseAddress + (u64)mbi.RegionSize;
    return end >= start && end <= regionEnd;
}

static bool patchBytes(u8* address, const u8* data, SIZE_T len) {
    DWORD oldp=0;
    if (!VirtualProtect(address,len,PAGE_EXECUTE_READWRITE,&oldp)) return false;
    memcpy(address,data,len);
    DWORD ignored=0; VirtualProtect(address,len,oldp,&ignored);
    FlushInstructionCache(CurrentProcessPseudoHandle(),address,len);
    return true;
}

static bool patchPointer(u8* address, u64 value) {
    DWORD oldp=0;
    if (!VirtualProtect(address,8,PAGE_EXECUTE_READWRITE,&oldp)) return false;
    writeU64(address,value);
    DWORD ignored=0; VirtualProtect(address,8,oldp,&ignored);
    FlushInstructionCache(CurrentProcessPseudoHandle(),address,8);
    return true;
}

static void writeAbsJump(u8* out, u64 target) {
    out[0]=0xFF; out[1]=0x25; out[2]=out[3]=out[4]=out[5]=0;
    writeU64(out+6,target);
}

static u8* allocCaveAny() {
    return (u8*)VirtualAlloc(0,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
}

static u8* allocCaveNear(u8* nearAddress) {
    const u64 GRAN=0x10000ULL;
    const u64 MAXDIST=0x70000000ULL;
    u64 near=(u64)nearAddress;
    u64 moduleEnd=((u64)g_moduleBase + (u64)g_moduleSize + GRAN-1) & ~(GRAN-1);
    u64 maxAddr=near+MAXDIST;
    for (u64 a=moduleEnd; a<maxAddr; a+=GRAN) {
        u8* c=(u8*)VirtualAlloc((LPVOID)a,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
        if (c) {
            i64 rel=(i64)(u64)c - ((i64)near + 5);
            if (rel >= -2147483648LL && rel <= 2147483647LL) return c;
            VirtualFree(c,0,MEM_RELEASE);
        }
        if (a > maxAddr-GRAN) break;
    }
    u64 moduleStart=(u64)g_moduleBase & ~(GRAN-1);
    u64 minAddr=(near>MAXDIST)?near-MAXDIST:GRAN;
    if (moduleStart>GRAN) {
        u64 a=moduleStart-GRAN;
        while (a>=minAddr) {
            u8* c=(u8*)VirtualAlloc((LPVOID)a,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
            if (c) {
                i64 rel=(i64)(u64)c - ((i64)near + 5);
                if (rel >= -2147483648LL && rel <= 2147483647LL) return c;
                VirtualFree(c,0,MEM_RELEASE);
            }
            if (a < minAddr+GRAN) break;
            a-=GRAN;
        }
    }
    return 0;
}

static int buildPiggyback(u8* out, u64 capture, u64 next) {
    int n=0;
    out[n++]=0x50; // push rax
    out[n++]=0x48; out[n++]=0xB8; writeU64(out+n,capture); n+=8; // mov rax,capture
    out[n++]=0x4C; out[n++]=0x89; out[n++]=0x28; // mov [rax],r13
    out[n++]=0x58; // pop rax
    writeAbsJump(out+n,next); n+=14;
    return n;
}

static int buildDirect(u8* out, u64 capture, const u8* original, int olen, u64 ret) {
    int n=0;
    out[n++]=0x50;
    out[n++]=0x48; out[n++]=0xB8; writeU64(out+n,capture); n+=8;
    out[n++]=0x4C; out[n++]=0x89; out[n++]=0x28;
    out[n++]=0x58;
    memcpy(out+n,original,olen); n+=olen;
    writeAbsJump(out+n,ret); n+=14;
    return n;
}

static bool hookStillOurs() {
    if (!g_hookInstalled || !g_patchAddress || !g_cave) return false;
    if (g_mode==3) {
        if (g_patchAddress[0]!=0xE9) return false;
        i32 rel=readI32(g_patchAddress+1);
        u64 dst=(u64)g_patchAddress+5+(i64)rel;
        return dst==(u64)g_cave;
    }
    if (!bytesEq(g_patchAddress,FF25,6)) return false;
    return readU64(g_patchAddress+6)==(u64)g_cave;
}

static bool physicsReady() {
    if (!g_hookInstalled || !g_capture) return false;
    u64 p=*g_capture;
    return p>0x10000ULL && ptrWritable((void*)p,12);
}

static bool detectLayout(int* mode, u64* target) {
    *mode=0; *target=0;
    if (!g_moduleBase) getExeImage();
    if (!g_moduleBase || g_moduleSize < PreferredHookRva+64) {
        setMessage("Crimson Desert image not ready"); return false;
    }
    u8* p=g_moduleBase+PreferredHookRva;
    g_hookAddress=p;

    // Existing 5-byte relative jump (common flight/physics mod).
    if (p[0]==0xE9) {
        i32 rel=readI32(p+1);
        u64 t=(u64)p+5+(i64)rel;
        if (t>0x10000ULL) { *mode=3; *target=t; return true; }
    }
    // Existing FF25 absolute jump at the exact hook location.
    if (bytesEq(p,FF25,6)) {
        u64 t=readU64(p+6);
        if (t>0x10000ULL) { *mode=4; *target=t; return true; }
    }
    // Telemetry owns the slot after the original 8 bytes.
    if (bytesEq(p,HookSignature,8) && bytesEq(p+8,FF25,6)) {
        u64 t=readU64(p+14);
        if (t>0x10000ULL) { *mode=1; *target=t; return true; }
    }
    // Vanilla exact 2.01 bytes.
    if (bytesEq(p,HookSignature,8) && bytesEq(p+8,DirectOriginal,17)) {
        *mode=2; return true;
    }
    setMessage("Unsupported physics layout on this build");
    return false;
}

static bool installHook() {
    if (g_hookInstalled && hookStillOurs()) {
        g_supportedBuild=true;
        if (physicsReady()) {
            if(g_mode==1) setMessage("Ready - Telemetry piggyback");
            else if(g_mode==2) setMessage("Ready - direct physics hook");
            else if(g_mode==3) setMessage("Ready - chained existing E9 physics hook");
            else setMessage("Ready - chained existing FF25 physics hook");
        } else {
            setMessage("Hook ready - move character once");
        }
        return true;
    }

    int mode=0; u64 target=0;
    if (!detectLayout(&mode,&target)) { g_supportedBuild=false; return false; }
    g_supportedBuild=true;

    u8* cave=(mode==3)?allocCaveNear(g_hookAddress):allocCaveAny();
    if (!cave) { setMessage("Could not allocate teleport trampoline"); return false; }
    memset(cave,0,0x1000);
    volatile u64* capture=(volatile u64*)(cave+0x100);
    *capture=0;
    int codeLen=0;
    u8 code[96]; memset(code,0x90,sizeof(code));

    bool ok=false;
    if (mode==1) {
        g_patchAddress=g_hookAddress+8;
        codeLen=buildPiggyback(code,(u64)capture,target);
        memcpy(cave,code,codeLen);
        FlushInstructionCache(CurrentProcessPseudoHandle(),cave,codeLen);
        ok=patchPointer(g_patchAddress+6,(u64)cave);
        if(ok) setMessage("Telemetry hook linked - move character once");
    } else if (mode==2) {
        g_patchAddress=g_hookAddress+8;
        codeLen=buildDirect(code,(u64)capture,DirectOriginal,17,(u64)g_patchAddress+17);
        memcpy(cave,code,codeLen);
        FlushInstructionCache(CurrentProcessPseudoHandle(),cave,codeLen);
        u8 patch[17]; memset(patch,0x90,sizeof(patch)); writeAbsJump(patch,(u64)cave);
        ok=patchBytes(g_patchAddress,patch,17);
        if(ok) setMessage("Direct hook installed - move character once");
    } else if (mode==4) {
        g_patchAddress=g_hookAddress;
        codeLen=buildPiggyback(code,(u64)capture,target);
        memcpy(cave,code,codeLen);
        FlushInstructionCache(CurrentProcessPseudoHandle(),cave,codeLen);
        ok=patchPointer(g_patchAddress+6,(u64)cave);
        if(ok) setMessage("Ready - chained existing FF25 physics hook");
    } else if (mode==3) {
        g_patchAddress=g_hookAddress;
        codeLen=buildPiggyback(code,(u64)capture,target);
        memcpy(cave,code,codeLen);
        FlushInstructionCache(CurrentProcessPseudoHandle(),cave,codeLen);
        i64 rel64=(i64)(u64)cave - ((i64)(u64)g_patchAddress+5);
        if(rel64>=-2147483648LL && rel64<=2147483647LL) {
            u8 patch[5]; patch[0]=0xE9; writeI32(patch+1,(i32)rel64);
            ok=patchBytes(g_patchAddress,patch,5);
        }
        if(ok) setMessage("Ready - chained existing E9 physics hook");
    }

    if (!ok) {
        VirtualFree(cave,0,MEM_RELEASE);
        g_cave=0; g_capture=0; g_hookInstalled=false; g_mode=0;
        setMessage("Failed to install safe teleport hook");
        return false;
    }
    g_cave=cave; g_capture=capture; g_downstream=target; g_chainTarget=target;
    g_mode=mode; g_hookInstalled=true;
    return true;
}

static bool finiteD(double d) { return d==d && d>-100000000.0 && d<100000000.0; }

static bool teleport(double cx,double cy,double cz,double tx,double ty,double tz,const char** error) {
    if (!installHook()) { *error=g_message; return false; }
    if (!physicsReady()) { *error="Waiting for physics pointer - move character and try again"; return false; }
    if(!finiteD(cx)||!finiteD(cy)||!finiteD(cz)||!finiteD(tx)||!finiteD(ty)||!finiteD(tz)) { *error="Invalid teleport coordinates"; return false; }
    u64 pp=*g_capture;
    if (!ptrWritable((void*)pp,12)) { *error="Captured physics vector is no longer writable"; return false; }
    float* p=(float*)pp;
    float lx=p[0], ly=p[1], lz=p[2];
    if (!(lx==lx) || !(ly==ly) || !(lz==lz)) { *error="Captured physics vector is invalid"; return false; }
    double ox=cx-(double)lx, oy=cy-(double)ly, oz=cz-(double)lz;
    float nx=(float)(tx-ox), ny=(float)(ty-oy), nz=(float)(tz-oz);
    // A 12-byte vector write is intentionally local/in-process; the app never opens the game process.
    p[0]=nx; p[1]=ny; p[2]=nz;
    setMessage(g_mode==1?"Ready - Telemetry piggyback":g_mode==2?"Ready - direct physics hook":g_mode==3?"Ready - chained existing E9 physics hook":"Ready - chained existing FF25 physics hook");
    *error="";
    return true;
}

struct Buf { char* p; int cap; int n; };
static void bch(Buf* b,char c){ if(b->n<b->cap-1)b->p[b->n++]=c; }
static void bstr(Buf* b,const char* s){ if(!s)return; for(int i=0;s[i];i++)bch(b,s[i]); }
static void bbool(Buf* b,bool v){ bch(b,v?'1':'0'); }
static void bend(Buf* b){ if(b->n<b->cap)b->p[b->n]=0; }

static const char* modeName() {
    if(g_mode==1)return "telemetry"; if(g_mode==2)return "direct"; if(g_mode==3)return "chain-e9"; if(g_mode==4)return "chain-ff25"; return "none";
}

static void writeLine(HANDLE h,const char* s) {
    DWORD wrote=0; WriteFile(h,s,(DWORD)slen(s),&wrote,0);
}

static void sendStatus(HANDLE h) {
    installHook();
    char out[512]; Buf b={out,512,0};
    bstr(&b,"S|"); bbool(&b,g_hookInstalled); bch(&b,'|'); bbool(&b,physicsReady()); bch(&b,'|'); bbool(&b,g_supportedBuild); bch(&b,'|'); bstr(&b,modeName()); bch(&b,'|'); bstr(&b,g_message); bch(&b,'\n'); bend(&b);
    writeLine(h,out);
}

static bool isSpace(char c){return c==' '||c=='\t'||c=='\r'||c=='\n';}
static const char* skipSpace(const char* p){while(*p&&isSpace(*p))++p;return p;}
static bool parseDouble(const char** pp,double* out) {
    const char* p=skipSpace(*pp); bool neg=false; if(*p=='-'||*p=='+'){neg=*p=='-';++p;}
    bool any=false; double v=0.0;
    while(*p>='0'&&*p<='9'){any=true;v=v*10.0+(*p-'0');++p;}
    if(*p=='.'){++p;double place=0.1;while(*p>='0'&&*p<='9'){any=true;v+=(*p-'0')*place;place*=0.1;++p;}}
    if(!any)return false;
    if(*p=='e'||*p=='E'){++p;bool eneg=false;if(*p=='-'||*p=='+'){eneg=*p=='-';++p;}int e=0;bool eany=false;while(*p>='0'&&*p<='9'){eany=true;e=e*10+(*p-'0');if(e>308)e=308;++p;}if(!eany)return false;double m=1.0;for(int i=0;i<e;i++)m*=10.0;if(eneg)v/=m;else v*=m;}
    *out=neg?-v:v; *pp=p; return true;
}

static void handleCommand(HANDLE h, char* line) {
    if (starts(line,"ENSURE") || starts(line,"STATUS")) { sendStatus(h); return; }
    if (starts(line,"PING")) { writeLine(h,"PONG\n"); return; }
    if (starts(line,"TELEPORT")) {
        const char* p=line+8; double v[6];
        for(int i=0;i<6;i++){ if(!parseDouble(&p,&v[i])) { writeLine(h,"T|0|Invalid teleport command\n"); return; } }
        const char* err=""; bool ok=teleport(v[0],v[1],v[2],v[3],v[4],v[5],&err);
        char out[384]; Buf b={out,384,0}; bstr(&b,"T|");bbool(&b,ok);bch(&b,'|');bstr(&b,err);bch(&b,'\n');bend(&b);writeLine(h,out);sendStatus(h);return;
    }
    writeLine(h,"E|Unknown command\n");
}

static DWORD WINAPI CoreThread(LPVOID) {
    getExeImage();
    // Give other ASIs (especially CrimsonDesertTelemetry) time to install their hooks first.
    Sleep(1500);
    installHook();
    for (;;) {
        HANDLE pipe=CreateNamedPipeA(PIPE_NAME,PIPE_ACCESS_DUPLEX,PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT,1,4096,4096,0,0);
        if(pipe==INVALID_HANDLE_VALUE){Sleep(1000);continue;}
        BOOL connected=ConnectNamedPipe(pipe,0);
        if(!connected && GetLastError()!=ERROR_PIPE_CONNECTED){CloseHandle(pipe);Sleep(250);continue;}
        writeLine(pipe,"HELLO|1.0.6\n");
        sendStatus(pipe);
        char readbuf[1024]; char line[1024]; int ln=0;
        for(;;){
            DWORD got=0; BOOL ok=ReadFile(pipe,readbuf,sizeof(readbuf),&got,0);
            if(!ok||got==0)break;
            for(DWORD i=0;i<got;i++){
                char c=readbuf[i];
                if(c=='\n') { line[ln]=0; if(ln>0)handleCommand(pipe,line); ln=0; }
                else if(c!='\r' && ln<(int)sizeof(line)-1) line[ln++]=c;
            }
        }
        DisconnectNamedPipe(pipe); CloseHandle(pipe);
    }
    return 0;
}

extern "C" __declspec(dllexport) int DesertLinkCoreVersion(){ return 106; }
extern "C" BOOL WINAPI DllMain(void*, DWORD reason, void*) {
    if(reason==DLL_PROCESS_ATTACH) {
        HANDLE t=CreateThread(0,0,CoreThread,0,0,0);
        if(t) CloseHandle(t);
    }
    return TRUE;
}
