#include "discovery.hpp"
#include "steam_cache.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <set>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#endif

namespace mpg {
namespace {
constexpr std::size_t kMaximumManifest = 2 * 1024 * 1024;
struct Node { std::string key, value; std::vector<Node> children; };

std::wstring wide(std::string_view input) {
    std::wstring output;
    for (std::size_t i = 0; i < input.size();) {
        const auto lead = static_cast<unsigned char>(input[i++]);
        std::uint32_t code = 0;
        unsigned count = 0;
        if (lead < 0x80) code = lead;
        else if ((lead & 0xe0) == 0xc0) { code = lead & 0x1f; count = 1; }
        else if ((lead & 0xf0) == 0xe0) { code = lead & 0x0f; count = 2; }
        else if ((lead & 0xf8) == 0xf0) { code = lead & 0x07; count = 3; }
        else return {};
        if (i + count > input.size()) return {};
        for (unsigned n = 0; n < count; ++n) {
            const auto c = static_cast<unsigned char>(input[i++]);
            if ((c & 0xc0) != 0x80) return {};
            code = (code << 6) | (c & 0x3f);
        }
        if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) ||
            (count == 3 && code < 0x10000) || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) || !code) return {};
        if (sizeof(wchar_t) == 2 && code > 0xffff) {
            code -= 0x10000;
            output.push_back(static_cast<wchar_t>(0xd800 + (code >> 10)));
            output.push_back(static_cast<wchar_t>(0xdc00 + (code & 0x3ff)));
        } else output.push_back(static_cast<wchar_t>(code));
    }
    return output;
}
void utf8(std::string& out, unsigned code) {
    if (code < 0x80) out.push_back(static_cast<char>(code));
    else if (code < 0x800) { out.push_back(static_cast<char>(0xc0 | (code >> 6))); out.push_back(static_cast<char>(0x80 | (code & 63))); }
    else { out.push_back(static_cast<char>(0xe0 | (code >> 12))); out.push_back(static_cast<char>(0x80 | ((code >> 6) & 63))); out.push_back(static_cast<char>(0x80 | (code & 63))); }
}
class Parser {
public:
    explicit Parser(std::string_view input) : text_(input) { if (input.size() > kMaximumManifest) bad_ = true; }
    std::vector<Node> valve() {
        auto result = valveNodes(0, false);
        skip();
        return bad_ || pos_ != text_.size() ? std::vector<Node>{} : result;
    }
    std::vector<Node> json() {
        skip();
        if (!take('{')) return {};
        auto result = jsonObject(0);
        skip();
        return bad_ || pos_ != text_.size() ? std::vector<Node>{} : result;
    }
private:
    std::string_view text_;
    std::size_t pos_ = 0;
    unsigned nodes_ = 0;
    bool bad_ = false;
    void skip() {
        while (pos_ < text_.size()) {
            if (std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
            else if (text_.substr(pos_, 2) == "//") {
                pos_ = text_.find('\n', pos_ + 2);
                if (pos_ == std::string_view::npos) pos_ = text_.size();
            } else break;
        }
    }
    bool take(char c) { skip(); if (pos_ < text_.size() && text_[pos_] == c) { ++pos_; return true; } return false; }
    std::string string(bool json) {
        skip();
        if (pos_ >= text_.size()) { bad_ = true; return {}; }
        std::string result;
        if (text_[pos_] != '"') {
            if (json) { bad_ = true; return {}; }
            const auto first = pos_;
            while (pos_ < text_.size() && !std::isspace(static_cast<unsigned char>(text_[pos_])) &&
                   text_[pos_] != '{' && text_[pos_] != '}') ++pos_;
            if (pos_ == first || pos_ - first > 65536) { bad_ = true; return {}; }
            return std::string(text_.substr(first, pos_ - first));
        }
        ++pos_;
        while (pos_ < text_.size() && result.size() <= 65536) {
            char c = text_[pos_++];
            if (c == '"') return result;
            if (c == '\\') {
                if (pos_ >= text_.size()) break;
                c = text_[pos_++];
                if (c == '\\' || c == '"' || (json && c == '/')) result.push_back(c);
                else if (c == 'n') result.push_back('\n');
                else if (c == 't') result.push_back('\t');
                else if (c == 'r') result.push_back('\r');
                else if (json && c == 'u') {
                    unsigned code = 0;
                    if (pos_ + 4 > text_.size()) break;
                    for (unsigned n = 0; n < 4; ++n) {
                        const char digit = text_[pos_++];
                        const int v = digit >= '0' && digit <= '9' ? digit - '0' :
                            digit >= 'a' && digit <= 'f' ? digit - 'a' + 10 : digit >= 'A' && digit <= 'F' ? digit - 'A' + 10 : -1;
                        if (v < 0) { bad_ = true; return {}; }
                        code = code * 16 + static_cast<unsigned>(v);
                    }
                    if (!code || (code >= 0xd800 && code <= 0xdfff)) { bad_ = true; return {}; }
                    utf8(result, code);
                } else if (!json) { result.push_back('\\'); result.push_back(c); }
                else { bad_ = true; return {}; }
            } else if (static_cast<unsigned char>(c) < 0x20 && json) { bad_ = true; return {}; }
            else result.push_back(c);
        }
        bad_ = true;
        return {};
    }
    std::vector<Node> valveNodes(unsigned depth, bool closing) {
        std::vector<Node> result;
        if (depth > 16 || bad_) { bad_ = true; return {}; }
        for (;;) {
            skip();
            if (pos_ == text_.size()) { if (closing) bad_ = true; break; }
            if (text_[pos_] == '}') { ++pos_; if (!closing) bad_ = true; break; }
            if (++nodes_ > 8192) { bad_ = true; break; }
            Node node; node.key = string(false);
            if (bad_) break;
            if (take('{')) node.children = valveNodes(depth + 1, true);
            else node.value = string(false);
            if (bad_) break;
            result.push_back(std::move(node));
        }
        return result;
    }
    std::vector<Node> jsonObject(unsigned depth) {
        std::vector<Node> result;
        if (depth > 16 || bad_) { bad_ = true; return {}; }
        if (take('}')) return result;
        for (;;) {
            if (++nodes_ > 8192) { bad_ = true; return {}; }
            Node node; node.key = string(true);
            if (!take(':')) { bad_ = true; return {}; }
            jsonValue(node, depth + 1);
            if (bad_) return {};
            result.push_back(std::move(node));
            if (take('}')) return result;
            if (!take(',')) { bad_ = true; return {}; }
        }
    }
    void jsonValue(Node& node, unsigned depth) {
        if (depth > 16) { bad_ = true; return; }
        skip();
        if (pos_ >= text_.size()) { bad_ = true; return; }
        if (text_[pos_] == '"') node.value = string(true);
        else if (take('{')) node.children = jsonObject(depth);
        else if (take('[')) {
            if (take(']')) return;
            for (;;) {
                if (++nodes_ > 8192) { bad_ = true; return; }
                Node child; jsonValue(child, depth + 1);
                if (bad_) return;
                node.children.push_back(std::move(child));
                if (take(']')) return;
                if (!take(',')) { bad_ = true; return; }
            }
        } else {
            const auto first = pos_;
            while (pos_ < text_.size() && !std::isspace(static_cast<unsigned char>(text_[pos_])) &&
                   text_[pos_] != ',' && text_[pos_] != '}' && text_[pos_] != ']') ++pos_;
            if (pos_ == first) { bad_ = true; return; }
            node.value = std::string(text_.substr(first, pos_ - first));
            if (node.value != "true" && node.value != "false" && node.value != "null") {
                std::size_t i = 0;
                if (i < node.value.size() && node.value[i] == '-') ++i;
                if (i >= node.value.size() || !std::isdigit(static_cast<unsigned char>(node.value[i]))) { bad_ = true; return; }
                if (node.value[i] == '0') ++i;
                else while (i < node.value.size() && std::isdigit(static_cast<unsigned char>(node.value[i]))) ++i;
                if (i < node.value.size() && node.value[i] == '.') {
                    const auto start = ++i;
                    while (i < node.value.size() && std::isdigit(static_cast<unsigned char>(node.value[i]))) ++i;
                    if (start == i) { bad_ = true; return; }
                }
                if (i < node.value.size() && (node.value[i] == 'e' || node.value[i] == 'E')) {
                    ++i;
                    if (i < node.value.size() && (node.value[i] == '+' || node.value[i] == '-')) ++i;
                    const auto start = i;
                    while (i < node.value.size() && std::isdigit(static_cast<unsigned char>(node.value[i]))) ++i;
                    if (start == i) { bad_ = true; return; }
                }
                if (i != node.value.size()) bad_ = true;
            }
        }
    }
};
const Node* field(const std::vector<Node>& nodes, const char* key) {
    for (const auto& node : nodes) if (node.key == key) return &node;
    return nullptr;
}
std::string value(const std::vector<Node>& nodes, const char* key) {
    const auto* found = field(nodes, key); return found ? found->value : std::string{};
}
unsigned unsignedNumber(const std::string& text) {
    if (text.empty() || text.size() > 10) return 0;
    std::uint64_t result = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return 0;
        result = result * 10 + static_cast<unsigned>(c - '0');
        if (result > (std::numeric_limits<unsigned>::max)()) return 0;
    }
    return static_cast<unsigned>(result);
}
bool absolute(const std::wstring& path) {
    return (path.size() > 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) || path.rfind(L"\\\\", 0) == 0;
}
#ifdef _WIN32
const CatalogEntry* steamEntry(unsigned id) {
    for (const auto& entry : catalog())
        if (!entry.service && std::find(entry.steamAppIds.begin(), entry.steamAppIds.end(), id) != entry.steamAppIds.end()) return &entry;
    return nullptr;
}
#endif
}

std::vector<std::wstring> parseSteamLibraries(std::string_view text) {
    auto nodes = Parser(text).valve();
    const auto* root = field(nodes, "libraryfolders");
    if (!root) root = field(nodes, "LibraryFolders");
    if (!root) return {};
    std::vector<std::wstring> paths;
    for (const auto& entry : root->children) {
        if (entry.key.empty() || !std::all_of(entry.key.begin(), entry.key.end(), [](char c) { return c >= '0' && c <= '9'; })) continue;
        auto path = wide(entry.children.empty() ? entry.value : value(entry.children, "path"));
        if (absolute(path) && path.size() < 32760) paths.push_back(std::move(path));
    }
    return paths;
}
SteamManifest parseSteamManifest(std::string_view text) {
    const auto nodes = Parser(text).valve();
    const auto* root = field(nodes, "AppState");
    if (!root) return {};
    SteamManifest result;
    result.appId = unsignedNumber(value(root->children, "appid"));
    result.name = wide(value(root->children, "name"));
    result.installDir = wide(value(root->children, "installdir"));
    if (!result.appId || result.installDir.empty() || result.installDir == L"." || result.installDir == L".." ||
        result.installDir.find_first_of(L"\\/:") != std::wstring::npos) return {};
    return result;
}
InstalledRoot parseEpicManifest(std::string_view text) {
    const auto nodes = Parser(text).json();
    InstalledRoot root;
    root.path = wide(value(nodes, "InstallLocation"));
    root.name = wide(value(nodes, "DisplayName"));
    const auto normalized = normalizePath(root.path);
    if (!absolute(root.path) || root.name.empty() || normalized.size() <= 3 ||
        normalized.find(L"\\..\\") != std::wstring::npos || normalized.find(L"\\.\\") != std::wstring::npos) return {};
    const auto lastSlash = normalized.find_last_of(L'\\');
    const auto last = normalized.substr(lastSlash == std::wstring::npos ? 0 : lastSlash + 1);
    for (const auto* broad : {L"windows", L"program files", L"program files (x86)", L"users", L"programdata", L"steamapps", L"common"})
        if (last == broad) return {};
    if (const auto* categories = field(nodes, "AppCategories"))
        for (const auto& category : categories->children)
            if (category.value == "games" || category.value == "game") root.verifiedGame = true;
    const auto executable = wide(value(nodes, "LaunchExecutable"));
    if (!executable.empty()) {
        const auto full = root.path + L"\\" + executable;
        if (pathWithin(full, root.path)) root.catalogId = defaultCatalogId(full);
    }
    if (root.catalogId.empty()) {
        for (const auto& entry : catalog()) if (!entry.service && normalizePath(entry.name) == normalizePath(root.name)) {
            root.catalogId = entry.id; break;
        }
    }
    if (!root.verifiedGame && root.catalogId.empty()) return {};
    return root;
}

#ifdef _WIN32
namespace {
std::wstring registry(HKEY hive, const wchar_t* key, const wchar_t* name) {
    std::vector<wchar_t> data(32768);
    DWORD bytes = static_cast<DWORD>(data.size() * sizeof(wchar_t));
    if (RegGetValueW(hive, key, name, RRF_RT_REG_SZ, nullptr, data.data(), &bytes) != ERROR_SUCCESS) return {};
    data.back() = 0; return data.data();
}
std::string readFile(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > kMaximumManifest) { CloseHandle(file); return {}; }
    std::string result(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD actual = 0;
    const bool ok = ReadFile(file, result.data(), static_cast<DWORD>(result.size()), &actual, nullptr) && actual == result.size();
    CloseHandle(file);
    return ok ? result : std::string{};
}
bool directory(const std::wstring& path) {
    // Automatic library discovery is local only; an offline network library must
    // never turn a cheap scan or Quit into a network reconnect wait.
    if (path.size() < 3 || path[1] != L':' || (path[2] != L'\\' && path[2] != L'/')) return false;
    const auto root = path.substr(0, 2) + L"\\";
    const UINT type = GetDriveTypeW(root.c_str());
    if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE) return false;
    const auto attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}
template<class F> void manifests(const std::wstring& path, const wchar_t* pattern, F visitor) {
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW((path + L"\\" + pattern).c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return;
    unsigned count = 0;
    do {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) visitor(path + L"\\" + data.cFileName);
    } while (++count < 4096 && FindNextFileW(find, &data));
    FindClose(find);
}
}
std::vector<InstalledRoot> discoverInstalledGames() {
    std::vector<InstalledRoot> output;
    std::set<std::wstring> steamBases;
    for (const auto& base : {registry(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath"),
                             registry(HKEY_LOCAL_MACHINE,L"Software\\WOW6432Node\\Valve\\Steam",L"InstallPath"),
                             registry(HKEY_LOCAL_MACHINE,L"Software\\Valve\\Steam",L"InstallPath")})
        if (absolute(base) && directory(base)) steamBases.insert(normalizePath(base));
    std::set<std::wstring> libraries = steamBases;
    for (const auto& base : steamBases) for (const auto* relative : {L"\\steamapps\\libraryfolders.vdf", L"\\config\\libraryfolders.vdf"})
        for (const auto& path : parseSteamLibraries(readFile(base + relative)))
            if (directory(path)) libraries.insert(normalizePath(path));
    struct InstalledSteam { SteamManifest manifest; std::wstring steamapps; };
    std::vector<InstalledSteam> installed;
    std::set<unsigned> installedIds;
    for (const auto& library : libraries) {
        const auto steamapps = library + L"\\steamapps";
        manifests(steamapps, L"appmanifest_*.acf", [&](const std::wstring& path) {
            const auto manifest = parseSteamManifest(readFile(path));
            if (!manifest.appId || manifest.appId == 431960 || manifest.appId == 1366800 ||
                manifest.appId == 1905180 || manifest.appId == 629520) return;
            if (!directory(steamapps + L"\\common\\" + manifest.installDir)) return;
            installed.push_back({manifest, steamapps});
            installedIds.insert(manifest.appId);
        });
    }
    std::set<unsigned> verifiedIds;
    for (const auto& base : steamBases) {
        const auto verified = readSteamGameIds(base + L"\\appcache\\appinfo.vdf", installedIds);
        verifiedIds.insert(verified.begin(), verified.end());
    }
    for (const auto& app : installed) {
        const auto* entry = steamEntry(app.manifest.appId);
        if (!entry && !verifiedIds.count(app.manifest.appId)) continue;
        InstalledRoot root{app.steamapps + L"\\common\\" + app.manifest.installDir,
            app.manifest.name.empty() && entry ? entry->name : app.manifest.name,
            app.manifest.appId, entry ? entry->id : std::wstring{}, true};
        output.push_back(std::move(root));
    }
    PWSTR programData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT, nullptr, &programData))) {
        const auto path = std::wstring(programData) + L"\\Epic\\EpicGamesLauncher\\Data\\Manifests";
        CoTaskMemFree(programData);
        if (directory(path))
            manifests(path, L"*.item", [&](const std::wstring& file) {
                auto root = parseEpicManifest(readFile(file));
                if (!root.path.empty() && directory(root.path)) output.push_back(std::move(root));
            });
    }
    std::set<std::wstring> seen;
    output.erase(std::remove_if(output.begin(), output.end(), [&](const InstalledRoot& root) {
        return !seen.insert(normalizePath(root.path)).second;
    }), output.end());
    return output;
}
#else
std::vector<InstalledRoot> discoverInstalledGames() { return {}; }
#endif
}
