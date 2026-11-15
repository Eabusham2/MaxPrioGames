#pragma once
#include "policy.hpp"
#include <string_view>
namespace mpg {
struct SteamManifest {
    unsigned appId = 0;
    std::wstring name;
    std::wstring installDir;
};
// Bounded parsers are platform independent; malformed input returns no matches.
std::vector<std::wstring> parseSteamLibraries(std::string_view text);
SteamManifest parseSteamManifest(std::string_view text);
InstalledRoot parseEpicManifest(std::string_view text);
std::vector<InstalledRoot> discoverInstalledGames();
}
