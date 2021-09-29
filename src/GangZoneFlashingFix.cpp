#include "urmem/urmem.hpp"

namespace ch = std::chrono;
constexpr unsigned short MAX_GANGZONES{ 1024 };

#pragma pack(push, 1)
struct stGangzonePool {
    struct stGangzone {
        float fPosition[4];
        unsigned int uiColor;
        unsigned int uiAltColor;
    } *data[MAX_GANGZONES];

    int iIsListed[MAX_GANGZONES];
} *gangzone_pool;
#pragma pack(pop)

class CGangZoneFlashingFix {
public:
    static void Init () {
        auto handle{ GetModuleHandle("User32.dll") };

        if (handle != nullptr) {
            auto address{ reinterpret_cast<urmem::address_t>(GetProcAddress(handle, "TranslateMessage")) };

            if (address != NULL) {
                // https://gist.github.com/katursis/57bb550b97ff0ebbf7ebab0ff51a73b7#file-main-cpp-L22
                hook_translate_message = urmem::hook::make(address, urmem::get_func_addr(&HOOK_TranslateMessage));
            }
        }
    }

private:
    static constexpr auto info_offset{ 0x21A0F8 };
    static constexpr auto pool_offset{ 0x3CD };
    static constexpr auto gangzone_pool_offset{ 0x08 };
    static constexpr auto draw_gangzone_pool_offset{ 0x9F1E7 };

    static constexpr auto draw_area_on_radar_address{ 0x5853D0 };
    static constexpr auto menu_status_address{ 0xBA67A4 };

    static int __stdcall HOOK_TranslateMessage(const MSG* msg) {
        hook_translate_message->disable();

        auto result{ urmem::call_function<urmem::calling_convention::stdcall, int>(
            hook_translate_message->get_original_addr(), msg) };

        base_address = reinterpret_cast<urmem::address_t>(GetModuleHandle("samp.dll"));

        if (base_address != NULL) {
            auto samp_info_address{ *reinterpret_cast<urmem::address_t*>(base_address + info_offset) };

            if (samp_info_address != NULL) {
                auto samp_pool_address{ *reinterpret_cast<urmem::address_t*>(samp_info_address + pool_offset) };

                if (samp_pool_address != NULL) {
                    gangzone_pool = *reinterpret_cast<stGangzonePool**>(samp_pool_address + gangzone_pool_offset);

                    if (gangzone_pool != nullptr) {
                        hook_draw_gangzone = urmem::hook::make(base_address + draw_gangzone_pool_offset,
                            urmem::get_func_addr(&HOOK_DrawGangzone), urmem::hook::type::call);

                        return result;
                    }
                }
            }
        }

        hook_translate_message->enable();
        return result;
    }

    static void __stdcall HOOK_DrawGangzone() {
        static bool change_color{ false };
        static auto last_time{ ch::steady_clock::now() };

        if (ch::duration_cast<ch::milliseconds>(ch::steady_clock::now() - last_time).count() > 500) {
            change_color ^= true;
            last_time = ch::steady_clock::now();
        }

        for (unsigned short i = 0; i < MAX_GANGZONES; i++) {
            if (gangzone_pool->iIsListed[i] != 0 && gangzone_pool->data[i] != nullptr) {
                unsigned long color{ change_color ? gangzone_pool->data[i]->uiAltColor : gangzone_pool->data[i]->uiColor };

                urmem::call_function<urmem::calling_convention::cdeclcall, void, float*, unsigned long*, bool>(
                    draw_area_on_radar_address, gangzone_pool->data[i]->fPosition, &color,
                    static_cast<bool>(*reinterpret_cast<unsigned char*>(menu_status_address)));
            }
        }
    }

    static urmem::address_t base_address;
    static std::shared_ptr<urmem::hook> hook_translate_message;
    static std::shared_ptr<urmem::hook> hook_draw_gangzone;
};

urmem::address_t CGangZoneFlashingFix::base_address{};
std::shared_ptr<urmem::hook> CGangZoneFlashingFix::hook_translate_message{};
std::shared_ptr<urmem::hook> CGangZoneFlashingFix::hook_draw_gangzone{};


int __stdcall DllMain(HMODULE hModule, DWORD dwReasonForCall, LPVOID lpReserved) {
    if (dwReasonForCall == DLL_PROCESS_ATTACH) {
        CGangZoneFlashingFix::Init();
    }

    return 1;
}