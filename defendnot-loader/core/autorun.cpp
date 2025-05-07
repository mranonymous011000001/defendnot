#include "core/core.hpp"

#include "shared/ctx.hpp"
#include "shared/defer.hpp"
#include "shared/names.hpp"

#include <print>
#include <stdexcept>

#include <comdef.h>
#include <taskschd.h>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")
#pragma comment(lib, "ole32.lib")

namespace loader {
    namespace {
        constexpr std::string_view kTaskName = names::kProjectName;

        template <typename Callable>
        [[nodiscard]] bool with_service(Callable&& callback) {
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(hr)) {
                return false;
            }

            defer->void {
                CoUninitialize();
            };

            ITaskService* service = nullptr;
            hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, reinterpret_cast<void**>(&service));
            if (FAILED(hr)) {
                return false;
            }

            defer->void {
                service->Release();
            };

            hr = service->Connect(VARIANT{}, VARIANT{}, VARIANT{}, VARIANT{});
            if (FAILED(hr)) {
                return false;
            }

            ITaskFolder* root_folder = nullptr;
            hr = service->GetFolder(BSTR(L"\\"), &root_folder);
            if (FAILED(hr)) {
                return false;
            }

            defer->void {
                root_folder->Release();
            };

            root_folder->DeleteTask(BSTR(kTaskName.data()), 0);
            return callback(service, root_folder);
        }
    } // namespace

    [[nodiscard]] bool add_to_autorun() {
        const auto bin_path = shared::get_this_module_path();

        return with_service([bin_path](ITaskService* service, ITaskFolder* folder) -> bool {
            ITaskDefinition* task = nullptr;
            auto hr = service->NewTask(0, &task);
            if (FAILED(hr)) {
                return false;
            }

            defer->void {
                task->Release();
            };

            IRegistrationInfo* reg_info = nullptr;
            hr = task->get_RegistrationInfo(&reg_info);
            if (FAILED(hr)) {
                return false;
            }

            defer->void {
                reg_info->Release();
            };
            reg_info->put_Author(_bstr_t(names::kRepoUrl.data()));

            IPrincipal* pPrincipal = nullptr;
            hr = task->get_Principal(&pPrincipal);
            if (FAILED(hr)) {
                return false;
            }

            defer->void {
                pPrincipal->Release();
            };

            ITriggerCollection* trigger_collection = nullptr;
            hr = task->get_Triggers(&trigger_collection);
            if (FAILED(hr)) {
                return false;
            }

            defer->void {
                trigger_collection->Release();
            };

            ITrigger* trigger = nullptr;
            hr = trigger_collection->Create(TASK_TRIGGER_LOGON, &trigger);
            if (FAILED(hr)) {
                return false;
            }

            defer->void {
                trigger->Release();
            };

            IActionCollection* action_collection = nullptr;
            hr = task->get_Actions(&action_collection);
            if (FAILED(hr)) {
                return false;
            }

            defer->void {
                action_collection->Release();
            };

            IAction* action = nullptr;
            hr = action_collection->Create(TASK_ACTION_EXEC, &action);
            if (FAILED(hr)) {
                return false;
            }

            defer->void {
                action->Release();
            };

            IExecAction* exec_action = nullptr;
            hr = action->QueryInterface(IID_IExecAction, (void**)&exec_action);
            if (FAILED(hr)) {
                return false;
            }

            defer->void {
                exec_action->Release();
            };

            ITaskSettings* settings = nullptr;
            hr = task->get_Settings(&settings);
            if (FAILED(hr)) {
                return false;
            }

            defer->void {
                settings->Release();
            };

            pPrincipal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
            settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
            settings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
            exec_action->put_Path(_bstr_t(bin_path.string().c_str()));
            exec_action->put_Arguments(_bstr_t("--from-autorun"));

            IRegisteredTask* registered_task = nullptr;
            hr = folder->RegisterTaskDefinition(_bstr_t(kTaskName.data()), task, TASK_CREATE_OR_UPDATE, VARIANT{}, VARIANT{}, TASK_LOGON_INTERACTIVE_TOKEN,
                                                _variant_t(L""), &registered_task);

            defer->void {
                registered_task->Release();
            };
            return SUCCEEDED(hr);
        });
    }

    [[nodiscard]] bool remove_from_autorun() {
        return with_service([]<typename... TArgs>(TArgs...) -> bool { return true; });
    }
} // namespace loader
