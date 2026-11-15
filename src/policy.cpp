#include "policy.hpp"
#include <algorithm>
#include <cwctype>

namespace mpg {
namespace {
std::wstring basename(std::wstring_view path) {
    const auto normalized = normalizePath(path);
    const auto slash = normalized.find_last_of(L'\\');
    return normalized.substr(slash == std::wstring::npos ? 0 : slash + 1);
}
bool disabled(const Settings& settings, const std::wstring& id) {
    return std::find(settings.disabledDefaults.begin(), settings.disabledDefaults.end(), id) != settings.disabledDefaults.end();
}
bool aliases(const CatalogEntry& entry, std::wstring_view path) {
    const auto exe = basename(path);
    bool matched = false;
    for (const auto& alias : entry.executables) if (exe == basename(alias)) { matched = true; break; }
    if (!matched) return false;
    if (entry.pathHints.empty()) return true;
    const auto normalized = normalizePath(path);
    for (const auto& hint : entry.pathHints)
        if (!hint.empty() && normalized.find(normalizePath(hint)) != std::wstring::npos) return true;
    return false;
}
const CatalogEntry* entryForRoot(const InstalledRoot& root) {
    for (const auto& entry : catalog()) {
        if (!root.catalogId.empty() && entry.id == root.catalogId) return &entry;
        if (root.steamAppId && std::find(entry.steamAppIds.begin(), entry.steamAppIds.end(), root.steamAppId) != entry.steamAppIds.end())
            return &entry;
    }
    return nullptr;
}
}

std::wstring normalizePath(std::wstring_view path) {
    std::wstring result(path);
    for (auto& c : result) {
        if (c == L'/') c = L'\\';
        c = static_cast<wchar_t>(std::towlower(c));
    }
    if (result.rfind(L"\\\\?\\unc\\", 0) == 0) result = L"\\\\" + result.substr(8);
    else if (result.rfind(L"\\\\?\\", 0) == 0) result.erase(0, 4);
    while (result.size() > 3 && result.back() == L'\\') result.pop_back();
    return result;
}
bool samePath(std::wstring_view a, std::wstring_view b) { return normalizePath(a) == normalizePath(b); }
bool pathWithin(std::wstring_view path, std::wstring_view root) {
    const auto p = normalizePath(path), r = normalizePath(root);
    if (r.empty() || p.size() <= r.size() || p.compare(0, r.size(), r) != 0) return false;
    if (r.back() != L'\\' && p[r.size()] != L'\\') return false;
    // Store manifests do not get to escape their declared root using dot segments.
    return p.find(L"\\..\\") == std::wstring::npos && p.find(L"\\.\\") == std::wstring::npos;
}
bool excludedExecutable(std::wstring_view path) {
    const auto p = normalizePath(path);
    const auto exe = basename(p);
    // Exact common OS binaries are never automatic game-service candidates.
    static const wchar_t* const blocked[] = {
        L"svchost.exe", L"services.exe", L"lsass.exe", L"csrss.exe", L"smss.exe",
        L"wininit.exe", L"winlogon.exe", L"dwm.exe", L"explorer.exe", L"system",
        L"setup.exe", L"install.exe", L"installer.exe", L"uninstall.exe", L"unins000.exe",
        L"dxsetup.exe", L"msiexec.exe", L"crashreporter.exe", L"crashpad_handler.exe",
        L"unitycrashhandler64.exe", L"unitycrashhandler32.exe", L"crashreportclient.exe",
        L"crashreportclient-win64-shipping.exe", L"steamwebhelper.exe", L"cefsharp.browsersubprocess.exe",
        L"epicwebhelper.exe", L"chrome.exe", L"msedge.exe", L"firefox.exe", L"werfault.exe",
        L"powershell.exe", L"pwsh.exe", L"cmd.exe", L"conhost.exe", L"rundll32.exe", L"dllhost.exe",
        L"vc_redist.x64.exe", L"vc_redist.x86.exe", L"vcredist_x64.exe", L"vcredist_x86.exe",
        L"obs64.exe", L"obs32.exe", L"wallpaper32.exe", L"wallpaper64.exe", L"soundpad.exe",
        L"crosshairx.exe", L"steam.exe", L"epicgameslauncher.exe", L"goggalaxy.exe"
    };
    for (const auto* value : blocked) if (exe == value) return true;
    for (const auto* component : {L"\\_commonredist\\", L"\\redist\\", L"\\redistributables\\",
                                  L"\\directx\\", L"\\.egstore\\", L"\\installer\\", L"\\installers\\"})
        if (p.find(component) != std::wstring::npos) return true;
    return exe.rfind(L"unins", 0) == 0 || exe.find(L"crashreport") != std::wstring::npos ||
           exe.find(L"crashhandler") != std::wstring::npos || exe.find(L"prereq") != std::wstring::npos ||
           exe.find(L"redist") != std::wstring::npos;
}
std::wstring defaultCatalogId(std::wstring_view path) {
    if (excludedExecutable(path)) return {};
    for (const auto& entry : catalog()) if (aliases(entry, path)) return entry.id;
    return {};
}
Match matchProgram(std::wstring_view path, const Settings& settings, const std::vector<InstalledRoot>& roots) {
    if (path.empty()) return {};
    for (const auto& excluded : settings.excludedPaths) if (samePath(path, excluded)) return {};
    const auto exe = basename(path);
    for (const auto* core : {L"svchost.exe", L"services.exe", L"lsass.exe", L"csrss.exe", L"smss.exe", L"wininit.exe", L"winlogon.exe", L"dwm.exe", L"system"})
        if (exe == core) return {};
    // User-selected executable paths are explicit intent, not automatic detection.
    for (const auto& custom : settings.customPrograms)
        if (samePath(path, custom.path)) return {true, custom.name, {}, false};
    if (excludedExecutable(path)) return {};
    for (const auto& entry : catalog()) {
        if (!aliases(entry, path)) continue;
        if (disabled(settings, entry.id) || (entry.service && !settings.includeGameServices)) return {};
    }
    for (const auto& root : roots) {
        if (!pathWithin(path, root.path)) continue;
        const auto* entry = entryForRoot(root);
        if (entry && (disabled(settings, entry->id) || (entry->service && !settings.includeGameServices))) return {};
    }
    for (const auto& entry : catalog())
        if (aliases(entry, path)) return {true, entry.name, entry.id, entry.service};
    for (const auto& root : roots) {
        if (!pathWithin(path, root.path)) continue;
        if (const auto* entry = entryForRoot(root)) return {true, entry->name, entry->id, entry->service};
        if (settings.detectInstalledGames && root.verifiedGame) return {true, root.name, {}, false};
    }
    return {};
}
bool validSettings(const Settings& settings) {
    if (settings.intervalSeconds < 1 || settings.intervalSeconds > 60 ||
        settings.disabledDefaults.size() > 2048 || settings.customPrograms.size() > 1024 ||
        settings.excludedPaths.size() > 4096) return false;
    for (const auto& value : settings.disabledDefaults) if (value.empty() || value.size() > 256) return false;
    for (const auto& value : settings.customPrograms)
        if (value.name.empty() || value.name.size() > 512 || value.path.empty() || value.path.size() > 32767) return false;
    for (const auto& value : settings.excludedPaths) if (value.empty() || value.size() > 32767) return false;
    return true;
}
}
