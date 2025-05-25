// In core/log.hpp
#pragma once
// REMOVE: #include "shared/ctx.hpp"
#include "shared/util.hpp" // For alloc_console
#include <print>
#include <thread>
#include <Windows.h>

namespace defendnot {
    constexpr bool HARDCODED_LOG_VERBOSE = true; // Or false

    template <typename... TArgs>
    void logln(const std::format_string<TArgs...> fmt, TArgs... args) noexcept {
        if (!HARDCODED_LOG_VERBOSE) { // Use a local hardcoded value
            return;
        }
        shared::alloc_console(); // shared::alloc_console itself doesn't depend on ctx
        std::println(stdout, fmt, std::forward<TArgs>(args)...);
    }
}
