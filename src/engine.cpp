#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "engine.hpp"
#include "policy.hpp"
#include "discovery.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <utility>

namespace mpg {
namespace {
struct Handle {
    HANDLE value = nullptr;
    Handle() = default;
    explicit Handle(HANDLE h) : value(h == INVALID_HANDLE_VALUE ? nullptr : h) {}
    ~Handle() { if (value) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : value(other.value) { other.value = nullptr; }
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) { if (value) CloseHandle(value); value = other.value; other.value = nullptr; }
        return *this;
    }
    explicit operator bool() const { return value != nullptr; }
};
std::uint64_t createdAt(HANDLE process) {
    FILETIME created{}, exit{}, kernel{}, user{};
    if (!GetProcessTimes(process, &created, &exit, &kernel, &user)) return 0;
    return (static_cast<std::uint64_t>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
}
std::wstring imagePath(HANDLE process) {
    std::vector<wchar_t> text(32768);
    DWORD count = static_cast<DWORD>(text.size());
    return QueryFullProcessImageNameW(process, 0, text.data(), &count) ? std::wstring(text.data(), count) : std::wstring{};
}
void enablePriorityPrivilege() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) return;
    Handle guard(token);
    TOKEN_PRIVILEGES privileges{};
    privileges.PrivilegeCount = 1;
    if (!LookupPrivilegeValueW(nullptr, SE_INC_BASE_PRIORITY_NAME, &privileges.Privileges[0].Luid)) return;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    // Enable only a privilege already present in our own account token.
    AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(privileges), nullptr, nullptr);
}
bool cpuSets(HANDLE process, std::vector<ULONG>& output) {
    ULONG required = 0;
    const BOOL first = GetProcessDefaultCpuSets(process, nullptr, 0, &required);
    if (!first && GetLastError() != ERROR_INSUFFICIENT_BUFFER) return false;
    if (required > 4096) return false;
    output.resize(required);
    if (!required) return true;
    ULONG actual = required;
    if (!GetProcessDefaultCpuSets(process, output.data(), required, &actual) || actual > required) return false;
    output.resize(actual);
    std::sort(output.begin(), output.end());
    return true;
}
struct Record {
    DWORD pid = 0;
    std::wstring executable, path;
    Handle query, control;
    std::uint64_t created = 0;
    bool target = false;
    Match match;
    std::uint64_t matchedSettings = 0, matchedRoots = 0;
    DWORD originalPriority = 0, appliedPriority = 0;
    bool priorityChanged = false;
    DWORD_PTR originalAffinity = 0, appliedAffinity = 0;
    bool affinityCaptured = false, affinityChanged = false;
    std::vector<ULONG> originalCpuSets;
    bool cpuSetsCaptured = false, cpuSetsChanged = false;
    ULONGLONG nextOpen = 0, nextPriority = 0, nextAffinity = 0, nextCpuSets = 0;
    unsigned openFailures = 0;
};
bool sameProcess(const Record& record) {
    return record.control && record.created && WaitForSingleObject(record.control.value, 0) == WAIT_TIMEOUT &&
           createdAt(record.control.value) == record.created;
}
void restore(Record& record) {
    if (sameProcess(record)) {
        // A different current value belongs to the process or another tool.
        if (record.priorityChanged && record.originalPriority && GetPriorityClass(record.control.value) == record.appliedPriority)
            SetPriorityClass(record.control.value, record.originalPriority);
        if (record.affinityChanged) {
            DWORD_PTR current = 0, allowed = 0;
            if (GetProcessAffinityMask(record.control.value, &current, &allowed) && current == record.appliedAffinity &&
                record.originalAffinity && (record.originalAffinity & allowed) == record.originalAffinity)
                SetProcessAffinityMask(record.control.value, record.originalAffinity);
        }
        if (record.cpuSetsChanged) {
            std::vector<ULONG> current;
            if (cpuSets(record.control.value, current) && current.empty())
                SetProcessDefaultCpuSets(record.control.value, record.originalCpuSets.data(), static_cast<ULONG>(record.originalCpuSets.size()));
        }
    }
    record.control = Handle{};
    record.originalPriority = record.appliedPriority = 0;
    record.priorityChanged = false;
    record.originalAffinity = record.appliedAffinity = 0;
    record.affinityCaptured = record.affinityChanged = false;
    record.originalCpuSets.clear();
    record.cpuSetsCaptured = record.cpuSetsChanged = false;
    record.nextOpen = record.nextPriority = record.nextAffinity = record.nextCpuSets = 0;
    record.openFailures = 0;
    record.target = false;
}
bool control(Record& record, ULONGLONG now) {
    if (record.control) return true;
    if (now < record.nextOpen || !record.created) return false;
    Handle opened(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION | PROCESS_SET_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, record.pid));
    if (!opened || createdAt(opened.value) != record.created) {
        const unsigned delay = (std::min)(300u, 15u << (std::min)(record.openFailures++, 4u));
        record.nextOpen = now + static_cast<ULONGLONG>(delay) * 1000;
        return false;
    }
    record.control = std::move(opened);
    record.originalPriority = GetPriorityClass(record.control.value);
    record.openFailures = 0;
    return true;
}
void apply(Record& record, ULONGLONG now, bool oneGroup) {
    if (!control(record, now)) return;
    const DWORD currentPriority = GetPriorityClass(record.control.value);
    if (currentPriority && record.originalPriority && currentPriority != REALTIME_PRIORITY_CLASS && now >= record.nextPriority) {
        if (SetPriorityClass(record.control.value, REALTIME_PRIORITY_CLASS)) {
            const DWORD actual = GetPriorityClass(record.control.value);
            if (actual == REALTIME_PRIORITY_CLASS) {
                record.appliedPriority = actual;
                record.priorityChanged = record.originalPriority != actual;
            } else {
                // A token can produce HIGH instead of Realtime. Undo that alternate
                // result and never count it as Realtime or use it as a fallback.
                if (actual && actual != currentPriority && !SetPriorityClass(record.control.value, currentPriority)) {
                    record.appliedPriority = actual;
                    record.priorityChanged = record.originalPriority != actual;
                }
                record.nextPriority = now + 30000;
            }
        } else record.nextPriority = now + 30000;
    }
    if (oneGroup && now >= record.nextAffinity) {
        DWORD_PTR current = 0, allowed = 0;
        if (GetProcessAffinityMask(record.control.value, &current, &allowed) && current && allowed) {
            if (!record.affinityCaptured) { record.originalAffinity = current; record.affinityCaptured = true; }
            if (current != allowed) {
                if (SetProcessAffinityMask(record.control.value, allowed)) {
                    record.appliedAffinity = allowed;
                    record.affinityChanged = record.originalAffinity != allowed;
                } else record.nextAffinity = now + 30000;
            }
        } else record.nextAffinity = now + 30000;
    }
    // Empty CPU-set selection permits all scheduler-allowed sets across groups.
    // Never impose a legacy 64-bit process mask on a multi-group machine.
    if (now >= record.nextCpuSets) {
        std::vector<ULONG> current;
        if (cpuSets(record.control.value, current)) {
            if (!record.cpuSetsCaptured) { record.originalCpuSets = current; record.cpuSetsCaptured = true; }
            if (!current.empty()) {
                if (SetProcessDefaultCpuSets(record.control.value, nullptr, 0)) record.cpuSetsChanged = !record.originalCpuSets.empty();
                else record.nextCpuSets = now + 30000;
            }
        } else record.nextCpuSets = now + 30000;
    }
}
bool equivalent(const Snapshot& a, const Snapshot& b) {
    if (a.realtimeCount != b.realtimeCount || a.processes.size() != b.processes.size()) return false;
    for (std::size_t i = 0; i < a.processes.size(); ++i) {
        const auto& x = a.processes[i]; const auto& y = b.processes[i];
        if (x.pid != y.pid || x.name != y.name || x.path != y.path || x.managed != y.managed ||
            x.realtime != y.realtime || x.catalogId != y.catalogId) return false;
    }
    return true;
}
}

struct Monitor::Impl {
    struct State {
        mutable std::mutex mutex;
        std::condition_variable changed;
        std::function<void()> notify;
        Settings settings;
        Snapshot latest;
        std::mutex recordsMutex;
        std::map<DWORD, Record> records;
        std::uint64_t version = 1;
        bool stopping = false, update = true;
    };
    std::shared_ptr<State> state = std::make_shared<State>();
    std::thread thread;
    explicit Impl(std::function<void()> callback) { state->notify = std::move(callback); }
    static void run(std::shared_ptr<State> state) {
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
        enablePriorityPrivilege();
        std::vector<InstalledRoot> roots;
        ULONGLONG nextDiscovery = 0;
        std::uint64_t scans = 0;
        std::uint64_t rootsVersion = 0;
        bool first = true;
        for (;;) {
            Settings settings;
            std::uint64_t settingsVersion = 0;
            {
                std::unique_lock<std::mutex> lock(state->mutex);
                if (!first) state->changed.wait_for(lock, std::chrono::seconds(state->settings.intervalSeconds), [&] { return state->stopping || state->update; });
                if (state->stopping) break;
                settings = state->settings;
                settingsVersion = state->version;
                state->update = false;
            }
            first = false;
            const auto now = GetTickCount64();
            if (now >= nextDiscovery) {
                try { roots = discoverInstalledGames(); } catch (...) {}
                ++rootsVersion;
                nextDiscovery = now + 300000;
            }
            { std::lock_guard<std::mutex> lock(state->mutex); if (state->stopping) break; }
            Handle processes(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
            if (!processes) continue;
            std::set<DWORD> live;
            PROCESSENTRY32W process{}; process.dwSize = sizeof(process);
            if (!Process32FirstW(processes.value, &process)) continue;
            Snapshot snapshot; snapshot.scanCount = ++scans;
            const bool oneGroup = GetActiveProcessorGroupCount() == 1;
            std::unique_lock<std::mutex> recordsLock(state->recordsMutex);
            auto& records = state->records;
            bool interrupted = false;
            do {
                { std::lock_guard<std::mutex> lock(state->mutex); if (state->stopping) { interrupted = true; break; } }
                if (process.th32ProcessID == 0 || process.th32ProcessID == GetCurrentProcessId()) continue;
                live.insert(process.th32ProcessID);
                auto& record = records[process.th32ProcessID];
                if ((record.query && WaitForSingleObject(record.query.value, 0) != WAIT_TIMEOUT) ||
                    (!record.executable.empty() && record.executable != process.szExeFile)) {
                    if (record.target) restore(record);
                    record = Record{};
                }
                record.pid = process.th32ProcessID;
                record.executable = process.szExeFile;
                if (!record.query && now >= record.nextOpen) {
                    record.query = Handle(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, record.pid));
                    if (record.query) { record.created = createdAt(record.query.value); record.path = imagePath(record.query.value); }
                    else record.nextOpen = now + 60000;
                }
                if (!record.query || record.path.empty()) continue;
                if (record.matchedSettings != settingsVersion || record.matchedRoots != rootsVersion) {
                    record.match = matchProgram(record.path, settings, roots);
                    record.matchedSettings = settingsVersion;
                    record.matchedRoots = rootsVersion;
                }
                const Match& match = record.match;
                if (match.matched) { record.target = true; apply(record, now, oneGroup); }
                else if (record.target) restore(record);
                const DWORD actual = GetPriorityClass(record.query.value);
                RunningProcess item;
                item.pid = record.pid;
                item.name = match.matched ? match.name : record.executable;
                item.path = record.path;
                item.managed = match.matched && static_cast<bool>(record.control);
                item.realtime = actual == REALTIME_PRIORITY_CLASS;
                item.catalogId = match.catalogId;
                if (item.managed && item.realtime) ++snapshot.realtimeCount;
                snapshot.processes.push_back(std::move(item));
            } while (Process32NextW(processes.value, &process));
            if (interrupted) break;
            for (auto it = records.begin(); it != records.end();) {
                if (!live.count(it->first)) {
                    if (it->second.target) restore(it->second);
                    it = records.erase(it);
                }
                else ++it;
            }
            recordsLock.unlock();
            std::sort(snapshot.processes.begin(), snapshot.processes.end(), [](const RunningProcess& a, const RunningProcess& b) { return a.pid < b.pid; });
            std::function<void()> notify;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                const bool changed = !equivalent(state->latest, snapshot);
                state->latest = std::move(snapshot);
                if (!state->stopping && changed) notify = state->notify;
            }
            if (notify) { try { notify(); } catch (...) {} }
        }
        {
            std::lock_guard<std::mutex> lock(state->recordsMutex);
            for (auto& entry : state->records) if (entry.second.target) restore(entry.second);
        }
        SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
    }
};

Monitor::Monitor(std::function<void()> changed) : impl_(std::make_unique<Impl>(std::move(changed))) {}
Monitor::~Monitor() { stop(); }
void Monitor::start(const Settings& settings) {
    if (impl_->thread.joinable()) { update(settings); return; }
    // A previous blocked discovery may have been detached during stop. It retains
    // its permanently stopped State; a restart always gets a new independent State.
    auto fresh = std::make_shared<Impl::State>();
    fresh->notify = impl_->state->notify;
    impl_->state = std::move(fresh);
    {
        std::lock_guard<std::mutex> lock(impl_->state->mutex);
        impl_->state->settings = validSettings(settings) ? settings : Settings{};
        ++impl_->state->version;
        impl_->state->stopping = false;
        impl_->state->update = true;
    }
    impl_->thread = std::thread(&Impl::run, impl_->state);
}
void Monitor::update(const Settings& settings) {
    if (!validSettings(settings)) return;
    {
        std::lock_guard<std::mutex> lock(impl_->state->mutex);
        impl_->state->settings = settings;
        ++impl_->state->version;
        impl_->state->update = true;
    }
    impl_->state->changed.notify_one();
}
Snapshot Monitor::snapshot() const { std::lock_guard<std::mutex> lock(impl_->state->mutex); return impl_->state->latest; }
void Monitor::stop() {
    { std::lock_guard<std::mutex> lock(impl_->state->mutex); impl_->state->stopping = true; }
    impl_->state->changed.notify_one();
    if (!impl_->thread.joinable()) return;
    const HANDLE handle = static_cast<HANDLE>(impl_->thread.native_handle());
    const auto deadline = GetTickCount64() + 5000;
    while (WaitForSingleObject(handle, 0) == WAIT_TIMEOUT && GetTickCount64() < deadline) {
        CancelSynchronousIo(handle);
        WaitForSingleObject(handle, 100);
    }
    if (WaitForSingleObject(handle, 0) == WAIT_OBJECT_0) impl_->thread.join();
    else {
        // Discovery never holds this lock. Even an uncooperative metadata read
        // therefore cannot strand the app's owned scheduling state on exit.
        std::unique_lock<std::mutex> recordsLock(impl_->state->recordsMutex, std::try_to_lock);
        if (recordsLock.owns_lock())
            for (auto& entry : impl_->state->records) if (entry.second.target) restore(entry.second);
        impl_->thread.detach();
    }
}
}
