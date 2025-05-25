#include "bootstrap.hpp"
#include "core/com.hpp"
#include "core/log.hpp" // log.hpp might still use shared::ctx for verbosity
// REMOVE or comment out: #include "shared/ctx.hpp"
#include "shared/defer.hpp" // Keep if defer is used directly here

#include <Windows.h>
#include <string> // For std::wstring for SysAllocString

namespace defendnot {

    // --- Directly Hardcoded Configuration Values ---
    constexpr bool   G_DEFENDNOT_ENABLED = true;      // true for ON, false for OFF
    constexpr bool   G_VERBOSE_LOGGING = true;      // true for verbose, false for silent
    const wchar_t*   G_AV_DISPLAY_NAME = L"Super Stealth AV"; // AV Name as wide string

    // If core/log.hpp is modified to not depend on shared::ctx.verbose,
    // you could implement a local log function here or pass G_VERBOSE_LOGGING to it.
    // For now, let's assume log.hpp might still look at shared::ctx.
    // To fully decouple, log.hpp would also need to change.

    void startup() {
        // Log the state to confirm
        // If log.hpp is still using shared::ctx.verbose, this might not reflect G_VERBOSE_LOGGING
        // unless shared::ctx is also modified or log.hpp is changed.
        // For simplicity in this example, we'll assume logln will work or you'd adapt it.
        logln("DefendNot DLL starting. Hardcoded State: %s, Hardcoded Verbose: %s, Hardcoded AV Name: '%ls'",
              (G_DEFENDNOT_ENABLED ? "ON" : "OFF"),
              (G_VERBOSE_LOGGING ? "true" : "false"),
              G_AV_DISPLAY_NAME);

        logln("init: {:#x}", com_checked(CoInitialize(nullptr)));

        auto inst = IWscAVStatus::get();

        logln("unregister: {:#x}", com_retry_while_pending([&inst]() -> HRESULT { return inst->Unregister(); }) & 0xFFFFFFFF);
        
        if (!G_DEFENDNOT_ENABLED) { // Check the hardcoded boolean
            logln("Hardcoded state is OFF. Exiting after unregister attempt.");
            if (inst) inst->Release();
            CoUninitialize();
            return;
        }

        if (wcslen(G_AV_DISPLAY_NAME) == 0) {
            logln("Hardcoded AV Name is empty! This is an error.");
            throw std::runtime_error("Hardcoded AV Name can not be empty!");
        }

        BSTR name_bstr = SysAllocString(G_AV_DISPLAY_NAME); // Directly use the wide string
        if (!name_bstr && wcslen(G_AV_DISPLAY_NAME) > 0) {
             logln("SysAllocString failed for AV name!");
             throw std::runtime_error("SysAllocString failed for AV name");
        }
        
        defer->void {
            if (name_bstr) SysFreeString(name_bstr);
        };

        logln("register: {:#x}", com_checked(inst->Register(name_bstr, name_bstr, 0, 0)));
        logln("update: {:#x}", com_checked(inst->UpdateStatus(WSCSecurityProductState::ON, 3)));

        if (inst) inst->Release();
        CoUninitialize();
        logln("DefendNot DLL startup complete.");
    }
} // namespace defendnot
