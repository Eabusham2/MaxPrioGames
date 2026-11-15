#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "config.hpp"
#include "policy.hpp"
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <cwchar>
#include <sstream>

namespace mpg {
namespace {
std::wstring executable() {
    std::vector<wchar_t> buffer(32768);
    const auto size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return size && size < buffer.size() ? std::wstring(buffer.data(), size) : std::wstring{};
}
std::wstring directory() {
    std::vector<wchar_t> buffer(32768);
    const auto size = GetEnvironmentVariableW(L"MAXPRIOGAMES_CONFIG_DIR", buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size && size < buffer.size()) return buffer.data();
    PWSTR local = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &local))) {
        const auto result = std::wstring(local) + L"\\MaxPrioGames";
        CoTaskMemFree(local);
        return result;
    }
    const auto count = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
    return (count && count < buffer.size() ? std::wstring(buffer.data()) : L".\\") + L"MaxPrioGames";
}
std::wstring hash(std::wstring_view text) {
    std::uint64_t value = 14695981039346656037ull;
    for (auto c : text) { value ^= static_cast<unsigned>(c) & 255u; value *= 1099511628211ull;
        value ^= (static_cast<unsigned>(c) >> 8) & 255u; value *= 1099511628211ull; }
    return std::to_wstring(value);
}
std::wstring hex(std::wstring_view text) {
    static constexpr wchar_t digits[] = L"0123456789abcdef";
    std::wstring result; result.reserve(text.size() * 4);
    for (auto c : text) for (int shift = 12; shift >= 0; shift -= 4) result += digits[(c >> shift) & 15];
    return result;
}
std::wstring unhex(std::wstring_view text) {
    if (text.empty() || text.size() % 4 || text.size() > 32767u * 4u) return {};
    std::wstring result; result.reserve(text.size() / 4);
    for (std::size_t i = 0; i < text.size(); i += 4) {
        unsigned value = 0;
        for (unsigned n = 0; n < 4; ++n) {
            const wchar_t c = text[i + n];
            const int digit = c >= L'0' && c <= L'9' ? c - L'0' : c >= L'a' && c <= L'f' ? c - L'a' + 10 : -1;
            if (digit < 0) return {};
            value = value * 16 + static_cast<unsigned>(digit);
        }
        if (!value) return {};
        result.push_back(static_cast<wchar_t>(value));
    }
    return result;
}
unsigned number(const wchar_t* section, const wchar_t* key, unsigned fallback, unsigned maximum, const std::wstring& path) {
    wchar_t text[32]{};
    const auto count = GetPrivateProfileStringW(section, key, L"", text, 32, path.c_str());
    if (!count || count >= 31) return fallback;
    std::uint64_t value = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (text[i] < L'0' || text[i] > L'9') return fallback;
        value = value * 10 + static_cast<unsigned>(text[i] - L'0');
        if (value > maximum) return fallback;
    }
    return static_cast<unsigned>(value);
}
std::wstring string(const wchar_t* section, const std::wstring& key, const std::wstring& path, unsigned maximum) {
    std::vector<wchar_t> text(static_cast<std::size_t>(maximum) * 4 + 2);
    const auto count = GetPrivateProfileStringW(section, key.c_str(), L"", text.data(), static_cast<DWORD>(text.size()), path.c_str());
    return count < text.size() - 1 ? unhex(std::wstring_view(text.data(), count)) : std::wstring{};
}
bool ensureDirectory() {
    const auto result = SHCreateDirectoryExW(nullptr, directory().c_str(), nullptr);
    return result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS;
}
}

std::wstring settingsPath() { return directory() + L"\\settings.ini"; }
Settings loadSettings() {
    Settings result;
    const auto path = settingsPath();
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info) &&
        (info.nFileSizeHigh || info.nFileSizeLow > 16u * 1024u * 1024u)) return result;
    result.intervalSeconds = number(L"General", L"IntervalSeconds", 3, 60, path);
    if (!result.intervalSeconds) result.intervalSeconds = 3;
    result.detectInstalledGames = number(L"General", L"DetectInstalledGames", 1, 1, path) != 0;
    result.includeGameServices = number(L"General", L"IncludeGameServices", 1, 1, path) != 0;
    const auto disabledCount = number(L"Disabled", L"Count", 0, 2048, path);
    for (unsigned i = 0; i < disabledCount; ++i) {
        auto value = string(L"Disabled", L"Id" + std::to_wstring(i), path, 256);
        if (!value.empty()) result.disabledDefaults.push_back(std::move(value));
    }
    const auto customCount = number(L"Programs", L"Count", 0, 1024, path);
    for (unsigned i = 0; i < customCount; ++i) {
        const auto suffix = std::to_wstring(i);
        CustomProgram program{string(L"Programs", L"Name" + suffix, path, 512), string(L"Programs", L"Path" + suffix, path, 32767)};
        if (!program.name.empty() && !program.path.empty()) result.customPrograms.push_back(std::move(program));
    }
    const auto excludedCount = number(L"Excluded", L"Count", 0, 4096, path);
    for (unsigned i = 0; i < excludedCount; ++i) {
        auto value = string(L"Excluded", L"Path" + std::to_wstring(i), path, 32767);
        if (!value.empty()) result.excludedPaths.push_back(std::move(value));
    }
    return result;
}
bool saveSettings(const Settings& settings, std::wstring* error) {
    if (error) error->clear();
    if (!validSettings(settings) || !ensureDirectory()) { if (error) *error = L"Settings could not be saved."; return false; }
    std::wostringstream out;
    out << L"[General]\r\nIntervalSeconds=" << settings.intervalSeconds << L"\r\nDetectInstalledGames=" << (settings.detectInstalledGames ? 1 : 0)
        << L"\r\nIncludeGameServices=" << (settings.includeGameServices ? 1 : 0) << L"\r\n\r\n[Disabled]\r\nCount=" << settings.disabledDefaults.size() << L"\r\n";
    for (std::size_t i = 0; i < settings.disabledDefaults.size(); ++i) out << L"Id" << i << L"=" << hex(settings.disabledDefaults[i]) << L"\r\n";
    out << L"\r\n[Programs]\r\nCount=" << settings.customPrograms.size() << L"\r\n";
    for (std::size_t i = 0; i < settings.customPrograms.size(); ++i) {
        out << L"Name" << i << L"=" << hex(settings.customPrograms[i].name) << L"\r\nPath" << i << L"=" << hex(settings.customPrograms[i].path) << L"\r\n";
    }
    out << L"\r\n[Excluded]\r\nCount=" << settings.excludedPaths.size() << L"\r\n";
    for (std::size_t i = 0; i < settings.excludedPaths.size(); ++i) out << L"Path" << i << L"=" << hex(settings.excludedPaths[i]) << L"\r\n";
    const auto contents = std::wstring(1, 0xfeff) + out.str();
    if (contents.size() * sizeof(wchar_t) > 16u * 1024u * 1024u) { if (error) *error = L"Settings could not be saved."; return false; }
    const auto path = settingsPath();
    const auto temporary = path + L".tmp";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { if (error) *error = L"Settings could not be saved."; return false; }
    DWORD written = 0;
    const DWORD bytes = static_cast<DWORD>(contents.size() * sizeof(wchar_t));
    bool ok = WriteFile(file, contents.data(), bytes, &written, nullptr) && written == bytes;
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (ok) ok = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) { DeleteFileW(temporary.c_str()); if (error) *error = L"Settings could not be saved."; return false; }
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, path.c_str());
    return true;
}
bool portableMode() {
    const auto exe = executable();
    const auto slash = exe.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return true;
    const auto flags = GetFileAttributesW((exe.substr(0, slash + 1) + L"installed.flag").c_str());
    return flags == INVALID_FILE_ATTRIBUTES || (flags & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
bool portableFirstLaunch() {
    if (!portableMode()) return false;
    const auto file = directory() + L"\\portable-seen.ini";
    const auto key = hash(normalizePath(executable()));
    return number(L"Launches", key.c_str(), 0, 1, file) != 1;
}
void markPortableLaunched() {
    if (!portableMode() || !ensureDirectory()) return;
    const auto file = directory() + L"\\portable-seen.ini";
    const auto key = hash(normalizePath(executable()));
    WritePrivateProfileStringW(L"Launches", key.c_str(), L"1", file.c_str());
}
}
