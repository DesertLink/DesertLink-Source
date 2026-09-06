// DesertLink App Setup v1.0.6 PUBLIC - app-only installer
// Installs only the DesertLink desktop application and creates Desktop/Start Menu shortcuts.
// It does NOT install or modify any ASI plugin and performs NO network downloads.
// First-time users manually download the official Electron v44.2.0 Windows x64 ZIP
// from the link in the Nexus description; setup verifies its SHA-256 before use.

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long DWORD;
typedef long LONG;
typedef unsigned long long u64;
typedef unsigned long long ULONG_PTR;
typedef unsigned long long SIZE_T;
typedef int BOOL;
typedef void* HANDLE;
typedef void* HWND;
typedef void* HINSTANCE;
typedef const wchar_t* LPCWSTR;
typedef wchar_t* LPWSTR;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef unsigned short WORD;
typedef long HRESULT;
typedef ULONG_PTR WPARAM;
typedef long long LPARAM;
typedef void* HMODULE;
typedef void* FARPROC;
#define WINAPI __stdcall
#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE ((HANDLE)(long long)-1)
#define MAX_PATH 260
#define GENERIC_READ 0x80000000UL
#define GENERIC_WRITE 0x40000000UL
#define FILE_SHARE_READ 0x00000001UL
#define CREATE_ALWAYS 2UL
#define OPEN_EXISTING 3UL
#define FILE_ATTRIBUTE_NORMAL 0x00000080UL
#define FILE_BEGIN 0UL
#define INFINITE 0xFFFFFFFFUL
#define MOVEFILE_REPLACE_EXISTING 0x1UL
#define CREATE_NO_WINDOW 0x08000000UL
#define MB_OK 0x00000000UL
#define MB_ICONINFORMATION 0x00000040UL
#define MB_ICONERROR 0x00000010UL
#define MB_OKCANCEL 0x00000001UL
#define IDOK 1
#define OFN_FILEMUSTEXIST 0x00001000UL
#define OFN_PATHMUSTEXIST 0x00000800UL
#define OFN_EXPLORER 0x00080000UL
#define OFN_HIDEREADONLY 0x00000004UL
#define CLSCTX_INPROC_SERVER 0x1UL
#define COINIT_APARTMENTTHREADED 0x2UL
#define CSIDL_DESKTOPDIRECTORY 0x0010
#define CSIDL_PROGRAMS 0x0002
#define SHGFP_TYPE_CURRENT 0

struct LARGE_INTEGER_X { long long QuadPart; };
struct STARTUPINFOW_X {
  DWORD cb; LPWSTR lpReserved; LPWSTR lpDesktop; LPWSTR lpTitle;
  DWORD dwX; DWORD dwY; DWORD dwXSize; DWORD dwYSize; DWORD dwXCountChars; DWORD dwYCountChars;
  DWORD dwFillAttribute; DWORD dwFlags; WORD wShowWindow; WORD cbReserved2; u8* lpReserved2;
  HANDLE hStdInput; HANDLE hStdOutput; HANDLE hStdError;
};
struct PROCESS_INFORMATION_X { HANDLE hProcess; HANDLE hThread; DWORD dwProcessId; DWORD dwThreadId; };
struct OPENFILENAMEW_X {
  DWORD lStructSize; HWND hwndOwner; HINSTANCE hInstance; LPCWSTR lpstrFilter; LPWSTR lpstrCustomFilter;
  DWORD nMaxCustFilter; DWORD nFilterIndex; LPWSTR lpstrFile; DWORD nMaxFile; LPWSTR lpstrFileTitle;
  DWORD nMaxFileTitle; LPCWSTR lpstrInitialDir; LPCWSTR lpstrTitle; DWORD Flags; WORD nFileOffset; WORD nFileExtension;
  LPCWSTR lpstrDefExt; LPARAM lCustData; LPVOID lpfnHook; LPCWSTR lpTemplateName; LPVOID pvReserved; DWORD dwReserved; DWORD FlagsEx;
};
struct GUID_X { u32 Data1; u16 Data2; u16 Data3; u8 Data4[8]; };
typedef const GUID_X& REFGUID_X;
typedef const GUID_X& REFIID_X;

struct IUnknown_X;
struct IUnknown_Vtbl {
  HRESULT (WINAPI *QueryInterface)(IUnknown_X*, REFIID_X, void**);
  ULONG_PTR (WINAPI *AddRef)(IUnknown_X*);
  ULONG_PTR (WINAPI *Release)(IUnknown_X*);
};
struct IUnknown_X { IUnknown_Vtbl* lpVtbl; };

struct IShellLinkW_X;
struct IShellLinkW_Vtbl {
  HRESULT (WINAPI *QueryInterface)(IShellLinkW_X*, REFIID_X, void**);
  ULONG_PTR (WINAPI *AddRef)(IShellLinkW_X*);
  ULONG_PTR (WINAPI *Release)(IShellLinkW_X*);
  HRESULT (WINAPI *GetPath)(IShellLinkW_X*, LPWSTR, int, void*, DWORD);
  HRESULT (WINAPI *GetIDList)(IShellLinkW_X*, void**);
  HRESULT (WINAPI *SetIDList)(IShellLinkW_X*, const void*);
  HRESULT (WINAPI *GetDescription)(IShellLinkW_X*, LPWSTR, int);
  HRESULT (WINAPI *SetDescription)(IShellLinkW_X*, LPCWSTR);
  HRESULT (WINAPI *GetWorkingDirectory)(IShellLinkW_X*, LPWSTR, int);
  HRESULT (WINAPI *SetWorkingDirectory)(IShellLinkW_X*, LPCWSTR);
  HRESULT (WINAPI *GetArguments)(IShellLinkW_X*, LPWSTR, int);
  HRESULT (WINAPI *SetArguments)(IShellLinkW_X*, LPCWSTR);
  HRESULT (WINAPI *GetHotkey)(IShellLinkW_X*, WORD*);
  HRESULT (WINAPI *SetHotkey)(IShellLinkW_X*, WORD);
  HRESULT (WINAPI *GetShowCmd)(IShellLinkW_X*, int*);
  HRESULT (WINAPI *SetShowCmd)(IShellLinkW_X*, int);
  HRESULT (WINAPI *GetIconLocation)(IShellLinkW_X*, LPWSTR, int, int*);
  HRESULT (WINAPI *SetIconLocation)(IShellLinkW_X*, LPCWSTR, int);
  HRESULT (WINAPI *SetRelativePath)(IShellLinkW_X*, LPCWSTR, DWORD);
  HRESULT (WINAPI *Resolve)(IShellLinkW_X*, HWND, DWORD);
  HRESULT (WINAPI *SetPath)(IShellLinkW_X*, LPCWSTR);
};
struct IShellLinkW_X { IShellLinkW_Vtbl* lpVtbl; };

struct IPersistFile_X;
struct IPersistFile_Vtbl {
  HRESULT (WINAPI *QueryInterface)(IPersistFile_X*, REFIID_X, void**);
  ULONG_PTR (WINAPI *AddRef)(IPersistFile_X*);
  ULONG_PTR (WINAPI *Release)(IPersistFile_X*);
  HRESULT (WINAPI *GetClassID)(IPersistFile_X*, GUID_X*);
  HRESULT (WINAPI *IsDirty)(IPersistFile_X*);
  HRESULT (WINAPI *Load)(IPersistFile_X*, LPCWSTR, DWORD);
  HRESULT (WINAPI *Save)(IPersistFile_X*, LPCWSTR, BOOL);
  HRESULT (WINAPI *SaveCompleted)(IPersistFile_X*, LPCWSTR);
  HRESULT (WINAPI *GetCurFile)(IPersistFile_X*, LPWSTR*);
};
struct IPersistFile_X { IPersistFile_Vtbl* lpVtbl; };

extern "C" void* memset(void* dst, int c, unsigned long long n){ unsigned char* p=(unsigned char*)dst; for(unsigned long long i=0;i<n;i++)p[i]=(unsigned char)c; return dst; }
extern "C" void* memcpy(void* dst, const void* src, unsigned long long n){ unsigned char* d=(unsigned char*)dst; const unsigned char* s=(const unsigned char*)src; for(unsigned long long i=0;i<n;i++)d[i]=s[i]; return dst; }
extern "C" {
__declspec(dllimport) DWORD WINAPI GetModuleFileNameW(HINSTANCE, LPWSTR, DWORD);
__declspec(dllimport) HANDLE WINAPI CreateFileW(LPCWSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
__declspec(dllimport) BOOL WINAPI GetFileSizeEx(HANDLE,LARGE_INTEGER_X*);
__declspec(dllimport) BOOL WINAPI SetFilePointerEx(HANDLE,LARGE_INTEGER_X,LARGE_INTEGER_X*,DWORD);
__declspec(dllimport) BOOL WINAPI ReadFile(HANDLE,LPVOID,DWORD,DWORD*,LPVOID);
__declspec(dllimport) BOOL WINAPI WriteFile(HANDLE,LPCVOID,DWORD,DWORD*,LPVOID);
__declspec(dllimport) BOOL WINAPI CloseHandle(HANDLE);
__declspec(dllimport) DWORD WINAPI GetEnvironmentVariableW(LPCWSTR,LPWSTR,DWORD);
__declspec(dllimport) BOOL WINAPI CreateDirectoryW(LPCWSTR,LPVOID);
__declspec(dllimport) DWORD WINAPI GetTempPathW(DWORD,LPWSTR);
__declspec(dllimport) BOOL WINAPI DeleteFileW(LPCWSTR);
__declspec(dllimport) BOOL WINAPI MoveFileExW(LPCWSTR,LPCWSTR,DWORD);
__declspec(dllimport) BOOL WINAPI CreateProcessW(LPCWSTR,LPWSTR,LPVOID,LPVOID,BOOL,DWORD,LPVOID,LPCWSTR,STARTUPINFOW_X*,PROCESS_INFORMATION_X*);
__declspec(dllimport) DWORD WINAPI WaitForSingleObject(HANDLE,DWORD);
__declspec(dllimport) BOOL WINAPI GetExitCodeProcess(HANDLE,DWORD*);
__declspec(dllimport) void WINAPI ExitProcess(DWORD);
__declspec(dllimport) int WINAPI MessageBoxW(HWND,LPCWSTR,LPCWSTR,unsigned int);
__declspec(dllimport) HMODULE WINAPI LoadLibraryW(LPCWSTR);
__declspec(dllimport) FARPROC WINAPI GetProcAddress(HMODULE,const char*);
__declspec(dllimport) BOOL WINAPI FreeLibrary(HMODULE);
__declspec(dllimport) BOOL WINAPI GetOpenFileNameW(OPENFILENAMEW_X*);
}

static int wlen(const wchar_t* s){ int n=0; while(s&&s[n]) ++n; return n; }
static void wcopy(wchar_t* d,const wchar_t* s,int cap){ int i=0; if(cap<=0)return; while(s&&s[i]&&i<cap-1){d[i]=s[i];i++;} d[i]=0; }
static void wcat(wchar_t* d,const wchar_t* s,int cap){ int n=wlen(d),i=0; while(s&&s[i]&&n<cap-1){d[n++]=s[i++];} d[n]=0; }
static bool fileExists(const wchar_t* p){ HANDLE h=CreateFileW(p,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0); if(h==INVALID_HANDLE_VALUE)return false; CloseHandle(h); return true; }
static void makePath2(const wchar_t* a,const wchar_t* b,wchar_t* out,int cap){wcopy(out,a,cap); if(wlen(out)&&out[wlen(out)-1]!=L'\\')wcat(out,L"\\",cap); wcat(out,b,cap);} 
static bool runWait(wchar_t* cmd){ STARTUPINFOW_X si={}; PROCESS_INFORMATION_X pi={}; si.cb=sizeof(si); if(!CreateProcessW(0,cmd,0,0,FALSE,CREATE_NO_WINDOW,0,0,&si,&pi))return false; WaitForSingleObject(pi.hProcess,INFINITE); DWORD ec=1; GetExitCodeProcess(pi.hProcess,&ec); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return ec==0; }
static bool okhr(HRESULT h){ return h>=0; }

// Compact SHA-256 implementation
struct SHA256_CTX { u32 h[8]; u64 bits; u8 buf[64]; u32 used; };
static const u32 K256[64]={
0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
static u32 rr(u32 x,u32 n){return (x>>n)|(x<<(32-n));}
static void shablock(SHA256_CTX* c,const u8* b){u32 w[64];for(int i=0;i<16;i++)w[i]=((u32)b[i*4]<<24)|((u32)b[i*4+1]<<16)|((u32)b[i*4+2]<<8)|b[i*4+3];for(int i=16;i<64;i++){u32 s0=rr(w[i-15],7)^rr(w[i-15],18)^(w[i-15]>>3);u32 s1=rr(w[i-2],17)^rr(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}u32 a=c->h[0],bb=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],h=c->h[7];for(int i=0;i<64;i++){u32 S1=rr(e,6)^rr(e,11)^rr(e,25);u32 ch=(e&f)^((~e)&g);u32 t1=h+S1+ch+K256[i]+w[i];u32 S0=rr(a,2)^rr(a,13)^rr(a,22);u32 maj=(a&bb)^(a&cc)^(bb&cc);u32 t2=S0+maj;h=g;g=f;f=e;e=d+t1;d=cc;cc=bb;bb=a;a=t1+t2;}c->h[0]+=a;c->h[1]+=bb;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=h;}
static void shainit(SHA256_CTX* c){c->h[0]=0x6a09e667u;c->h[1]=0xbb67ae85u;c->h[2]=0x3c6ef372u;c->h[3]=0xa54ff53au;c->h[4]=0x510e527fu;c->h[5]=0x9b05688cu;c->h[6]=0x1f83d9abu;c->h[7]=0x5be0cd19u;c->bits=0;c->used=0;}
static void shaupdate(SHA256_CTX* c,const u8* p,u32 n){c->bits+=(u64)n*8;while(n){u32 take=64-c->used;if(take>n)take=n;for(u32 i=0;i<take;i++)c->buf[c->used+i]=p[i];c->used+=take;p+=take;n-=take;if(c->used==64){shablock(c,c->buf);c->used=0;}}}
static void shafinal(SHA256_CTX* c,u8 out[32]){c->buf[c->used++]=0x80;if(c->used>56){while(c->used<64)c->buf[c->used++]=0;shablock(c,c->buf);c->used=0;}while(c->used<56)c->buf[c->used++]=0;for(int i=7;i>=0;i--)c->buf[c->used++]=(u8)(c->bits>>(i*8));shablock(c,c->buf);for(int i=0;i<8;i++){out[i*4]=(u8)(c->h[i]>>24);out[i*4+1]=(u8)(c->h[i]>>16);out[i*4+2]=(u8)(c->h[i]>>8);out[i*4+3]=(u8)c->h[i];}}
static bool shaFile(const wchar_t* p,u8 out[32]){HANDLE h=CreateFileW(p,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);if(h==INVALID_HANDLE_VALUE)return false;SHA256_CTX c;shainit(&c);u8 buf[65536];DWORD got=0;while(ReadFile(h,buf,sizeof(buf),&got,0)&&got)shaupdate(&c,buf,got);CloseHandle(h);shafinal(&c,out);return true;}
static bool hashEqHex(const u8 h[32],const char* hex){for(int i=0;i<32;i++){char a=hex[i*2],b=hex[i*2+1];u8 x=(u8)(((a<='9'?a-'0':a-'a'+10)<<4)|(b<='9'?b-'0':b-'a'+10));if(h[i]!=x)return false;}return true;}

static bool extractPayload(const wchar_t* payloadZip,const wchar_t* installDir){
  wchar_t cmd[2048]; wcopy(cmd,L"tar.exe -xf \"",2048);wcat(cmd,payloadZip,2048);wcat(cmd,L"\" -C \"",2048);wcat(cmd,installDir,2048);wcat(cmd,L"\"",2048);return runWait(cmd);
}
static bool writeEmbeddedPayload(const wchar_t* outPath){
  wchar_t self[MAX_PATH*4]; if(!GetModuleFileNameW(0,self,(DWORD)(MAX_PATH*4)))return false;
  HANDLE in=CreateFileW(self,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);if(in==INVALID_HANDLE_VALUE)return false;
  LARGE_INTEGER_X sz={}; if(!GetFileSizeEx(in,&sz)||sz.QuadPart<16){CloseHandle(in);return false;}
  LARGE_INTEGER_X pos={}; pos.QuadPart=sz.QuadPart-8; if(!SetFilePointerEx(in,pos,0,FILE_BEGIN)){CloseHandle(in);return false;}
  u64 payloadSize=0;DWORD got=0;if(!ReadFile(in,&payloadSize,8,&got,0)||got!=8||payloadSize==0||payloadSize>(u64)sz.QuadPart-8){CloseHandle(in);return false;}
  pos.QuadPart=sz.QuadPart-8-(long long)payloadSize;if(!SetFilePointerEx(in,pos,0,FILE_BEGIN)){CloseHandle(in);return false;}
  HANDLE out=CreateFileW(outPath,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(out==INVALID_HANDLE_VALUE){CloseHandle(in);return false;}
  u8 buf[65536];u64 left=payloadSize;bool ok=true;while(left){DWORD want=(left>sizeof(buf))?sizeof(buf):(DWORD)left;DWORD r=0,w=0;if(!ReadFile(in,buf,want,&r,0)||r==0){ok=false;break;}if(!WriteFile(out,buf,r,&w,0)||w!=r){ok=false;break;}left-=r;}
  CloseHandle(out);CloseHandle(in);return ok&&left==0;
}

static bool createShortcut(const wchar_t* exePath,const wchar_t* workDir,const wchar_t* lnkPath){
  HMODULE ole=LoadLibraryW(L"ole32.dll"); if(!ole)return false;
  typedef HRESULT (WINAPI *PFN_CoInitializeEx)(LPVOID,DWORD);
  typedef HRESULT (WINAPI *PFN_CoCreateInstance)(REFGUID_X,LPVOID,DWORD,REFIID_X,LPVOID*);
  typedef void (WINAPI *PFN_CoUninitialize)();
  PFN_CoInitializeEx pCoInitializeEx=(PFN_CoInitializeEx)GetProcAddress(ole,"CoInitializeEx");
  PFN_CoCreateInstance pCoCreateInstance=(PFN_CoCreateInstance)GetProcAddress(ole,"CoCreateInstance");
  PFN_CoUninitialize pCoUninitialize=(PFN_CoUninitialize)GetProcAddress(ole,"CoUninitialize");
  if(!pCoInitializeEx||!pCoCreateInstance||!pCoUninitialize){FreeLibrary(ole);return false;}

  const GUID_X CLSID_ShellLink={0x00021401,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
  const GUID_X IID_IShellLinkW={0x000214F9,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
  const GUID_X IID_IPersistFile={0x0000010b,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

  HRESULT ci=pCoInitializeEx(0,COINIT_APARTMENTTHREADED);
  IShellLinkW_X* sl=0;
  HRESULT hr=pCoCreateInstance(CLSID_ShellLink,0,CLSCTX_INPROC_SERVER,IID_IShellLinkW,(LPVOID*)&sl);
  if(!okhr(hr)||!sl){if(okhr(ci))pCoUninitialize();FreeLibrary(ole);return false;}
  sl->lpVtbl->SetPath(sl,exePath);
  sl->lpVtbl->SetWorkingDirectory(sl,workDir);
  sl->lpVtbl->SetDescription(sl,L"DesertLink - Crimson Desert Map Companion");
  sl->lpVtbl->SetIconLocation(sl,exePath,0);
  IPersistFile_X* pf=0;
  hr=sl->lpVtbl->QueryInterface(sl,IID_IPersistFile,(void**)&pf);
  bool ok=false;
  if(okhr(hr)&&pf){hr=pf->lpVtbl->Save(pf,lnkPath,TRUE);ok=okhr(hr);pf->lpVtbl->Release(pf);}
  sl->lpVtbl->Release(sl);
  if(okhr(ci))pCoUninitialize();
  FreeLibrary(ole);
  return ok;
}

static bool getShellFolder(int csidl,wchar_t* out,int cap){
  HMODULE shell=LoadLibraryW(L"shell32.dll");if(!shell)return false;
  typedef HRESULT (WINAPI *PFN_SHGetFolderPathW)(HWND,int,HANDLE,DWORD,LPWSTR);
  PFN_SHGetFolderPathW fn=(PFN_SHGetFolderPathW)GetProcAddress(shell,"SHGetFolderPathW");
  if(!fn){FreeLibrary(shell);return false;}
  HRESULT hr=fn(0,csidl,0,SHGFP_TYPE_CURRENT,out);
  FreeLibrary(shell);return okhr(hr)&&wlen(out)>0;
}

extern "C" void WINAPI entry(){
  const wchar_t* title=L"DesertLink App Setup v1.0.6";
  if(MessageBoxW(0,L"This setup installs ONLY the DesertLink desktop app and creates shortcuts.\n\nIt does NOT install, copy, or modify any ASI files.\n\nInstall DesertLinkCore.asi and CrimsonDesertTelemetry.asi separately using the instructions on the Nexus page.\n\nSetup performs no Internet downloads.\n\nContinue?",title,MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)ExitProcess(0);

  wchar_t local[MAX_PATH*4]; if(!GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH*4)){MessageBoxW(0,L"Could not locate LOCALAPPDATA.",title,MB_OK|MB_ICONERROR);ExitProcess(2);}  
  wchar_t programs[MAX_PATH*4];makePath2(local,L"Programs",programs,MAX_PATH*4);CreateDirectoryW(programs,0);
  wchar_t installDir[MAX_PATH*4];makePath2(programs,L"DesertLink",installDir,MAX_PATH*4);CreateDirectoryW(installDir,0);

  wchar_t tmp[MAX_PATH*4];if(!GetTempPathW(MAX_PATH*4,tmp)){MessageBoxW(0,L"Could not locate the Windows temp folder.",title,MB_OK|MB_ICONERROR);ExitProcess(3);}
  wchar_t payloadZip[MAX_PATH*4];makePath2(tmp,L"DesertLink_app_payload_v106.zip",payloadZip,MAX_PATH*4);
  if(!writeEmbeddedPayload(payloadZip)){MessageBoxW(0,L"Could not unpack the embedded DesertLink application files.",title,MB_OK|MB_ICONERROR);ExitProcess(4);}

  wchar_t appExe[MAX_PATH*4];makePath2(installDir,L"DesertLink.exe",appExe,MAX_PATH*4);
  if(!fileExists(appExe)){
    wchar_t legacyBase[MAX_PATH*4];makePath2(local,L"DesertLink\\Required Files\\Runtime",legacyBase,MAX_PATH*4);
    wchar_t legacyExe[MAX_PATH*4];makePath2(legacyBase,L"DesertLink.exe",legacyExe,MAX_PATH*4);
    if(fileExists(legacyExe)){
      wchar_t copyCmd[4096];wcopy(copyCmd,L"xcopy.exe /E /I /Y /Q \"",4096);wcat(copyCmd,legacyBase,4096);wcat(copyCmd,L"\\*\" \"",4096);wcat(copyCmd,installDir,4096);wcat(copyCmd,L"\"",4096);
      runWait(copyCmd);
    }
  }

  if(!fileExists(appExe)){
    MessageBoxW(0,L"First-time setup needs the official Electron v44.2.0 Windows x64 runtime ZIP.\n\nDownload electron-v44.2.0-win32-x64.zip using the official link in the Nexus description, then select that ZIP in the next window.\n\nDesertLink Setup does not download anything itself.",title,MB_OK|MB_ICONINFORMATION);
    wchar_t runtimeZip[MAX_PATH*4]; runtimeZip[0]=0; wcopy(runtimeZip,L"electron-v44.2.0-win32-x64.zip",MAX_PATH*4);
    static const wchar_t zipFilter[]=L"Electron runtime ZIP (electron-v44.2.0-win32-x64.zip)\0electron-v44.2.0-win32-x64.zip\0ZIP Files (*.zip)\0*.zip\0All Files (*.*)\0*.*\0\0";
    OPENFILENAMEW_X zfn={};zfn.lStructSize=sizeof(zfn);zfn.lpstrFilter=zipFilter;zfn.lpstrFile=runtimeZip;zfn.nMaxFile=MAX_PATH*4;zfn.lpstrTitle=L"Select electron-v44.2.0-win32-x64.zip";zfn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER|OFN_HIDEREADONLY;zfn.lpstrDefExt=L"zip";
    if(!GetOpenFileNameW(&zfn)){MessageBoxW(0,L"Runtime selection cancelled.\n\nDownload the official Electron runtime ZIP from the Nexus description and run setup again.",title,MB_OK|MB_ICONINFORMATION);ExitProcess(0);}
    u8 digest[32];if(!shaFile(runtimeZip,digest)||!hashEqHex(digest,"4021363e3090d67a144ebedb90765cf193b0e61f300c519c83f0174502a481da")){
      MessageBoxW(0,L"Runtime SHA-256 verification failed.\n\nMake sure you selected the official electron-v44.2.0-win32-x64.zip linked in the Nexus description.\n\nNothing from that ZIP was installed.",title,MB_OK|MB_ICONERROR);ExitProcess(6);
    }
    wchar_t cmd[2048];wcopy(cmd,L"tar.exe -xf \"",2048);wcat(cmd,runtimeZip,2048);wcat(cmd,L"\" -C \"",2048);wcat(cmd,installDir,2048);wcat(cmd,L"\"",2048);
    if(!runWait(cmd)){MessageBoxW(0,L"Windows could not extract the Electron runtime.\n\nCurrent Windows 10/11 systems include tar.exe.",title,MB_OK|MB_ICONERROR);ExitProcess(7);}
    wchar_t electronExe[MAX_PATH*4];makePath2(installDir,L"electron.exe",electronExe,MAX_PATH*4);
    if(!MoveFileExW(electronExe,appExe,MOVEFILE_REPLACE_EXISTING)){MessageBoxW(0,L"Could not prepare DesertLink.exe after extracting the runtime.",title,MB_OK|MB_ICONERROR);ExitProcess(8);}
  }

  if(!extractPayload(payloadZip,installDir)){MessageBoxW(0,L"Could not install DesertLink application files.",title,MB_OK|MB_ICONERROR);ExitProcess(9);}  
  DeleteFileW(payloadZip);
  wchar_t defaultAsar[MAX_PATH*4];makePath2(installDir,L"resources\\default_app.asar",defaultAsar,MAX_PATH*4);DeleteFileW(defaultAsar);

  bool desktopOk=false,startOk=false;
  wchar_t folder[MAX_PATH*4];
  if(getShellFolder(CSIDL_DESKTOPDIRECTORY,folder,MAX_PATH*4)){
    wchar_t lnk[MAX_PATH*4];makePath2(folder,L"DesertLink.lnk",lnk,MAX_PATH*4);desktopOk=createShortcut(appExe,installDir,lnk);
  }
  if(getShellFolder(CSIDL_PROGRAMS,folder,MAX_PATH*4)){
    wchar_t menuDir[MAX_PATH*4];makePath2(folder,L"DesertLink",menuDir,MAX_PATH*4);CreateDirectoryW(menuDir,0);
    wchar_t lnk[MAX_PATH*4];makePath2(menuDir,L"DesertLink.lnk",lnk,MAX_PATH*4);startOk=createShortcut(appExe,installDir,lnk);
  }

  if(!desktopOk && !startOk){
    MessageBoxW(0,L"DesertLink was installed successfully, but Windows did not allow Setup to create shortcuts.\n\nThe app is installed at:\n%LOCALAPPDATA%\\Programs\\DesertLink\\DesertLink.exe",title,MB_OK|MB_ICONINFORMATION);
  }else{
    MessageBoxW(0,L"DesertLink App installed successfully.\n\nA Desktop shortcut and Start Menu shortcut were created.\n\nThis installer did not touch any ASI files and performed no Internet downloads.\n\nDesertLink will start now.",title,MB_OK|MB_ICONINFORMATION);
  }

  wchar_t launch[2048];wcopy(launch,L"\"",2048);wcat(launch,appExe,2048);wcat(launch,L"\"",2048);
  STARTUPINFOW_X si={};PROCESS_INFORMATION_X pi={};si.cb=sizeof(si);if(CreateProcessW(0,launch,0,0,FALSE,0,0,installDir,&si,&pi)){CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}  
  ExitProcess(0);
}
