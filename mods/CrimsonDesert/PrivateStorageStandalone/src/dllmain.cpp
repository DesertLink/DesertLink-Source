#include <Windows.h>
#include <atomic>

#include "core/log.h"
#include "core/paths.h"
#include "game/farhook.h"
#include "game/mem.h"
#include "private_storage.h"

namespace
{
    HMODULE g_module = nullptr;
    std::atomic<bool> g_stop{false};
    HANDLE g_worker = nullptr;

    DWORD WINAPI Worker(LPVOID)
    {
        // Let the game finish building its UI/stage systems before resolving
        // and installing the three narrow hooks.
        const DWORD start = GetTickCount();
        while (!g_stop.load() && GetTickCount() - start < 10000)
            Sleep(100);

        if (g_stop.load()) return 0;

        const bool ok = dlps::Start();
        LOG_NOTE("[mod] DesertLink Private Storage standalone: %s",
                 ok ? "ready" : "disabled; see errors above");
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = module;
        DisableThreadLibraryCalls(module);

        psm::Paths::Init(module);

        // crashpad_handler.exe and tiny helper processes may load ASIs too.
        if (!psm::mem::Game().base || psm::mem::Game().size < 64ull * 1024 * 1024)
            return TRUE;

        psm::Log::Claim(L"DesertLinkPrivateStorage");
        LOG_NOTE("[mod] DesertLinkPrivateStorage starting");

        g_worker = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (g_worker) CloseHandle(g_worker);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        g_stop = true;

        if (!reserved)
        {
            dlps::Stop();
            psm::farhook::RemoveAll();
        }

        psm::Log::Shutdown();
    }
    return TRUE;
}
