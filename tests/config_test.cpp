#include "config.hpp"
#include <windows.h>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
int wmain() {
    wchar_t temporary[32768]{};
    const auto size = GetTempPathW(32768, temporary);
    if (!size || size >= 32768) return 2;
    const auto path = std::wstring(temporary) + L"MaxPrioGames.ConfigTest." + std::to_wstring(GetCurrentProcessId());
    SetEnvironmentVariableW(L"MAXPRIOGAMES_CONFIG_DIR", path.c_str());
    try {
        mpg::Settings settings;
        settings.intervalSeconds = 7;
        settings.detectInstalledGames = false;
        settings.includeGameServices = false;
        settings.disabledDefaults = {L"steam-730", L"custom-name"};
        settings.customPrograms = {{L"Name = [test] \u0627", L"C:\\Games with spaces\\Test[1]=.exe"}};
        settings.excludedPaths = {L"D:\\Excluded\\Game.exe"};
        require(mpg::saveSettings(settings), "save settings");
        const auto loaded = mpg::loadSettings();
        require(loaded.intervalSeconds == 7 && !loaded.detectInstalledGames && !loaded.includeGameServices, "general roundtrip");
        require(loaded.disabledDefaults == settings.disabledDefaults && loaded.excludedPaths == settings.excludedPaths, "list roundtrip");
        require(loaded.customPrograms.size() == 1 && loaded.customPrograms[0].name == settings.customPrograms[0].name &&
                loaded.customPrograms[0].path == settings.customPrograms[0].path, "Unicode/special path roundtrip");
        settings.intervalSeconds = 0;
        require(!mpg::saveSettings(settings), "invalid interval rejected");
        require(mpg::loadSettings().intervalSeconds == 7, "failed write preserves previous settings");
        require(mpg::portableFirstLaunch(), "fresh portable location");
        mpg::markPortableLaunched();
        require(!mpg::portableFirstLaunch(), "portable marker persisted");
        DeleteFileW(mpg::settingsPath().c_str());
        DeleteFileW((path + L"\\portable-seen.ini").c_str());
        RemoveDirectoryW(path.c_str());
        std::cout << "Settings roundtrip, validation, atomic preservation, and portable marker passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
