#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace mpg {
struct CatalogEntry {
    std::wstring id;
    std::wstring name;
    std::vector<std::wstring> executables;
    std::vector<std::wstring> pathHints;
    bool service = false;
    std::vector<unsigned> steamAppIds;
};
struct CustomProgram { std::wstring name; std::wstring path; };
struct Settings {
    unsigned intervalSeconds = 3;
    bool detectInstalledGames = true;
    bool includeGameServices = true;
    std::vector<std::wstring> disabledDefaults;
    std::vector<CustomProgram> customPrograms;
    std::vector<std::wstring> excludedPaths;
};
struct RunningProcess {
    std::uint32_t pid = 0;
    std::wstring name;
    std::wstring path;
    bool realtime = false;
    bool managed = false;
    std::wstring catalogId;
};
struct Snapshot {
    std::vector<RunningProcess> processes;
    unsigned realtimeCount = 0;
    std::uint64_t scanCount = 0;
};
const std::vector<CatalogEntry>& catalog();
}
