#include "policy.hpp"
#include "discovery.hpp"
#include <cstdlib>
#include <iostream>

namespace {
unsigned checks = 0;
void expect(bool ok, const char* message) { ++checks; if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
}
int main() {
    using namespace mpg;
    expect(samePath(L"C:/Games/Test.EXE", L"c:\\games\\test.exe"), "case and separators");
    expect(samePath(L"\\\\?\\C:\\Games\\Test.exe", L"c:\\games\\test.exe"), "extended path prefix");
    expect(pathWithin(L"C:\\Games\\One\\bin\\game.exe", L"C:\\Games\\One"), "nested root");
    expect(!pathWithin(L"C:\\Games\\OneTwo\\game.exe", L"C:\\Games\\One"), "root boundary");
    expect(!pathWithin(L"C:\\Games\\One\\..\\outside.exe", L"C:\\Games\\One"), "root traversal");
    Settings settings;
    expect(!matchProgram(L"C:\\Windows\\System32\\notepad.exe", settings).matched, "unrelated program untouched");
    settings.customPrograms.push_back({L"My game", L"C:\\Private Games\\Mine.exe"});
    expect(matchProgram(L"c:\\private games\\MINE.exe", settings).matched, "exact custom path");
    expect(!matchProgram(L"D:\\Elsewhere\\Mine.exe", settings).matched, "custom basename does not match");
    settings.excludedPaths.push_back(L"C:\\Private Games\\Mine.exe");
    expect(!matchProgram(L"c:\\private games\\mine.exe", settings).matched, "exclusion overrides custom");
    settings.customPrograms.push_back({L"Not a game", L"C:\\Windows\\System32\\svchost.exe"});
    expect(!matchProgram(L"C:\\Windows\\System32\\svchost.exe", settings).matched, "shared OS service host excluded");

    const CatalogEntry* game = nullptr;
    const CatalogEntry* service = nullptr;
    for (const auto& entry : catalog()) {
        if (!game && !entry.service && !entry.steamAppIds.empty()) game = &entry;
        if (!service && entry.service && !entry.executables.empty()) service = &entry;
    }
    expect(game != nullptr && service != nullptr, "game and exact service catalog present");
    InstalledRoot installed{L"C:\\SteamLibrary\\steamapps\\common\\Verified Game", game->name, game->steamAppIds.front(), game->id, true};
    const std::wstring running = installed.path + L"\\Binaries\\Win64\\Runtime-Win64-Shipping.exe";
    const auto found = matchProgram(running, settings, {installed});
    expect(found.matched && found.catalogId == game->id, "Steam AppID root identifies unknown executable alias");
    settings.disabledDefaults.push_back(game->id);
    expect(!matchProgram(running, settings, {installed}).matched, "disabled default blocks root autodetection");
    settings.customPrograms.push_back({L"Explicit re-add", running});
    expect(matchProgram(running, settings, {installed}).matched, "explicit custom overrides disabled default");
    settings.excludedPaths.push_back(running);
    expect(!matchProgram(running, settings, {installed}).matched, "exclusion overrides every recognition source");
    expect(!matchProgram(installed.path + L"\\_CommonRedist\\DirectX\\DXSETUP.exe", Settings{}, {installed}).matched, "redist within game excluded");
    expect(!matchProgram(installed.path + L"\\CrashReportClient.exe", Settings{}, {installed}).matched, "crash helper excluded");
    expect(!matchProgram(installed.path + L"\\obs64.exe", Settings{}, {installed}).matched, "utility in game root excluded");
    const std::wstring servicePath = L"C:\\Program Files\\" + (service->pathHints.empty() ? std::wstring(L"GameService") : service->pathHints.front()) + L"\\" + service->executables.front();
    Settings noServices;
    noServices.includeGameServices = false;
    expect(!matchProgram(servicePath, noServices).matched, "service toggle respected");
    InstalledRoot unknown{L"D:\\Library\\Another", L"Another game", 0, {}, false};
    expect(!matchProgram(unknown.path + L"\\Another.exe", Settings{}, {unknown}).matched, "unclassified installed app not assumed game");
    unknown.verifiedGame = true;
    expect(matchProgram(unknown.path + L"\\Another.exe", Settings{}, {unknown}).matched, "metadata-classified additional game");
    Settings noDetection; noDetection.detectInstalledGames = false;
    expect(!matchProgram(unknown.path + L"\\Another.exe", noDetection, {unknown}).matched, "extra detection toggle respected");
    expect(matchProgram(running, noDetection, {installed}).matched, "catalog games remain recognized without extra detection");

    const auto libraries = parseSteamLibraries(R"vdf("libraryfolders" { "0" { "path" "C:\\Steam" "apps" { "1" "2" } } "1" { "path" "D:\\Steam Library" } })vdf");
    expect(libraries.size() == 2 && libraries[1] == L"D:\\Steam Library", "modern Steam libraries");
    expect(parseSteamLibraries(R"vdf("LibraryFolders" { "1" "D:\\Games" })vdf").size() == 1, "legacy Steam libraries");
    expect(parseSteamLibraries("\"libraryfolders\" { \"0\" {").empty(), "truncated VDF");
    const auto manifest = parseSteamManifest(R"vdf("AppState" { "appid" "730" "name" "Counter-Strike 2" "installdir" "Counter-Strike Global Offensive" })vdf");
    expect(manifest.appId == 730 && !manifest.installDir.empty(), "Steam installed manifest");
    expect(parseSteamManifest(R"vdf("AppState" { "appid" "730" "installdir" "..\\outside" })vdf").appId == 0, "manifest traversal rejected");
    expect(parseSteamManifest(R"vdf("AppState" { "appid" "99999999999999" "installdir" "A" })vdf").appId == 0, "overflow AppID rejected");
    const auto epic = parseEpicManifest(R"json({"InstallLocation":"D:\\Epic\\UnknownTitle","DisplayName":"Unknown title","AppCategories":["games"],"LaunchExecutable":"game-main.exe"})json");
    expect(epic.verifiedGame && !epic.path.empty(), "Epic explicit game category");
    expect(parseEpicManifest(R"json({"InstallLocation":"D:\\Epic\\Tool","DisplayName":"Unrelated software","AppCategories":["software"]})json").path.empty(), "Epic software not a game");
    expect(parseEpicManifest(R"json({"InstallLocation":"D:\\Epic\\Tool","DisplayName":"Unrelated software"})json").path.empty(), "ambiguous Epic app skipped");
    expect(parseEpicManifest("{\"InstallLocation\":").path.empty(), "truncated JSON");
    expect(validSettings(Settings{}), "default settings valid");
    Settings invalid; invalid.intervalSeconds = 0;
    expect(!validSettings(invalid), "zero interval rejected");
    std::cout << checks << " backend policy/discovery checks passed\n";
}
