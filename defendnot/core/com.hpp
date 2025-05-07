#pragma once
#include <format>
#include <source_location>
#include <stdexcept>
#include <thread>

#include "core/log.hpp"

#include <Windows.h>

namespace defendnot {
    namespace detail {
        inline GUID RCLSID = {0x0F2102C37, 0x90C3, 0x450C, {0x0B3, 0x0F6, 0x92, 0x0BE, 0x16, 0x93, 0x0BD, 0x0F2}};
        inline GUID IID_IWscAVStatus = {0x3901A765, 0x0AB91, 0x4BA9, {0xA5, 0x53, 0x5B, 0x85, 0x38, 0xDE, 0xB8, 0x40}};
    } // namespace detail

    enum class WSCSecurityProductState : std::uint32_t {
        ON = 0,
        OFF = 1,
        SNOOZED = 2,
        EXPIRED = 3
    };

    inline HRESULT com_checked(HRESULT result, const std::source_location loc = std::source_location::current()) {
        if (result == 0) {
            return result;
        }

        auto msg = std::format("Got HRESULT={:#x} at\n{}:{}", static_cast<std::uint32_t>(result) & 0xFFFFFFFF, loc.function_name(), loc.line());
        throw std::runtime_error(msg);
    }

    template <typename Callable>
    inline HRESULT com_retry_while_pending(Callable&& fn) {
        bool delayed = false;
        HRESULT status = 0;
        do {
            if (status != 0) {
                delayed = true;
                logln("delaying for com retry...");
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }

            status = fn();
        } while (status == E_PENDING);

        if (delayed) {
            /// Sleep for additional 15 seconds to let WSC proceed all previous requests
            std::this_thread::sleep_for(std::chrono::seconds(15));
        }

        return status;
    }

    class IWscAVStatus {
    private:
        /// Incomplete stubs to just increase the vfunc id
        virtual HRESULT QueryInterface() = 0;
        virtual HRESULT AddRef() = 0;
        virtual HRESULT Release() = 0;

    public:
        virtual HRESULT Register(BSTR path_to_signed_product_exe, BSTR display_name) = 0;
        virtual HRESULT Unregister() = 0;
        virtual HRESULT UpdateStatus(WSCSecurityProductState state, std::uint32_t idk) = 0;

        static IWscAVStatus* get() {
            IWscAVStatus* result = nullptr;
            com_checked(CoCreateInstance(detail::RCLSID, 0, 1, detail::IID_IWscAVStatus, reinterpret_cast<LPVOID*>(&result)));
            return result;
        }
    };
} // namespace defendnot
