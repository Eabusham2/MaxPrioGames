#include "startup.hpp"
#include <windows.h>
#include <taskschd.h>
#include <sddl.h>
#include <vector>
#include <string>

namespace mpg::startup {
namespace {
template<class T> class ComPtr {
    T* value_ = nullptr;
public:
    ~ComPtr() { if (value_) value_->Release(); }
    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    T* operator->() const { return value_; }
    T* get() const { return value_; }
    T** put() { if (value_) { value_->Release(); value_ = nullptr; } return &value_; }
};
class Bstr {
    BSTR value_ = nullptr;
public:
    explicit Bstr(const wchar_t* s) : value_(SysAllocString(s)) {}
    explicit Bstr(const std::wstring& s) : Bstr(s.c_str()) {}
    ~Bstr() { SysFreeString(value_); }
    operator BSTR() const { return value_; }
};
class Apartment {
    HRESULT result_;
public:
    Apartment() : result_(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)) {}
    ~Apartment() { if (SUCCEEDED(result_)) CoUninitialize(); }
    bool valid() const { return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE; }
};
std::wstring sid() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) return {};
    DWORD bytes = 0;
    GetTokenInformation(token,TokenUser,nullptr,0,&bytes);
    std::vector<BYTE> buffer(bytes);
    if (!bytes || !GetTokenInformation(token,TokenUser,buffer.data(),bytes,&bytes)) {
        CloseHandle(token); return {};
    }
    LPWSTR value = nullptr;
    const auto* user = reinterpret_cast<TOKEN_USER*>(buffer.data());
    const BOOL ok = ConvertSidToStringSidW(user->User.Sid,&value);
    std::wstring result = ok && value ? value : L"";
    if (value) LocalFree(value);
    CloseHandle(token);
    return result;
}
std::wstring taskName() { return L"MaxPrioGames-" + sid(); }
std::wstring message(HRESULT code) {
    wchar_t* value = nullptr;
    const DWORD size = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,static_cast<DWORD>(code),0,reinterpret_cast<LPWSTR>(&value),0,nullptr);
    std::wstring result;
    if (size && value) result = value;
    else {
        wchar_t hex[16]{};
        swprintf_s(hex,L"0x%08lx",static_cast<unsigned long>(code));
        result = hex;
    }
    if (value) LocalFree(value);
    return result;
}
bool fail(std::wstring& error, const wchar_t* step, HRESULT code) {
    error = std::wstring(step) + L": " + message(code);
    return false;
}
HRESULT connect(ComPtr<ITaskService>& service, ComPtr<ITaskFolder>& folder) {
    HRESULT hr = CoInitializeSecurity(nullptr,-1,nullptr,nullptr,
        RPC_C_AUTHN_LEVEL_PKT_PRIVACY,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE,nullptr);
    // Another COM component can initialize this first.
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) return hr;
    hr = CoCreateInstance(CLSID_TaskScheduler,nullptr,CLSCTX_INPROC_SERVER,
        IID_ITaskService,reinterpret_cast<void**>(service.put()));
    if (FAILED(hr)) return hr;
    VARIANT empty; VariantInit(&empty);
    hr = service->Connect(empty,empty,empty,empty);
    if (FAILED(hr)) return hr;
    return service->GetFolder(Bstr(L"\\"),folder.put());
}
bool matchingTask(IRegisteredTask* task, bool requireEnabled) {
    if (!task) return false;
    if (requireEnabled) {
        VARIANT_BOOL enabled = VARIANT_FALSE;
        if (FAILED(task->get_Enabled(&enabled)) || enabled != VARIANT_TRUE) return false;
    }
    ComPtr<ITaskDefinition> definition;
    ComPtr<IActionCollection> actions;
    ComPtr<IAction> action;
    ComPtr<IExecAction> exec;
    if (FAILED(task->get_Definition(definition.put())) ||
        FAILED(definition->get_Actions(actions.put()))) return false;
    LONG count = 0;
    if (FAILED(actions->get_Count(&count)) || count != 1 ||
        FAILED(actions->get_Item(1,action.put())) ||
        FAILED(action->QueryInterface(IID_IExecAction,reinterpret_cast<void**>(exec.put())))) return false;
    BSTR path = nullptr, args = nullptr;
    const HRESULT pathResult = exec->get_Path(&path);
    const HRESULT argsResult = exec->get_Arguments(&args);
    const bool matches = SUCCEEDED(pathResult) && SUCCEEDED(argsResult) && path && args &&
        _wcsicmp(path,executablePath().c_str()) == 0 && wcscmp(args,L"--startup") == 0;
    SysFreeString(path); SysFreeString(args);
    return matches;
}
}
std::wstring executablePath() {
    std::vector<wchar_t> buffer(32768,0);
    const DWORD count = GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
    if (!count || count >= buffer.size()) return {};
    return std::wstring(buffer.data(),count);
}
bool isElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD bytes = 0;
    const BOOL ok = GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&bytes);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated != 0;
}
bool isEnabled() {
    Apartment apartment;
    if (!apartment.valid()) return false;
    ComPtr<ITaskService> service;
    ComPtr<ITaskFolder> folder;
    if (FAILED(connect(service,folder))) return false;
    ComPtr<IRegisteredTask> task;
    if (FAILED(folder->GetTask(Bstr(taskName()),task.put()))) return false;
    return matchingTask(task.get(),true);
}
bool setEnabled(bool enable, std::wstring& error) {
    error.clear();
    if (!isElevated()) {
        error = L"Run MaxPrioGames as administrator to change startup.";
        return false;
    }
    const auto user = sid();
    const auto path = executablePath();
    if (user.empty() || path.empty()) {
        error = L"Could not identify the current user or executable path.";
        return false;
    }
    Apartment apartment;
    if (!apartment.valid()) { error = L"Could not initialize Windows Task Scheduler."; return false; }
    ComPtr<ITaskService> service;
    ComPtr<ITaskFolder> folder;
    HRESULT hr = connect(service,folder);
    if (FAILED(hr)) return fail(error,L"Could not connect to Task Scheduler",hr);
    const auto name = L"MaxPrioGames-" + user;
    if (!enable) {
        ComPtr<IRegisteredTask> existing;
        hr = folder->GetTask(Bstr(name),existing.put());
        if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
            hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)) return true;
        if (FAILED(hr)) return fail(error,L"Could not inspect startup task",hr);
        // Another installed/portable copy may now own startup; leave its registration alone.
        if (!matchingTask(existing.get(),false)) return true;
        hr = folder->DeleteTask(Bstr(name),0);
        return SUCCEEDED(hr) || fail(error,L"Could not remove startup task",hr);
    }
    ComPtr<ITaskDefinition> definition;
    hr = service->NewTask(0,definition.put());
    if (FAILED(hr)) return fail(error,L"Could not define startup task",hr);
    ComPtr<IRegistrationInfo> registration;
    hr = definition->get_RegistrationInfo(registration.put());
    if (FAILED(hr)) return fail(error,L"Could not describe startup task",hr);
    hr = registration->put_Author(Bstr(L"MaxPrioGames"));
    if (FAILED(hr)) return fail(error,L"Could not set task author",hr);
    hr = registration->put_Description(Bstr(L"Start MaxPrioGames in the notification area at sign-in. Native game priority and affinity management; no service."));
    if (FAILED(hr)) return fail(error,L"Could not set task description",hr);
    ComPtr<IPrincipal> principal;
    hr = definition->get_Principal(principal.put());
    if (FAILED(hr)) return fail(error,L"Could not select startup user",hr);
    hr = principal->put_UserId(Bstr(user));
    if (FAILED(hr)) return fail(error,L"Could not set startup user",hr);
    hr = principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
    if (FAILED(hr)) return fail(error,L"Could not set interactive startup",hr);
    hr = principal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
    if (FAILED(hr)) return fail(error,L"Could not configure elevated process management",hr);
    ComPtr<ITaskSettings> settings;
    hr = definition->get_Settings(settings.put());
    if (FAILED(hr)) return fail(error,L"Could not configure startup settings",hr);
#define MPG_TASK_SETTING(call) do { hr = (call); if (FAILED(hr)) return fail(error,L"Could not configure startup settings",hr); } while (0)
    MPG_TASK_SETTING(settings->put_Enabled(VARIANT_TRUE));
    MPG_TASK_SETTING(settings->put_AllowDemandStart(VARIANT_TRUE));
    MPG_TASK_SETTING(settings->put_StartWhenAvailable(VARIANT_TRUE));
    MPG_TASK_SETTING(settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE));
    MPG_TASK_SETTING(settings->put_StopIfGoingOnBatteries(VARIANT_FALSE));
    MPG_TASK_SETTING(settings->put_RunOnlyIfIdle(VARIANT_FALSE));
    MPG_TASK_SETTING(settings->put_RunOnlyIfNetworkAvailable(VARIANT_FALSE));
    MPG_TASK_SETTING(settings->put_WakeToRun(VARIANT_FALSE));
    MPG_TASK_SETTING(settings->put_AllowHardTerminate(VARIANT_FALSE));
    MPG_TASK_SETTING(settings->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW));
    MPG_TASK_SETTING(settings->put_ExecutionTimeLimit(Bstr(L"PT0S")));
#undef MPG_TASK_SETTING
    ComPtr<ITriggerCollection> triggers;
    ComPtr<ITrigger> trigger;
    ComPtr<ILogonTrigger> logon;
    hr = definition->get_Triggers(triggers.put());
    if (FAILED(hr)) return fail(error,L"Could not create sign-in trigger",hr);
    hr = triggers->Create(TASK_TRIGGER_LOGON,trigger.put());
    if (FAILED(hr)) return fail(error,L"Could not create sign-in trigger",hr);
    hr = trigger->QueryInterface(IID_ILogonTrigger,reinterpret_cast<void**>(logon.put()));
    if (FAILED(hr)) return fail(error,L"Could not configure sign-in trigger",hr);
    hr = logon->put_UserId(Bstr(user));
    if (FAILED(hr)) return fail(error,L"Could not scope sign-in trigger",hr);
    hr = logon->put_Delay(Bstr(L"PT5S"));
    if (FAILED(hr)) return fail(error,L"Could not set sign-in delay",hr);
    ComPtr<IActionCollection> actions;
    ComPtr<IAction> action;
    ComPtr<IExecAction> exec;
    hr = definition->get_Actions(actions.put());
    if (FAILED(hr)) return fail(error,L"Could not configure startup action",hr);
    hr = actions->Create(TASK_ACTION_EXEC,action.put());
    if (FAILED(hr)) return fail(error,L"Could not configure startup action",hr);
    hr = action->QueryInterface(IID_IExecAction,reinterpret_cast<void**>(exec.put()));
    if (FAILED(hr)) return fail(error,L"Could not configure startup command",hr);
    hr = exec->put_Path(Bstr(path));
    if (FAILED(hr)) return fail(error,L"Could not set executable path",hr);
    hr = exec->put_Arguments(Bstr(L"--startup"));
    if (FAILED(hr)) return fail(error,L"Could not set startup argument",hr);
    const auto slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        hr = exec->put_WorkingDirectory(Bstr(path.substr(0,slash)));
        if (FAILED(hr)) return fail(error,L"Could not set working directory",hr);
    }
    VARIANT userVariant, empty;
    VariantInit(&userVariant); VariantInit(&empty);
    userVariant.vt = VT_BSTR;
    userVariant.bstrVal = SysAllocString(user.c_str());
    if (!userVariant.bstrVal) return fail(error,L"Could not allocate startup user",E_OUTOFMEMORY);
    ComPtr<IRegisteredTask> registered;
    hr = folder->RegisterTaskDefinition(Bstr(name),definition.get(),
        TASK_CREATE_OR_UPDATE | TASK_IGNORE_REGISTRATION_TRIGGERS,userVariant,empty,
        TASK_LOGON_INTERACTIVE_TOKEN,empty,registered.put());
    VariantClear(&userVariant);
    if (FAILED(hr)) return fail(error,L"Could not register startup task",hr);
    if (!matchingTask(registered.get(),true)) {
        error = L"The saved startup task does not match this executable.";
        return false;
    }
    return true;
}
}
