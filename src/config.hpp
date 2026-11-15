#pragma once
#include "model.hpp"
namespace mpg {
Settings loadSettings();
bool saveSettings(const Settings&, std::wstring* error = nullptr);
bool portableMode();
bool portableFirstLaunch();
void markPortableLaunched();
std::wstring settingsPath();
}
