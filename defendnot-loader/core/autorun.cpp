#include "core/core.hpp"

#include "shared/ctx.hpp"
#include "shared/names.hpp"

#include <memory>
#include <print>
#include <stdexcept>
#include <type_traits>

#include <comdef.h>
#include <taskschd.h>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")
#pragma comment(lib, "ole32.lib")

namespace loader {
    namespace {
        constexpr std::string_view kTaskName = names::kProjectName;

        /// A very basic implementation, a lot of stuff is missing
        template <typename Ty>
        class ComPtr {
        public:
            ComPtr() = default;
            explicit ComPtr(Ty* ptr): ptr_(ptr) { }

            ~ComPtr() {
                if (ptr_ != nullptr) {
                    ptr_->Release();
                }
            }

            ComPtr(const ComPtr&) = delete;
            ComPtr& operator=(const ComPtr&) = delete;

            [[nodiscard]] Ty* get() const {
                return ptr_;
            }

            [[nodiscard]] Ty* operator->() const {
                return ptr_;
            }

            [[nodiscard]] Ty** ref_to_ptr() {
                return &ptr_;
            }

        private:
            Ty* ptr_ = nullptr;
        };

        void co_initialize() {
            static std::once_flag fl;
            std::call_once(fl, []() -> void {
                const auto result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

                if (FAILED(result)) {
                    throw std::runtime_error("failed to CoInitializeEx");
                }
            });
        }

        template <typename Callable>
        [[nodiscard]] bool with_service(Callable&& callback) {
            co_initialize();

            ComPtr<ITaskService> service;
            auto hr =
                CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, reinterpret_cast<void**>(service.ref_to_ptr()));
            if (FAILED(hr)) {
                return false;
            }

            hr = service->Connect(VARIANT{}, VARIANT{}, VARIANT{}, VARIANT{});
            if (FAILED(hr)) {
                return false;
            }

            ComPtr<ITaskFolder> root_folder;
            hr = service->GetFolder(BSTR(L"\\"), root_folder.ref_to_ptr());
            if (FAILED(hr)) {
                return false;
            }

            /// Cleanup our task, we will recreate it in the callback if needed
            root_folder->DeleteTask(BSTR(kTaskName.data()), 0);
            return callback(service.get(), root_folder.get());
        }
    } // namespace

    [[nodiscard]] bool add_to_autorun() {
        const auto bin_path = shared::get_this_module_path();

        return with_service([bin_path](ITaskService* service, ITaskFolder* folder) -> bool {
            ComPtr<ITaskDefinition> task;
            auto hr = service->NewTask(0, task.ref_to_ptr());
            if (FAILED(hr)) {
                return false;
            }

            ComPtr<IRegistrationInfo> reg_info;
            hr = task->get_RegistrationInfo(reg_info.ref_to_ptr());
            if (FAILED(hr)) {
                return false;
            }

            ComPtr<IPrincipal> principal;
            hr = task->get_Principal(principal.ref_to_ptr());
            if (FAILED(hr)) {
                return false;
            }

            ComPtr<ITriggerCollection> trigger_collection;
            hr = task->get_Triggers(trigger_collection.ref_to_ptr());
            if (FAILED(hr)) {
                return false;
            }

            ComPtr<ITrigger> trigger;
            hr = trigger_collection->Create(TASK_TRIGGER_LOGON, trigger.ref_to_ptr());
            if (FAILED(hr)) {
                return false;
            }

            ComPtr<IActionCollection> action_collection;
            hr = task->get_Actions(action_collection.ref_to_ptr());
            if (FAILED(hr)) {
                return false;
            }

            ComPtr<IAction> action;
            hr = action_collection->Create(TASK_ACTION_EXEC, action.ref_to_ptr());
            if (FAILED(hr)) {
                return false;
            }

            ComPtr<IExecAction> exec_action;
            hr = action->QueryInterface(IID_IExecAction, reinterpret_cast<void**>(exec_action.ref_to_ptr()));
            if (FAILED(hr)) {
                return false;
            }

            ComPtr<ITaskSettings> settings;
            hr = task->get_Settings(settings.ref_to_ptr());
            if (FAILED(hr)) {
                return false;
            }

            /// Elevated, when any user logs in
            principal->put_GroupId(bstr_t("BUILTIN\\Users"));
            principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
            principal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);

            /// Info
            reg_info->put_Author(bstr_t(names::kRepoUrl.data()));

            /// Start even if we're on batteries
            settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
            settings->put_StopIfGoingOnBatteries(VARIANT_FALSE);

            /// Binary
            exec_action->put_Path(bstr_t(bin_path.string().c_str()));
            exec_action->put_Arguments(bstr_t("--from-autorun"));

            /// Register the task and we are done
            ComPtr<IRegisteredTask> registered_task;
            hr = folder->RegisterTaskDefinition(bstr_t(kTaskName.data()), task.get(), TASK_CREATE_OR_UPDATE, VARIANT{}, VARIANT{},
                                                TASK_LOGON_INTERACTIVE_TOKEN, variant_t(L""), registered_task.ref_to_ptr());
            return SUCCEEDED(hr);
        });
    }

    [[nodiscard]] bool remove_from_autorun() {
        return with_service([]<typename... TArgs>(TArgs...) -> bool { return true; });
    }
} // namespace loader
