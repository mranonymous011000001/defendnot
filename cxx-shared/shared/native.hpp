#pragma once
#include <stdexcept>
#include <Windows.h>

#pragma pack(push, 1)
namespace native {
    class PEB {
    public:
        std::uint8_t inherited_address_space;
        std::uint8_t read_image_file_exec_options;
        /// - we don't need other fields
    };

    static_assert(offsetof(PEB, read_image_file_exec_options) == 1);

    inline PEB* get_peb() {
        static auto function = reinterpret_cast<PEB* (*)()>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetCurrentPeb"));

        if (function == nullptr) {
            throw std::runtime_error("no RtlGetCurrentPeb");
        }

        static auto result = function();
        if (result == nullptr) [[unlikely]] {
            throw std::runtime_error("no peb");
        }

        return result;
    }
} // namespace native
#pragma pack(pop)
