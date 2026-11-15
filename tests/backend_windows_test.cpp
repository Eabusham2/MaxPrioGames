#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "engine.hpp"
#include <windows.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool await(const std::function<bool()>& check, DWORD timeout = 10000) {
    const auto deadline = GetTickCount64() + timeout;
    do { if (check()) return true; Sleep(40); } while (GetTickCount64() < deadline);
    return check();
}
struct Child {
    HANDLE event = nullptr;
    PROCESS_INFORMATION process{};
    ~Child() {
        if (event) SetEvent(event);
        if (process.hProcess) {
            SetPriorityClass(process.hProcess, NORMAL_PRIORITY_CLASS);
            if (WaitForSingleObject(process.hProcess, 3000) == WAIT_TIMEOUT) TerminateProcess(process.hProcess, 8);
            CloseHandle(process.hProcess);
        }
        if (process.hThread) CloseHandle(process.hThread);
        if (event) CloseHandle(event);
    }
};
bool readSets(HANDLE process, std::vector<ULONG>& sets) {
    ULONG count = 0;
    if (!GetProcessDefaultCpuSets(process, nullptr, 0, &count) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) return false;
    sets.resize(count);
    if (count && !GetProcessDefaultCpuSets(process, sets.data(), count, &count)) return false;
    sets.resize(count); std::sort(sets.begin(), sets.end()); return true;
}
ULONG firstCpuSet(HANDLE process) {
    ULONG required = 0;
    GetSystemCpuSetInformation(nullptr, 0, &required, process, 0);
    if (!required || required > 1024 * 1024) return 0xffffffffu;
    std::vector<BYTE> data(required);
    if (!GetSystemCpuSetInformation(reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(data.data()), required, &required, process, 0)) return 0xffffffffu;
    for (std::size_t pos = 0; pos + sizeof(SYSTEM_CPU_SET_INFORMATION) <= required;) {
        const auto* info = reinterpret_cast<const SYSTEM_CPU_SET_INFORMATION*>(data.data() + pos);
        if (info->Size < sizeof(SYSTEM_CPU_SET_INFORMATION) || info->Size > required - pos) break;
        if (info->Type == CpuSetInformation && (!info->CpuSet.Allocated || info->CpuSet.AllocatedToTargetProcess)) return info->CpuSet.Id;
        pos += info->Size;
    }
    return 0xffffffffu;
}
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    try {
        Child child;
        const auto eventName = L"Local\\MaxPrioGames.BackendTest." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());
        child.event = CreateEventW(nullptr, TRUE, FALSE, eventName.c_str());
        require(child.event != nullptr, "could not create test stop event");
        std::wstring command = L"\"" + std::wstring(argv[1]) + L"\" \"" + eventName + L"\"";
        STARTUPINFOW startup{}; startup.cb = sizeof(startup);
        require(CreateProcessW(argv[1], command.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED | CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS,
                              nullptr, nullptr, &startup, &child.process) != FALSE, "could not start disposable sleeping helper");
        const bool oneGroup = GetActiveProcessorGroupCount() == 1;
        DWORD_PTR initial = 0, allowed = 0;
        require(GetProcessAffinityMask(child.process.hProcess, &initial, &allowed) != FALSE, "read initial affinity");
        const DWORD_PTR narrow = oneGroup ? allowed & (~allowed + 1) : initial;
        if (oneGroup) require(SetProcessAffinityMask(child.process.hProcess, narrow) != FALSE, "set helper-only narrow affinity");
        const ULONG setId = firstCpuSet(child.process.hProcess);
        const bool selectedSet = setId != 0xffffffffu && SetProcessDefaultCpuSets(child.process.hProcess, &setId, 1);
        std::vector<ULONG> originalSets;
        require(readSets(child.process.hProcess, originalSets), "read initial CPU set selection");
        require(ResumeThread(child.process.hThread) != static_cast<DWORD>(-1), "resume sleeping helper");
        mpg::Settings settings;
        settings.intervalSeconds = 1;
        settings.detectInstalledGames = false;
        settings.includeGameServices = false;
        for (const auto& entry : mpg::catalog()) settings.disabledDefaults.push_back(entry.id);
        settings.customPrograms.push_back({L"Disposable test helper", argv[1]});
        mpg::Monitor monitor([] {});
        monitor.start(settings);
        auto realtime = [&] {
            DWORD_PTR current = 0, system = 0;
            std::vector<ULONG> sets;
            return GetPriorityClass(child.process.hProcess) == REALTIME_PRIORITY_CLASS &&
                GetProcessAffinityMask(child.process.hProcess, &current, &system) &&
                (!oneGroup || current == allowed) && readSets(child.process.hProcess, sets) && sets.empty();
        };
        require(await(realtime), "actual Realtime/all-allowed-affinity readback failed");
        require(await([&] { const auto snapshot = monitor.snapshot(); return snapshot.realtimeCount == 1 &&
            std::any_of(snapshot.processes.begin(), snapshot.processes.end(), [&](const mpg::RunningProcess& p) {
                return p.pid == child.process.dwProcessId && p.managed && p.realtime;
            }); }), "count must reflect exactly the managed Realtime helper");
        settings.excludedPaths.push_back(argv[1]);
        monitor.update(settings);
        auto restored = [&](DWORD priority) {
            DWORD_PTR current = 0, system = 0;
            std::vector<ULONG> sets;
            return GetPriorityClass(child.process.hProcess) == priority &&
                GetProcessAffinityMask(child.process.hProcess, &current, &system) &&
                (!oneGroup || current == narrow) && readSets(child.process.hProcess, sets) && sets == originalSets;
        };
        require(await([&] { return restored(NORMAL_PRIORITY_CLASS); }), "removing helper did not restore original policy");
        require(await([&] { return monitor.snapshot().realtimeCount == 0; }), "removed helper still counted");
        settings.excludedPaths.clear(); settings.intervalSeconds = 60;
        monitor.update(settings);
        require(await(realtime), "re-enable helper failed");
        require(SetPriorityClass(child.process.hProcess, HIGH_PRIORITY_CLASS) != FALSE, "simulate another tool's priority change");
        monitor.stop();
        require(restored(HIGH_PRIORITY_CLASS), "quit overwrote another tool's priority or failed affinity restore");
        monitor.start(settings);
        require(await(realtime), "restart monitor failed");
        monitor.stop();
        require(restored(HIGH_PRIORITY_CLASS), "normal quit did not restore captured original values");
        std::cout << "Actual Realtime readback, all allowed affinity, exclusion, count, restart, and conditional restoration passed; CPU-set fixture="
                  << (selectedSet ? "selected and restored" : "default selection") << "; processor groups=" << GetActiveProcessorGroupCount() << '\n';
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
