#include "private_storage.h"

#include <Windows.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "core/log.h"
#include "game/addresses.h"
#include "game/farhook.h"
#include "game/mem.h"

namespace dlps
{
    namespace
    {
        psm::addr::Storage A{};

        // Engine staticstringA node. A negative refcount makes the game skip
        // AddRef/Release, so these process-lifetime nodes remain owned by us.
        struct SsNode
        {
            const char* p;
            uint32_t len;
            uint32_t hash;
            int32_t ref;
            uint8_t flag;
            uint8_t pad[3];
            char text[1];
        };
        static_assert(offsetof(SsNode, len) == 0x08);
        static_assert(offsetof(SsNode, ref) == 0x10);
        static_assert(offsetof(SsNode, text) == 0x18);

        struct Arg
        {
            uint8_t type;
            uint8_t pad[7];
            uint64_t value;
        };
        struct Sub
        {
            uint8_t kind;
            uint8_t pad[7];
            Arg* args;
            uint32_t count;
            uint32_t cap;
        };
        struct Data
        {
            uint8_t kind;
            uint8_t pad[7];
            Sub* subs;
            uint32_t count;
            uint32_t cap;
        };
        static_assert(sizeof(Arg) == 0x10);
        static_assert(sizeof(Sub) == 0x18);
        static_assert(offsetof(Data, subs) == 0x08);
        static_assert(offsetof(Data, count) == 0x10);

        using PostFn = void(__fastcall*)(uintptr_t, uint32_t, uintptr_t*, uintptr_t*, uint64_t*, Data*);
        using PhaseFn = uintptr_t(__fastcall*)(uintptr_t, uint32_t, uint32_t);
        using InputBlockFn = void(__fastcall*)(uintptr_t, uint64_t*, uint32_t);
        using Fn4 = uintptr_t(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);

        constexpr uint32_t kPlayerActor = 0xA0100001;
        constexpr uint8_t kPhaseIngameMenu = 0x0E;
        constexpr uint8_t kScreenIngame = 0x10;
        constexpr DWORD kReopenCooldownMs = 250;

        // Private Storage only. No capacity, stack, loot, vendor or inventory
        // modifications are present in this standalone.
        constexpr const char* kSetInventory =
            "SetInventory(Character,Focus,True,Default;CampWareHouse,Focus,True,Default)";
        constexpr const char* kSetTitle =
            "SetWareHouseInventoryName(UI_WareHouse_CampStroage)";
        constexpr const char* kModalHash = "197270237";
        constexpr const char* kTitleHash = "1494912655";
        constexpr const char* kIcon = "cd_icon_map_bank";

        SsNode* g_view = nullptr;
        SsNode* g_selector = nullptr;
        SsNode* g_icon = nullptr;
        SsNode* g_titleHash = nullptr;
        SsNode* g_modalHash = nullptr;
        SsNode* g_setInventory = nullptr;
        SsNode* g_setTitle = nullptr;

        std::atomic<bool> g_ready{false};
        std::atomic<bool> g_stop{false};
        std::atomic<bool> g_open{false};
        std::atomic<bool> g_openRequest{false};
        std::atomic<bool> g_closeRequest{false};
        std::atomic<uintptr_t> g_warehouse{0};
        std::atomic<uintptr_t> g_stageMgr{0};

        uint64_t g_nextId = 910000000;
        uint64_t g_openId = 0;
        uint32_t g_frames = 0;
        bool g_sawShow = false;
        uintptr_t g_blockMgr = 0;
        uint64_t g_blockKey = 0;
        DWORD g_closedAt = 0;

        HANDLE g_keyThread = nullptr;

        void* oHandler = nullptr;
        void* oModeSwitch = nullptr;
        void* oStageClose = nullptr;

        SsNode* MakeSs(const char* s)
        {
            const size_t n = std::strlen(s);
            auto* node = static_cast<SsNode*>(
                HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SsNode) + n + 1));
            if (!node) return nullptr;
            node->len = static_cast<uint32_t>(n);
            node->hash = 0xFFFFFFFF;
            node->ref = -1;
            std::memcpy(node->text, s, n + 1);
            node->p = node->text;
            return node;
        }

        bool ExactBuild()
        {
            const uintptr_t base = psm::mem::Game().base;
            if (!base) return false;
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

            constexpr DWORD kTimeDateStamp = 0x6AB28F00;
            constexpr DWORD kSizeOfImage = 0x173AB000;
            return nt->FileHeader.TimeDateStamp == kTimeDateStamp &&
                   nt->OptionalHeader.SizeOfImage == kSizeOfImage;
        }

        uintptr_t EventWrap()
        {
            uintptr_t mgr = 0, wrap = 0, vt = 0;
            if (!psm::mem::ReadPtr(A.eventManagerGlobal, &mgr) ||
                !psm::mem::ReadPtr(mgr + A.eventWrapOff, &wrap) ||
                !psm::mem::ReadPtr(wrap, &vt))
                return 0;
            return vt == A.eventWrapVtable ? wrap : 0;
        }

        bool PostGuarded(uintptr_t wrap, uintptr_t* view, uintptr_t* selector,
                         uint64_t* id, Data* data)
        {
            __try
            {
                reinterpret_cast<PostFn>(A.eventPost)(
                    wrap, kPlayerActor, view, selector, id, data);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        bool PhaseGuarded(uintptr_t pm, uint8_t phase, bool on)
        {
            __try
            {
                reinterpret_cast<PhaseFn>(A.requestPhase)(pm, phase, on ? 1u : 0u);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        bool InputBlockGuarded(uintptr_t mgr, uint64_t key, uint32_t mode)
        {
            __try
            {
                reinterpret_cast<InputBlockFn>(A.inputBlockSet)(mgr, &key, mode);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        bool Post(uint8_t kind, uint64_t stageId, const SsNode* idText,
                  Sub* extra, uint32_t extraCount)
        {
            const uintptr_t wrap = EventWrap();
            if (!wrap)
            {
                LOG_ERR("[storage] StageChartUIControl event wrap unavailable");
                return false;
            }

            Arg idArg{};
            idArg.type = 5;
            idArg.value = reinterpret_cast<uint64_t>(idText);

            Sub subs[4]{};
            subs[0].kind = 0x00;
            subs[0].args = &idArg;
            subs[0].count = subs[0].cap = 1;

            if (extraCount > 3) extraCount = 3;
            for (uint32_t i = 0; i < extraCount; ++i)
                subs[1 + i] = extra[i];

            Data data{};
            data.kind = kind;
            data.subs = subs;
            data.count = data.cap = 1 + extraCount;

            uintptr_t view = reinterpret_cast<uintptr_t>(g_view);
            uintptr_t selector = reinterpret_cast<uintptr_t>(g_selector);
            uint64_t id = stageId;
            return PostGuarded(wrap, &view, &selector, &id, &data);
        }

        uintptr_t StageManager()
        {
            uintptr_t known = g_stageMgr.load();
            uintptr_t vt = 0;
            if (known && psm::mem::ReadPtr(known, &vt) && vt == A.stageMgrVtable)
                return known;

            uintptr_t global = 0, actorMgr = 0, user = 0, character = 0;
            if (!psm::mem::ReadPtr(A.actorManagerGlobal, &global) ||
                !psm::mem::ReadPtr(global + 0x30, &actorMgr) ||
                !psm::mem::ReadPtr(actorMgr + 0x58, &user))
                return 0;

            psm::mem::ReadPtr(user + 0xD8, &character);
            const uintptr_t owners[2] = { user, character };

            for (uintptr_t owner : owners)
            {
                if (!owner) continue;
                uintptr_t comps = 0;
                const uintptr_t tables[2] = {
                    owner,
                    psm::mem::ReadPtr(owner + 0x68, &comps) ? comps : 0
                };

                for (uintptr_t table : tables)
                {
                    if (!table) continue;
                    for (unsigned off = 0; off < 0x800; off += 8)
                    {
                        uintptr_t candidate = 0;
                        if (!psm::mem::ReadPtr(table + off, &candidate) ||
                            !psm::mem::ReadPtr(candidate, &vt) ||
                            vt != A.stageMgrVtable)
                            continue;

                        g_stageMgr = candidate;
                        return candidate;
                    }
                }
            }
            return 0;
        }

        bool GateOpen(uintptr_t pm)
        {
            uint8_t mode = 0, screen = 0, blocked = 0;
            uint32_t queued = 0;
            uintptr_t proc = 0;

            psm::mem::Read8(pm + 0x28, &mode);
            psm::mem::Read8(pm + A.phaseScreenOff, &screen);
            psm::mem::Read32(pm + 0x70, &queued);
            if (psm::mem::ReadPtr(pm + 0x10, &proc))
                psm::mem::Read8(proc + 0xA0, &blocked);

            return mode == 4 && screen == kScreenIngame &&
                   queued == 0 && proc && blocked == 0;
        }

        void Forget()
        {
            g_blockMgr = 0;
            g_blockKey = 0;
            g_open = false;
            g_warehouse = 0;
            g_closedAt = GetTickCount();
        }

        bool PacketStageId(uintptr_t packet, uint64_t* out)
        {
            uintptr_t subs = 0;
            uint32_t count = 0;
            if (!psm::mem::ReadPtr(packet + 8, &subs) ||
                !psm::mem::Read32(packet + 0x10, &count) ||
                count > 64)
                return false;

            for (uint32_t i = 0; i < count; ++i)
            {
                const uintptr_t sub = subs + 0x18ull * i;
                uint8_t kind = 0;
                uintptr_t args = 0, node = 0, text = 0;
                uint32_t argCount = 0;

                if (!psm::mem::Read8(sub, &kind) || kind != 0) continue;
                if (!psm::mem::ReadPtr(sub + 8, &args) ||
                    !psm::mem::Read32(sub + 0x10, &argCount) ||
                    !argCount)
                    return false;

                char tmp[32]{};
                if (!psm::mem::ReadPtr(args + 8, &node) ||
                    !psm::mem::ReadPtr(node, &text) ||
                    !psm::mem::ReadCString(text, tmp, sizeof tmp))
                    return false;

                char* end = nullptr;
                *out = _strtoui64(tmp, &end, 10);
                return end && end != tmp && *end == 0;
            }
            return false;
        }

        void Close(uintptr_t pm, const char* reason)
        {
            if (!g_open.load()) return;

            LOG_NOTE("[storage] closing Private Storage: %s", reason);

            char idText[32]{};
            std::snprintf(idText, sizeof idText, "%llu",
                          static_cast<unsigned long long>(g_openId));
            SsNode* idNode = MakeSs(idText);
            if (idNode) Post(0x0F, g_openId, idNode, nullptr, 0);

            if (!PhaseGuarded(pm, kPhaseIngameMenu, false))
                LOG_ERR("[storage] failed to leave menu phase");

            if (g_blockMgr && !InputBlockGuarded(g_blockMgr, g_blockKey, 0))
                LOG_ERR("[storage] failed to release stage input block");

            Forget();
        }

        bool Open(uintptr_t pm)
        {
            if (g_open.load()) return true;
            if (GetTickCount() - g_closedAt < kReopenCooldownMs) return false;
            if (!GateOpen(pm))
            {
                LOG_NOTE("[storage] open ignored: game is not in free play");
                return false;
            }

            if (!EventWrap())
            {
                LOG_ERR("[storage] open failed: StageChart event wrap unavailable");
                return false;
            }

            const uintptr_t stageMgr = StageManager();
            if (!stageMgr)
            {
                LOG_ERR("[storage] open failed: ClientSequencerStageManager not found");
                return false;
            }

            const uint64_t id = ++g_nextId;
            char idText[32]{};
            std::snprintf(idText, sizeof idText, "%llu",
                          static_cast<unsigned long long>(id));
            SsNode* idNode = MakeSs(idText);
            if (!idNode)
            {
                LOG_ERR("[storage] open failed: allocation");
                return false;
            }

            g_openId = id;
            g_sawShow = false;
            g_frames = 0;
            g_open = true;

            if (InputBlockGuarded(stageMgr, id, 2))
            {
                g_blockMgr = stageMgr;
                g_blockKey = id;
            }
            else
            {
                LOG_ERR("[storage] stage input block call faulted");
            }

            if (!PhaseGuarded(pm, kPhaseIngameMenu, true))
            {
                LOG_ERR("[storage] menu phase request faulted");
            }

            // Same native StageChart packet order used by the game's warehouse
            // charts: create -> header -> modal -> commands -> show.
            if (!Post(0x12, id, idNode, nullptr, 0))
                LOG_ERR("[storage] packet 0x12 failed");

            Arg iconArg{};
            iconArg.type = 1;
            iconArg.value = reinterpret_cast<uint64_t>(g_icon);

            Arg titleArg{};
            titleArg.type = 9;
            titleArg.value = reinterpret_cast<uint64_t>(g_titleHash);

            Sub header[2]{};
            header[0].kind = 0x0B;
            header[0].args = &iconArg;
            header[0].count = header[0].cap = 1;
            header[1].kind = 0x08;
            header[1].args = &titleArg;
            header[1].count = header[1].cap = 1;
            if (!Post(0x14, id, idNode, header, 2))
                LOG_ERR("[storage] packet 0x14 failed");

            Arg modalArg{};
            modalArg.type = 9;
            modalArg.value = reinterpret_cast<uint64_t>(g_modalHash);
            Sub modal{};
            modal.kind = 0x07;
            modal.args = &modalArg;
            modal.count = modal.cap = 1;
            if (!Post(0x0D, id, idNode, &modal, 1))
                LOG_ERR("[storage] packet 0x0D failed");

            Arg invArg{};
            invArg.type = 5;
            invArg.value = reinterpret_cast<uint64_t>(g_setInventory);

            Arg nameArg{};
            nameArg.type = 5;
            nameArg.value = reinterpret_cast<uint64_t>(g_setTitle);

            Sub commands[2]{};
            commands[0].kind = 0x0E;
            commands[0].args = &invArg;
            commands[0].count = commands[0].cap = 1;
            commands[1].kind = 0x0E;
            commands[1].args = &nameArg;
            commands[1].count = commands[1].cap = 1;
            if (!Post(0x15, id, idNode, commands, 2))
                LOG_ERR("[storage] packet 0x15 failed");

            if (!Post(0x0E, id, idNode, nullptr, 0))
                LOG_ERR("[storage] packet 0x0E failed");

            LOG_NOTE("[storage] Private Storage open requested (stage %llu)",
                     static_cast<unsigned long long>(id));
            return true;
        }

        void Tick(uintptr_t pm)
        {
            if (g_openRequest.exchange(false))
            {
                if (g_open.load()) Close(pm, "toggle");
                else Open(pm);
            }

            if (g_closeRequest.exchange(false))
                Close(pm, "close request");

            uint8_t mode = 0, screen = 0;
            psm::mem::Read8(pm + 0x28, &mode);
            psm::mem::Read8(pm + A.phaseScreenOff, &screen);

            if (!g_open.load()) return;

            ++g_frames;

            if (mode != 4)
            {
                LOG_NOTE("[storage] game left normal play while storage was open");
                Forget();
                return;
            }

            if (g_frames == 150)
            {
                if (!g_sawShow)
                {
                    Close(pm, "warehouse did not receive show packet");
                    return;
                }
                if (screen != kPhaseIngameMenu)
                {
                    Close(pm, "menu phase did not start");
                    return;
                }
            }

            if (g_frames > 150)
            {
                const uintptr_t warehouse = g_warehouse.load();
                uint64_t currentId = 0;
                if (warehouse &&
                    psm::mem::Read64(warehouse + 0x128, &currentId) &&
                    currentId != g_openId)
                {
                    if (g_blockMgr) InputBlockGuarded(g_blockMgr, g_blockKey, 0);
                    Forget();
                }
                else if (screen == kScreenIngame)
                {
                    if (g_blockMgr) InputBlockGuarded(g_blockMgr, g_blockKey, 0);
                    Forget();
                }
            }
        }

        uintptr_t __fastcall hkHandler(
            uintptr_t self, uintptr_t rdx, uintptr_t r8, uintptr_t packet)
        {
            uintptr_t vt = 0;
            if (psm::mem::ReadPtr(self, &vt) && vt == A.warehouseVtable)
                g_warehouse = self;

            if (g_open.load() && packet)
            {
                uint8_t kind = 0;
                uint64_t id = 0;
                if (psm::mem::Read8(packet, &kind) &&
                    kind == 0x0E &&
                    PacketStageId(packet, &id) &&
                    id == g_openId)
                    g_sawShow = true;
            }

            return reinterpret_cast<Fn4>(oHandler)(self, rdx, r8, packet);
        }

        uintptr_t __fastcall hkModeSwitch(
            uintptr_t pm, uintptr_t rdx, uintptr_t r8, uintptr_t r9)
        {
            const uintptr_t result =
                reinterpret_cast<Fn4>(oModeSwitch)(pm, rdx, r8, r9);
            Tick(pm);
            return result;
        }

        uintptr_t __fastcall hkStageClose(
            uintptr_t rcx, uintptr_t idPtr, uintptr_t root, uintptr_t name)
        {
            uint64_t id = 0;
            char cmd[8]{};

            if (g_open.load() &&
                psm::mem::Read64(idPtr, &id) &&
                id == g_openId &&
                psm::mem::ReadCString(name, cmd, sizeof cmd) &&
                std::strcmp(cmd, "Close") == 0)
            {
                g_closeRequest = true;
                return 0;
            }

            return reinterpret_cast<Fn4>(oStageClose)(rcx, idPtr, root, name);
        }

        bool Hook(const char* name, uintptr_t target, void* detour, void** original)
        {
            char why[160]{};
            if (!psm::farhook::Install(name, target, detour, original, why, sizeof why))
            {
                LOG_ERR("[storage] hook %s failed: %s", name, why);
                return false;
            }
            LOG_NOTE("[storage] hooked %s", name);
            return true;
        }

        DWORD WINAPI KeyThread(LPVOID)
        {
            bool wasDown = false;
            while (!g_stop.load())
            {
                const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                const bool f1 = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
                const bool down = ctrl && f1;

                if (down && !wasDown)
                    g_openRequest = true;

                wasDown = down;
                Sleep(16);
            }
            return 0;
        }

        bool AllocateStrings()
        {
            g_view = MakeSs("WareHouseView");
            g_selector = MakeSs("selector-stagechart-self");
            g_icon = MakeSs(kIcon);
            g_titleHash = MakeSs(kTitleHash);
            g_modalHash = MakeSs(kModalHash);
            g_setInventory = MakeSs(kSetInventory);
            g_setTitle = MakeSs(kSetTitle);

            return g_view && g_selector && g_icon && g_titleHash &&
                   g_modalHash && g_setInventory && g_setTitle;
        }
    }

    bool Start()
    {
        if (g_ready.load()) return true;

        if (!ExactBuild())
        {
            LOG_ERR("[storage] unsupported Crimson Desert build; expected 2.03.02 / 1.0.0.2976");
            return false;
        }

        // Do not compete for the same game entry points.
        if (GetModuleHandleW(L"PrivateStorageMaster.asi") ||
            GetModuleHandleW(L"PrivateStorageAnywhere.asi"))
        {
            LOG_ERR("[storage] another private-storage ASI is loaded; remove it before testing DesertLinkPrivateStorage");
            return false;
        }

        if (!psm::addr::ResolveStorage(A))
        {
            LOG_ERR("[storage] runtime signatures/RTTI failed validation");
            return false;
        }

        if (!AllocateStrings())
        {
            LOG_ERR("[storage] string-node allocation failed");
            return false;
        }

        // Minimum runtime ownership for a safe standalone:
        // - Warehouse handler identifies/acknowledges our screen.
        // - StageClose turns native Esc/B close into our main-thread close.
        // - ModeSwitch is the already-proven main-thread execution point.
        if (!Hook("WarehouseHandler", A.warehouseHandler,
                  reinterpret_cast<void*>(&hkHandler), &oHandler) ||
            !Hook("StageClose", A.stageClose,
                  reinterpret_cast<void*>(&hkStageClose), &oStageClose) ||
            !Hook("ModeSwitch", A.modeSwitch,
                  reinterpret_cast<void*>(&hkModeSwitch), &oModeSwitch))
        {
            psm::farhook::RemoveAll();
            return false;
        }

        g_stop = false;
        g_keyThread = CreateThread(nullptr, 0, KeyThread, nullptr, 0, nullptr);
        g_ready = true;
        LOG_NOTE("[storage] READY — Ctrl+F1 toggles Private Storage");
        return true;
    }

    void Stop()
    {
        g_stop = true;
        if (g_keyThread)
        {
            WaitForSingleObject(g_keyThread, 1000);
            CloseHandle(g_keyThread);
            g_keyThread = nullptr;
        }

        g_ready = false;
    }

    void RequestOpen() { g_openRequest = true; }
    void RequestClose() { g_closeRequest = true; }
    void RequestToggle() { g_openRequest = true; }

    bool Ready() { return g_ready.load(); }
    bool IsOpen() { return g_open.load(); }
}

// Integration API for the future unified DesertLink menu. These are deliberately
// asynchronous: the actual game call is executed on the validated main-thread
// ModeSwitch path.
extern "C"
{
    __declspec(dllexport) int DLPS_ApiVersion()
    {
        return 1;
    }

    __declspec(dllexport) int DLPS_Ready()
    {
        return dlps::Ready() ? 1 : 0;
    }

    __declspec(dllexport) int DLPS_IsOpen()
    {
        return dlps::IsOpen() ? 1 : 0;
    }

    __declspec(dllexport) int DLPS_OpenPrivateStorage()
    {
        if (!dlps::Ready()) return 0;
        dlps::RequestOpen();
        return 1;
    }

    __declspec(dllexport) int DLPS_ClosePrivateStorage()
    {
        if (!dlps::Ready()) return 0;
        dlps::RequestClose();
        return 1;
    }

    __declspec(dllexport) int DLPS_TogglePrivateStorage()
    {
        if (!dlps::Ready()) return 0;
        dlps::RequestToggle();
        return 1;
    }
}
