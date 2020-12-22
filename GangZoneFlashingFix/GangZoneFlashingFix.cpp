#include <Windows.h>

#include "urmem/urmem.hpp"

namespace ch = std::chrono;
constexpr auto MAX_GANGZONES = 1024;

#pragma pack(push, 1)
struct stGangZonePool {
    struct stGangZone {
        float fPosition[4];
        UINT32 uiColor;
        UINT32 uiAltColor;
    } *pGangZone[MAX_GANGZONES];
    BOOL iIsListed[MAX_GANGZONES];
} *pGangZonePool;
#pragma pack(pop)

class CGangZoneFlashingFix {
public:
    static auto Init() -> void {
        Thread = std::make_shared<CThread>(nullptr, NULL, [](LPVOID) -> DWORD {
            for (; pGangZonePool == nullptr; std::this_thread::sleep_for(ch::milliseconds(100))) {
                if ((baseAddress = reinterpret_cast<urmem::address_t>(GetModuleHandle("samp.dll"))) == NULL)
                    continue;

                urmem::address_t sampInfoAddress = NULL;
                if ((sampInfoAddress = *reinterpret_cast<urmem::address_t*>(baseAddress + 0x21A0F8)) == NULL)
                    continue;

                urmem::address_t poolsAddress = NULL;
                if ((poolsAddress = *reinterpret_cast<urmem::address_t*>(sampInfoAddress + 0x3CD)) == NULL)
                    continue;

                if ((pGangZonePool = *reinterpret_cast<stGangZonePool**>(poolsAddress + 0x08)) != nullptr)
                    hkGangZoneDraw = std::make_shared<urmem::hook>(baseAddress + 0x9F1E7, urmem::get_func_addr(&HOOK_GangZoneDraw), urmem::hook::type::call, 5);
            }

            ExitThread(0);
            return 0;
        }, nullptr, NULL, nullptr);
    }

    static auto __stdcall HOOK_GangZoneDraw() -> void {
        static bool bState = false;
        static int64_t iTime = 0;

        if (ch::duration_cast<ch::milliseconds>(ch::steady_clock::now().time_since_epoch()).count() - iTime >= 500) {
            bState ^= true;
            iTime = ch::duration_cast<ch::milliseconds>(ch::steady_clock::now().time_since_epoch()).count();
        }

        for (WORD i = 0; i < MAX_GANGZONES; i++) {
            if (pGangZonePool->iIsListed[i] == TRUE) {
                urmem::call_function<urmem::calling_convention::thiscall, void, void*, float*, DWORD>(baseAddress + 0x9C9E0, reinterpret_cast<void*>(baseAddress + 0x21A10C),
                    pGangZonePool->pGangZone[i]->fPosition, bState ? pGangZonePool->pGangZone[i]->uiAltColor : pGangZonePool->pGangZone[i]->uiColor);
            }
        }
    }

private:
    class CThread {
    public:
        explicit CThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
            : hHandle{ CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId) } {
        }
        ~CThread() {
            if (hHandle)
                CloseHandle(hHandle);
        }

    private:
        HANDLE hHandle;
    };

    static urmem::address_t baseAddress;
    static std::shared_ptr<CThread> Thread;
    static std::shared_ptr<urmem::hook> hkGangZoneDraw;
};

urmem::address_t CGangZoneFlashingFix::baseAddress;
std::shared_ptr<CGangZoneFlashingFix::CThread> CGangZoneFlashingFix::Thread;
std::shared_ptr<urmem::hook> CGangZoneFlashingFix::hkGangZoneDraw;

auto APIENTRY DllMain(HMODULE hModule, DWORD dwReasonForCall, LPVOID lpReserved) -> BOOL {
    switch (dwReasonForCall) {
    case DLL_PROCESS_ATTACH:
        CGangZoneFlashingFix::Init();
        break;
    case DLL_PROCESS_DETACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }

    return TRUE;
}