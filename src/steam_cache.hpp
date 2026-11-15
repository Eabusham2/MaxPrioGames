#pragma once
#include <istream>
#include <set>
#include <string>

namespace mpg {
// Read-only, bounded v40/v41 appinfo.vdf classification. Only requested installed
// app IDs with an intact binary record and common/type=Game are returned.
std::set<unsigned> parseSteamGameIds(std::istream& cache, const std::set<unsigned>& installedIds);
std::set<unsigned> readSteamGameIds(const std::wstring& path, const std::set<unsigned>& installedIds);
}
