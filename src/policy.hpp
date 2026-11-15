#pragma once
#include "model.hpp"
#include <string_view>

namespace mpg {
struct InstalledRoot {
    std::wstring path;
    std::wstring name;
    unsigned steamAppId = 0;
    std::wstring catalogId;
    bool verifiedGame = false;
};
struct Match {
    bool matched = false;
    std::wstring name;
    std::wstring catalogId;
    bool service = false;
};
std::wstring normalizePath(std::wstring_view path);
bool samePath(std::wstring_view a, std::wstring_view b);
bool pathWithin(std::wstring_view path, std::wstring_view root);
bool excludedExecutable(std::wstring_view path);
std::wstring defaultCatalogId(std::wstring_view path);
Match matchProgram(std::wstring_view path, const Settings&,
                   const std::vector<InstalledRoot>& roots = {});
bool validSettings(const Settings&);
}
