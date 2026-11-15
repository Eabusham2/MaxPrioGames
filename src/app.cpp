#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <psapi.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "model.hpp"
#include "policy.hpp"
#include "engine.hpp"
#include "config.hpp"
#include "startup.hpp"

namespace {
constexpr wchar_t kMainClass[] = L"MaxPrioGames.Main";
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kSnapshotMessage = WM_APP + 2;
constexpr UINT kSmokeMessage = WM_APP + 3;
constexpr UINT kToggleMessage = WM_APP + 4;
constexpr UINT_PTR kBenchmarkTimer = 1;
constexpr UINT_PTR kTrayRetryTimer = 2;
constexpr int kSearch = 101, kList = 102, kAdd = 103, kRemove = 104;
constexpr int kDetect = 105, kServices = 106, kInterval = 107, kApply = 108;
constexpr int kToTray = 109, kQuit = 110, kStartup = 111;
constexpr int kMenuOpen = 1001, kMenuQuit = 1002, kMenuWindowBase = 2000;
constexpr COLORREF kInk = RGB(24, 34, 45);
constexpr COLORREF kMuted = RGB(91, 104, 119);

std::uint64_t FileTimeValue(const FILETIME& value) {
    ULARGE_INTEGER result{};
    result.LowPart = value.dwLowDateTime;
    result.HighPart = value.dwHighDateTime;
    return result.QuadPart;
}

bool WriteReport(const std::wstring& path, const std::string& content) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool good = WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr)
        && written == content.size();
    CloseHandle(file);
    return good;
}

std::wstring Basename(std::wstring_view path) {
    const auto split = path.find_last_of(L"\\/");
    return std::wstring(split == path.npos ? path : path.substr(split + 1));
}

std::wstring FriendlyName(std::wstring_view path) {
    auto name = Basename(path);
    const auto dot = name.find_last_of(L'.');
    if (dot != name.npos) name.resize(dot);
    return name;
}

bool EqualText(std::wstring_view left, std::wstring_view right) {
    return CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(),
        static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

bool Contains(std::wstring value, std::wstring query) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) { return std::towlower(ch); });
    std::transform(query.begin(), query.end(), query.begin(), [](wchar_t ch) { return std::towlower(ch); });
    return value.find(query) != value.npos;
}

std::wstring CanonicalPath(const std::wstring& path) {
    std::wstring canonical;
    HANDLE file = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        const DWORD length = GetFinalPathNameByHandleW(file, nullptr, 0, FILE_NAME_NORMALIZED);
        if (length && length < 32768) {
            std::vector<wchar_t> buffer(length + 1);
            const DWORD written = GetFinalPathNameByHandleW(file, buffer.data(), static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED);
            if (written && written < buffer.size()) canonical.assign(buffer.data(), written);
        }
        CloseHandle(file);
    }
    if (canonical.empty()) {
        const DWORD length = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
        if (length && length < 32768) {
            std::vector<wchar_t> buffer(length + 1);
            const DWORD written = GetFullPathNameW(path.c_str(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
            if (written && written < buffer.size()) canonical.assign(buffer.data(), written);
        }
    }
    if (canonical.rfind(L"\\\\?\\UNC\\", 0) == 0) canonical = L"\\\\" + canonical.substr(8);
    else if (canonical.rfind(L"\\\\?\\", 0) == 0) canonical.erase(0, 4);
    return canonical;
}

std::wstring ProcessPath(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};
    std::vector<wchar_t> text(32768);
    DWORD size = static_cast<DWORD>(text.size());
    const bool good = QueryFullProcessImageNameW(process, 0, text.data(), &size) != FALSE;
    CloseHandle(process);
    return good ? std::wstring(text.data(), size) : std::wstring{};
}

HICON MakeCounterIcon(unsigned count) {
    constexpr int size = 32;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* raw = nullptr;
    HBITMAP color = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &raw, nullptr, 0);
    if (!color || !raw) return nullptr;
    auto* pixels = static_cast<std::uint32_t*>(raw);
    const std::uint32_t fill = count ? 0xff166534u : 0xff586474u;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const double dx = x - 15.5, dy = y - 15.5;
            pixels[y * size + x] = dx * dx + dy * dy < 240 ? fill : 0;
        }
    HDC dc = CreateCompatibleDC(nullptr);
    HGDIOBJ previousBitmap = SelectObject(dc, color);
    std::wstring text = std::to_wstring(count);
    const int height = text.size() == 1 ? 26 : text.size() == 2 ? 23 : text.size() == 3 ? 18 : 13;
    HFONT font = CreateFontW(-height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ previousFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    RECT rect{0, -1, 32, 31};
    DrawTextW(dc, text.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, previousFont);
    SelectObject(dc, previousBitmap);
    DeleteObject(font);
    DeleteDC(dc);
    // GDI text does not preserve the alpha channel; restore it inside the disk.
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const double dx = x - 15.5, dy = y - 15.5;
            if (dx * dx + dy * dy < 240) pixels[y * size + x] |= 0xff000000u;
            else pixels[y * size + x] = 0;
        }
    std::array<unsigned char, 128> bits{};
    HBITMAP mask = CreateBitmap(32, 32, 1, 1, bits.data());
    ICONINFO icon{};
    icon.fIcon = TRUE;
    icon.hbmColor = color;
    icon.hbmMask = mask;
    HICON result = CreateIconIndirect(&icon);
    DeleteObject(color);
    if (mask) DeleteObject(mask);
    return result;
}

struct WindowChoice { std::wstring title; std::wstring path; };

std::vector<WindowChoice> OpenWindows() {
    std::vector<WindowChoice> windows;
    EnumWindows([](HWND window, LPARAM parameter) -> BOOL {
        auto& choices = *reinterpret_cast<std::vector<WindowChoice>*>(parameter);
        if (!IsWindowVisible(window) || GetWindowTextLengthW(window) == 0 || window == GetShellWindow()) return TRUE;
        const LONG_PTR style = GetWindowLongPtrW(window, GWL_EXSTYLE);
        if (style & WS_EX_TOOLWINDOW) return TRUE;
        DWORD pid = 0;
        GetWindowThreadProcessId(window, &pid);
        if (!pid || pid == GetCurrentProcessId()) return TRUE;
        auto path = ProcessPath(pid);
        if (path.empty()) return TRUE;
        if (EqualText(Basename(path), L"ApplicationFrameHost.exe")) {
            struct ChildChoice { DWORD ownerPid; std::wstring path; } child{pid, {}};
            EnumChildWindows(window, [](HWND candidate, LPARAM value) -> BOOL {
                auto& choice = *reinterpret_cast<ChildChoice*>(value);
                DWORD childPid = 0;
                GetWindowThreadProcessId(candidate, &childPid);
                if (childPid && childPid != choice.ownerPid && IsWindowVisible(candidate)) {
                    choice.path = ProcessPath(childPid);
                    if (!choice.path.empty()) return FALSE;
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&child));
            if (!child.path.empty()) path = std::move(child.path);
        }
        std::vector<wchar_t> title(static_cast<std::size_t>(GetWindowTextLengthW(window)) + 1);
        if (GetWindowTextW(window, title.data(), static_cast<int>(title.size())) > 0)
            choices.push_back({title.data(), std::move(path)});
        return TRUE;
    }, reinterpret_cast<LPARAM>(&windows));
    std::sort(windows.begin(), windows.end(), [](const WindowChoice& left, const WindowChoice& right) {
        return CompareStringOrdinal(left.title.c_str(), -1, right.title.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    });
    return windows;
}

struct Row {
    std::wstring key;
    std::wstring name;
    std::wstring id;
    std::wstring path;
    bool custom = false;
    bool service = false;
    bool detected = false;
};

struct App;
LRESULT CALLBACK MainProc(HWND, UINT, WPARAM, LPARAM);

struct App {
    HINSTANCE instance = nullptr;
    HWND main = nullptr;
    HWND list = nullptr;
    HWND search = nullptr;
    HWND startupButton = nullptr;
    UINT dpi = 96;
    UINT taskbarCreated = 0, quitMessage = 0, showMessage = 0;
    HICON trayIcon = nullptr;
    HICON mainIcon = nullptr;
    HFONT font = nullptr, titleFont = nullptr;
    HBRUSH background = nullptr;
    bool trayAdded = false;
    unsigned shownCount = ~0u;
    bool controlsReady = false;
    bool rebuilding = false;
    bool finishing = false;
    bool portable = false;
    bool smoke = false;
    bool benchmark = false;
    unsigned benchmarkSeconds = 50;
    std::wstring reportPath;
    std::wstring status;
    std::uint64_t startTicks = 0, startCpu = 0;
    int exitCode = 0;
    mpg::Settings settings;
    mpg::Snapshot snapshot;
    std::unique_ptr<mpg::Monitor> monitor;
    std::vector<Row> rows;
    std::vector<std::pair<Row, bool>> pendingToggles;

    int px(int value) const { return MulDiv(value, static_cast<int>(dpi), 96); }
    void makeFonts() {
        if (font) DeleteObject(font);
        if (titleFont) DeleteObject(titleFont);
        font = CreateFontW(-MulDiv(10, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        titleFont = CreateFontW(-MulDiv(19, static_cast<int>(dpi), 72), 0, 0, 0, FW_SEMIBOLD,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }
    HWND control(const wchar_t* cls, const wchar_t* text, DWORD style, int id,
                 int x, int y, int width, int height, DWORD extended = 0) {
        HWND child = CreateWindowExW(extended, cls, text, WS_CHILD | WS_VISIBLE | style,
            px(x), px(y), px(width), px(height), main,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return child;
    }
    bool enabled(const Row& row) const {
        if (row.custom)
            return std::none_of(settings.excludedPaths.begin(), settings.excludedPaths.end(),
                [&](const std::wstring& path) { return mpg::samePath(path, row.path); });
        if (row.service && !settings.includeGameServices) return false;
        return std::find(settings.disabledDefaults.begin(), settings.disabledDefaults.end(), row.id) == settings.disabledDefaults.end();
    }
    void ensureControls() {
        if (controlsReady) return;
        controlsReady = true;
        INITCOMMONCONTROLSEX common{sizeof(common), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
        InitCommonControlsEx(&common);
        makeFonts();
        search = control(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, kSearch, 20, 82, 320, 30, WS_EX_CLIENTEDGE);
        SendMessageW(search, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Find a game or program"));
        control(L"BUTTON", L"Add EXE", WS_TABSTOP | BS_PUSHBUTTON, kAdd, 352, 82, 92, 30);
        control(L"BUTTON", L"Remove", WS_TABSTOP | BS_PUSHBUTTON, kRemove, 454, 82, 102, 30);
        list = control(WC_LISTVIEWW, L"", WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
            kList, 20, 124, 536, 270, WS_EX_CLIENTEDGE);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);
        const std::array<const wchar_t*, 3> titles{L"Program", L"Type", L"Status"};
        const std::array<int, 3> widths{280, 85, 142};
        for (int i = 0; i < 3; ++i) {
            LVCOLUMNW column{};
            column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            column.pszText = const_cast<LPWSTR>(titles[i]);
            column.cx = px(widths[i]);
            column.iSubItem = i;
            ListView_InsertColumn(list, i, &column);
        }
        control(L"BUTTON", L"Recognize installed games automatically", WS_TABSTOP | BS_AUTOCHECKBOX,
            kDetect, 20, 406, 330, 24);
        control(L"BUTTON", L"Include game-related services", WS_TABSTOP | BS_AUTOCHECKBOX,
            kServices, 20, 435, 330, 24);
        control(L"STATIC", L"Interval (s)", SS_LEFT, 0, 360, 409, 82, 24);
        HWND interval = control(L"EDIT", std::to_wstring(settings.intervalSeconds).c_str(),
            WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, kInterval, 446, 404, 48, 28, WS_EX_CLIENTEDGE);
        SendMessageW(interval, EM_SETLIMITTEXT, 2, 0);
        control(L"BUTTON", L"Apply", WS_TABSTOP | BS_PUSHBUTTON, kApply, 502, 404, 54, 28);
        if (portable) startupButton = control(L"BUTTON", L"Add to startup", WS_TABSTOP | BS_PUSHBUTTON,
            kStartup, 20, 484, 178, 32);
        control(L"BUTTON", L"To tray", WS_TABSTOP | BS_PUSHBUTTON, kToTray, 360, 484, 88, 32);
        control(L"BUTTON", L"Quit", WS_TABSTOP | BS_PUSHBUTTON, kQuit, 460, 484, 96, 32);
        syncControls();
        fillList();
    }
    void syncControls() {
        if (!controlsReady) return;
        SendDlgItemMessageW(main, kDetect, BM_SETCHECK, settings.detectInstalledGames ? BST_CHECKED : BST_UNCHECKED, 0);
        SendDlgItemMessageW(main, kServices, BM_SETCHECK, settings.includeGameServices ? BST_CHECKED : BST_UNCHECKED, 0);
        SetDlgItemTextW(main, kInterval, std::to_wstring(settings.intervalSeconds).c_str());
    }
    void fillList() {
        if (!controlsReady || rebuilding) return;
        std::wstring selectedKey;
        const int selection = ListView_GetNextItem(list, -1, LVNI_SELECTED);
        if (selection >= 0 && selection < static_cast<int>(rows.size())) selectedKey = rows[selection].key;
        std::wstring topKey;
        const int top = ListView_GetTopIndex(list);
        if (top >= 0 && top < static_cast<int>(rows.size())) topKey = rows[top].key;
        wchar_t queryText[256]{};
        GetWindowTextW(search, queryText, 256);
        const std::wstring query = queryText;
        rows.clear();
        for (const auto& entry : mpg::catalog()) {
            bool matches = query.empty() || Contains(entry.name, query);
            for (const auto& executable : entry.executables) matches = matches || Contains(executable, query);
            if (matches) rows.push_back({L"default:" + entry.id, entry.name, entry.id, {}, false, entry.service});
        }
        for (const auto& program : settings.customPrograms)
            if (query.empty() || Contains(program.name, query) || Contains(program.path, query))
                rows.push_back({L"custom:" + mpg::normalizePath(program.path), program.name, {}, program.path, true, false});
        for (const auto& process : snapshot.processes) {
            if (!process.managed || !process.catalogId.empty() || !mpg::defaultCatalogId(process.path).empty()) continue;
            if (std::any_of(settings.customPrograms.begin(), settings.customPrograms.end(),
                [&](const mpg::CustomProgram& program) { return mpg::samePath(program.path, process.path); })) continue;
            if (std::any_of(settings.excludedPaths.begin(), settings.excludedPaths.end(),
                [&](const std::wstring& path) { return mpg::samePath(path, process.path); })) continue;
            if (std::any_of(rows.begin(), rows.end(),
                [&](const Row& row) { return row.detected && mpg::samePath(row.path, process.path); })) continue;
            const std::wstring name = process.name.empty() ? FriendlyName(process.path) : process.name;
            if (query.empty() || Contains(name, query) || Contains(process.path, query))
                rows.push_back({L"detected:" + mpg::normalizePath(process.path), name, {}, process.path, true, false, true});
        }
        std::map<std::wstring, unsigned> activeByPath;
        std::map<std::wstring, unsigned> activeByDefault;
        for (const auto& process : snapshot.processes) {
            if (!process.managed || !process.realtime) continue;
            ++activeByPath[mpg::normalizePath(process.path)];
            const auto id = process.catalogId.empty() ? mpg::defaultCatalogId(process.path) : process.catalogId;
            if (!id.empty()) ++activeByDefault[id];
        }
        rebuilding = true;
        SendMessageW(list, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(list);
        int scrollTo = 0;
        for (std::size_t index = 0; index < rows.size(); ++index) {
            const Row& row = rows[index];
            LVITEMW item{};
            item.mask = LVIF_TEXT | LVIF_PARAM;
            item.iItem = static_cast<int>(index);
            item.pszText = const_cast<LPWSTR>(row.name.c_str());
            item.lParam = static_cast<LPARAM>(index);
            ListView_InsertItem(list, &item);
            ListView_SetCheckState(list, static_cast<int>(index), enabled(row));
            const wchar_t* type = row.detected ? L"Detected" : row.custom ? L"Added" : row.service ? L"Service" : L"Game";
            ListView_SetItemText(list, static_cast<int>(index), 1, const_cast<LPWSTR>(type));
            const auto& counts = row.custom ? activeByPath : activeByDefault;
            const auto found = counts.find(row.custom ? mpg::normalizePath(row.path) : row.id);
            const unsigned active = found == counts.end() ? 0 : found->second;
            std::wstring state = active ? L"Realtime \u00d7 " + std::to_wstring(active) : enabled(row) ? L"Waiting" : L"Off";
            ListView_SetItemText(list, static_cast<int>(index), 2, state.data());
            if (row.key == selectedKey) ListView_SetItemState(list, static_cast<int>(index), LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            if (row.key == topKey) scrollTo = static_cast<int>(index);
        }
        RECT itemRect{};
        if (scrollTo && ListView_GetItemRect(list, 0, &itemRect, LVIR_BOUNDS))
            ListView_Scroll(list, 0, scrollTo * (itemRect.bottom - itemRect.top));
        SendMessageW(list, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(list, nullptr, FALSE);
        rebuilding = false;
    }
    bool commit(mpg::Settings next, const std::wstring& success = {}) {
        std::wstring error;
        if (!smoke && !mpg::saveSettings(next, &error)) {
            status = error.empty() ? L"Could not save this change." : error;
            syncControls();
            fillList();
            InvalidateRect(main, nullptr, FALSE);
            return false;
        }
        settings = std::move(next);
        if (monitor) monitor->update(settings);
        status = success;
        syncControls();
        fillList();
        InvalidateRect(main, nullptr, FALSE);
        return true;
    }
    void setEnabled(const Row& row, bool enable) {
        auto next = settings;
        if (row.custom) {
            next.excludedPaths.erase(std::remove_if(next.excludedPaths.begin(), next.excludedPaths.end(),
                [&](const std::wstring& path) { return mpg::samePath(path, row.path); }), next.excludedPaths.end());
            if (!enable) next.excludedPaths.push_back(row.path);
        } else {
            next.disabledDefaults.erase(std::remove(next.disabledDefaults.begin(), next.disabledDefaults.end(), row.id), next.disabledDefaults.end());
            if (!enable) next.disabledDefaults.push_back(row.id);
            else if (row.service) next.includeGameServices = true;
        }
        commit(std::move(next));
    }
    void removeSelected() {
        const int selected = ListView_GetNextItem(list, -1, LVNI_SELECTED);
        if (selected < 0 || selected >= static_cast<int>(rows.size())) return;
        const Row row = rows[selected];
        if (!row.custom) { setEnabled(row, false); return; }
        auto next = settings;
        next.customPrograms.erase(std::remove_if(next.customPrograms.begin(), next.customPrograms.end(),
            [&](const mpg::CustomProgram& program) { return mpg::samePath(program.path, row.path); }), next.customPrograms.end());
        if (std::none_of(next.excludedPaths.begin(), next.excludedPaths.end(),
            [&](const std::wstring& path) { return mpg::samePath(path, row.path); })) next.excludedPaths.push_back(row.path);
        commit(std::move(next));
    }
    void addProgram(const std::wstring& input) {
        const std::wstring path = CanonicalPath(input);
        if (path.empty()) { status = L"Select an existing executable."; InvalidateRect(main, nullptr, FALSE); return; }
        if (mpg::samePath(path, mpg::startup::executablePath())) return;
        std::wstring defaultId = mpg::defaultCatalogId(path);
        if (defaultId.empty()) {
            // Steam-ID-only catalog entries are resolved by the engine's verified
            // manifest discovery, rather than guessing executable names in the UI.
            for (const auto& process : snapshot.processes)
                if (!process.catalogId.empty() && mpg::samePath(process.path, path)) {
                    defaultId = process.catalogId;
                    break;
                }
        }
        auto next = settings;
        next.excludedPaths.erase(std::remove_if(next.excludedPaths.begin(), next.excludedPaths.end(),
            [&](const std::wstring& old) { return mpg::samePath(old, path); }), next.excludedPaths.end());
        if (!defaultId.empty()) {
            next.disabledDefaults.erase(std::remove(next.disabledDefaults.begin(), next.disabledDefaults.end(), defaultId), next.disabledDefaults.end());
            for (const auto& entry : mpg::catalog()) if (entry.id == defaultId && entry.service) next.includeGameServices = true;
        } else if (std::none_of(next.customPrograms.begin(), next.customPrograms.end(),
            [&](const mpg::CustomProgram& program) { return mpg::samePath(program.path, path); })) {
            next.customPrograms.push_back({FriendlyName(path), path});
        }
        commit(std::move(next), FriendlyName(path) + L" is included.");
    }
    void chooseExecutable() {
        std::vector<wchar_t> path(32768);
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = main;
        dialog.lpstrFilter = L"Windows programs (*.exe)\0*.exe\0\0";
        dialog.lpstrFile = path.data();
        dialog.nMaxFile = static_cast<DWORD>(path.size());
        dialog.lpstrTitle = L"Add a program";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT;
        if (GetOpenFileNameW(&dialog)) addProgram(path.data());
    }
    void applyInterval() {
        wchar_t text[20]{};
        GetDlgItemTextW(main, kInterval, text, 20);
        wchar_t* end = nullptr;
        const unsigned long interval = wcstoul(text, &end, 10);
        if (!*text || !end || *end || interval < 1 || interval > 60) {
            status = L"Choose a check interval from 1 to 60 seconds.";
            InvalidateRect(main, nullptr, FALSE);
            return;
        }
        auto next = settings;
        next.intervalSeconds = static_cast<unsigned>(interval);
        commit(std::move(next));
    }
    void show() {
        ensureControls();
        if (monitor) snapshot = monitor->snapshot();
        if (startupButton)
            SetWindowTextW(startupButton, mpg::startup::isEnabled() ? L"Remove from startup" : L"Add to startup");
        fillList();
        ShowWindow(main, IsIconic(main) ? SW_RESTORE : SW_SHOWNORMAL);
        SetForegroundWindow(main);
        InvalidateRect(main, nullptr, FALSE);
    }
    void hide() { ShowWindow(main, SW_HIDE); }
    void updateTray(bool add = false) {
        if (finishing) return;
        if (!add && trayAdded && shownCount == snapshot.realtimeCount) return;
        HICON replacement = MakeCounterIcon(snapshot.realtimeCount);
        if (!replacement) return;
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data);
        data.hWnd = main;
        data.uID = 1;
        data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        data.uCallbackMessage = kTrayMessage;
        data.hIcon = replacement;
        const std::wstring tip = L"MaxPrioGames \u00b7 " + std::to_wstring(snapshot.realtimeCount) +
            (snapshot.realtimeCount == 1 ? L" process at Realtime" : L" processes at Realtime");
        wcsncpy_s(data.szTip, tip.c_str(), _TRUNCATE);
        bool success = Shell_NotifyIconW(add || !trayAdded ? NIM_ADD : NIM_MODIFY, &data) != FALSE;
        if (!success && trayAdded) success = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
        if (success) {
            KillTimer(main, kTrayRetryTimer);
            if (trayIcon) DestroyIcon(trayIcon);
            trayIcon = replacement;
            trayAdded = true;
            shownCount = snapshot.realtimeCount;
            data.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &data);
        } else {
            DestroyIcon(replacement);
            trayAdded = false;
            // Explorer may still be starting at logon. Retry only while the icon
            // is absent; normal idle operation has no tray maintenance timer.
            SetTimer(main, kTrayRetryTimer, 3000, nullptr);
        }
    }
    HMENU buildMenu(const std::vector<WindowChoice>& choices) {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, kMenuOpen, L"Open");
        SetMenuDefaultItem(menu, kMenuOpen, FALSE);
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Add an open window");
        if (choices.empty()) AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"No open windows");
        for (std::size_t index = 0; index < choices.size(); ++index) {
            std::wstring title = choices[index].title;
            if (title.size() > 64) title = title.substr(0, 61) + L"\u2026";
            title += L"  (" + Basename(choices[index].path) + L")";
            // Menu ampersands are mnemonics; escape application titles literally.
            std::wstring escaped;
            for (wchar_t ch : title) { escaped += ch; if (ch == L'&') escaped += ch; }
            AppendMenuW(menu, MF_STRING, kMenuWindowBase + index, escaped.c_str());
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuQuit, L"Close");
        return menu;
    }
    void contextMenu() {
        const auto choices = OpenWindows();
        HMENU menu = buildMenu(choices);
        POINT point{};
        GetCursorPos(&point);
        SetForegroundWindow(main);
        const int selected = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
            point.x, point.y, main, nullptr);
        DestroyMenu(menu);
        PostMessageW(main, WM_NULL, 0, 0);
        if (selected == kMenuOpen) show();
        else if (selected == kMenuQuit) DestroyWindow(main);
        else if (selected >= kMenuWindowBase && selected < kMenuWindowBase + static_cast<int>(choices.size()))
            addProgram(choices[selected - kMenuWindowBase].path);
    }
    void draw(HDC dc) {
        RECT rect{};
        GetClientRect(main, &rect);
        FillRect(dc, &rect, background);
        if (!controlsReady) return;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, kInk);
        HGDIOBJ previous = SelectObject(dc, titleFont);
        RECT title{px(20), px(14), px(550), px(46)};
        DrawTextW(dc, L"MaxPrioGames", -1, &title, DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, font);
        SetTextColor(dc, kMuted);
        RECT summary{px(20), px(50), px(556), px(74)};
        const std::wstring message = std::to_wstring(snapshot.realtimeCount) + L" processes at Realtime  \u00b7  Automatic";
        DrawTextW(dc, message.c_str(), -1, &summary, DT_SINGLELINE | DT_NOPREFIX);
        RECT detail{px(20), px(462), px(556), px(482)};
        DrawTextW(dc, status.c_str(), -1, &detail, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(dc, previous);
    }
    void onSnapshot() {
        if (!monitor || finishing) return;
        snapshot = monitor->snapshot();
        updateTray();
        if (IsWindowVisible(main) && !IsIconic(main)) {
            fillList();
            RECT area{px(20), px(50), px(556), px(74)};
            InvalidateRect(main, &area, FALSE);
        }
    }
    void cleanup() {
        if (finishing) return;
        finishing = true;
        if (monitor) monitor->stop();
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data);
        data.hWnd = main;
        data.uID = 1;
        if (trayAdded) Shell_NotifyIconW(NIM_DELETE, &data);
        trayAdded = false;
    }
    ~App() {
        cleanup();
        if (trayIcon) DestroyIcon(trayIcon);
        if (mainIcon) DestroyIcon(mainIcon);
        if (font) DeleteObject(font);
        if (titleFont) DeleteObject(titleFont);
        if (background) DeleteObject(background);
    }
    void reportBenchmark() {
        if (monitor) snapshot = monitor->snapshot();
        PROCESS_MEMORY_COUNTERS_EX memory{};
        memory.cb = sizeof(memory);
        GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory));
        FILETIME created{}, exited{}, kernel{}, user{};
        GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
        const auto cpu = FileTimeValue(kernel) + FileTimeValue(user) - startCpu;
        const auto elapsed = GetTickCount64() - startTicks;
        DWORD handles = 0;
        GetProcessHandleCount(GetCurrentProcess(), &handles);
        const auto managed = std::count_if(snapshot.processes.begin(), snapshot.processes.end(),
            [](const mpg::RunningProcess& process) { return process.managed; });
        std::ostringstream json;
        json << "{\n  \"mode\": \"real-monitor-hidden-tray\",\n"
             << "  \"elapsed_ms\": " << elapsed << ",\n"
             << "  \"cpu_ms\": " << cpu / 10000.0 << ",\n"
             << "  \"cpu_percent_of_one_core\": " << (elapsed ? cpu / 100.0 / elapsed : 0) << ",\n"
             << "  \"working_set_bytes\": " << memory.WorkingSetSize << ",\n"
             << "  \"peak_working_set_bytes\": " << memory.PeakWorkingSetSize << ",\n"
             << "  \"private_bytes\": " << memory.PrivateUsage << ",\n"
             << "  \"handle_count\": " << handles << ",\n"
             << "  \"monitor_scan_count\": " << snapshot.scanCount << ",\n"
             << "  \"realtime_count\": " << snapshot.realtimeCount << ",\n"
             << "  \"tracked_process_count\": " << managed << ",\n"
             << "  \"interval_seconds\": " << settings.intervalSeconds << ",\n"
             << "  \"tray_added\": " << (trayAdded ? "true" : "false") << "\n}\n";
        if (!WriteReport(reportPath, json.str())) exitCode = 2;
        DestroyWindow(main);
    }
    void runSmoke() {
        show();
        const bool windowCreated = IsWindow(main) != FALSE;
        const bool initialZero = shownCount == 0;
        const bool listCreated = IsWindow(list) && ListView_GetItemCount(list) == static_cast<int>(mpg::catalog().size());
        const bool controlsExist = GetDlgItem(main, kAdd) && GetDlgItem(main, kRemove) &&
            GetDlgItem(main, kQuit) && GetDlgItem(main, kToTray) && GetDlgItem(main, kDetect) && GetDlgItem(main, kServices);
        snapshot.realtimeCount = 3;
        updateTray();
        const bool counterChanged = shownCount == 3 && trayIcon;
        const std::vector<WindowChoice> menuFixture{{L"Smoke & Game", L"C:\\MaxPrioGamesFixture\\Smoke.exe"}};
        HMENU menu = buildMenu(menuFixture);
        const bool menuWorks = menu && GetMenuItemCount(menu) == 6 && GetMenuItemID(menu, 0) == kMenuOpen &&
            GetMenuItemID(menu, 3) == kMenuWindowBase && GetMenuItemID(menu, 5) == kMenuQuit;
        if (menu) DestroyMenu(menu);
        bool toggled = false;
        if (!rows.empty()) {
            const Row first = rows.front();
            setEnabled(first, false);
            const bool disabled = !enabled(first);
            setEnabled(first, true);
            toggled = disabled && enabled(first);
        }
        SetWindowTextW(search, L"__no_matching_program_in_catalog__");
        fillList();
        const bool filterWorks = ListView_GetItemCount(list) == 0;
        SetWindowTextW(search, L"");
        fillList();
        const std::wstring customFixture = L"C:\\MaxPrioGamesFixture\\Smoke.exe";
        addProgram(customFixture);
        const bool customAdded = std::any_of(settings.customPrograms.begin(), settings.customPrograms.end(),
            [&](const mpg::CustomProgram& program) { return mpg::samePath(program.path, customFixture); });
        const auto customRow = std::find_if(rows.begin(), rows.end(), [&](const Row& row) {
            return row.custom && mpg::samePath(row.path, customFixture);
        });
        if (customRow != rows.end()) {
            const int index = static_cast<int>(customRow - rows.begin());
            ListView_SetItemState(list, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            removeSelected();
        }
        const bool customRemoved = std::none_of(settings.customPrograms.begin(), settings.customPrograms.end(),
            [&](const mpg::CustomProgram& program) { return mpg::samePath(program.path, customFixture); }) &&
            std::any_of(settings.excludedPaths.begin(), settings.excludedPaths.end(),
                [&](const std::wstring& path) { return mpg::samePath(path, customFixture); });
        addProgram(customFixture);
        const bool customReadded = std::any_of(settings.customPrograms.begin(), settings.customPrograms.end(),
            [&](const mpg::CustomProgram& program) { return mpg::samePath(program.path, customFixture); }) &&
            std::none_of(settings.excludedPaths.begin(), settings.excludedPaths.end(),
                [&](const std::wstring& path) { return mpg::samePath(path, customFixture); });
        const std::wstring detectedPath = L"C:\\MaxPrioGamesFixture\\DetectedGame.exe";
        snapshot.processes = {{12345, L"Detected fixture game", detectedPath, true, true, {}},
                              {12346, L"Detected fixture game", detectedPath, true, true, {}}};
        fillList();
        const auto detectedCount = std::count_if(rows.begin(), rows.end(), [](const Row& row) { return row.detected; });
        const auto detectedRow = std::find_if(rows.begin(), rows.end(), [](const Row& row) { return row.detected; });
        if (detectedRow != rows.end()) {
            const int index = static_cast<int>(detectedRow - rows.begin());
            ListView_SetItemState(list, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            removeSelected();
        }
        const bool detectedRemoval = detectedCount == 1 &&
            std::none_of(rows.begin(), rows.end(), [](const Row& row) { return row.detected; }) &&
            std::any_of(settings.excludedPaths.begin(), settings.excludedPaths.end(),
                [&](const std::wstring& path) { return mpg::samePath(path, detectedPath); });
        ShowWindow(main, SW_MINIMIZE);
        const bool minimized = IsIconic(main) != FALSE;
        ShowWindow(main, SW_RESTORE);
        SendMessageW(main, WM_CLOSE, 0, 0);
        const bool closeToTray = IsWindow(main) && !IsWindowVisible(main);
        const bool noMaximize = !(GetWindowLongPtrW(main, GWL_STYLE) & WS_MAXIMIZEBOX);
        const bool passed = windowCreated && trayAdded && initialZero && listCreated && controlsExist && counterChanged &&
            toggled && filterWorks && minimized && closeToTray && noMaximize && !monitor && menuWorks &&
            customAdded && customRemoved && customReadded && detectedRemoval;
        auto flag = [](bool value) { return value ? "true" : "false"; };
        std::ostringstream json;
        json << "{\n  \"passed\": " << flag(passed) << ",\n"
             << "  \"fixture_mode\": true,\n  \"priority_changes\": false,\n"
             << "  \"window_created\": " << flag(windowCreated) << ",\n"
             << "  \"tray_added\": " << flag(trayAdded) << ",\n"
             << "  \"initial_zero_counter\": " << flag(initialZero) << ",\n"
             << "  \"counter_updated\": " << flag(counterChanged) << ",\n"
             << "  \"tray_menu_order\": " << flag(menuWorks) << ",\n"
             << "  \"catalog_list_created\": " << flag(listCreated) << ",\n"
             << "  \"catalog_rows\": " << mpg::catalog().size() << ",\n"
             << "  \"controls_created\": " << flag(controlsExist) << ",\n"
             << "  \"enable_disable_works\": " << flag(toggled) << ",\n"
             << "  \"search_works\": " << flag(filterWorks) << ",\n"
             << "  \"custom_add_remove_readd\": " << flag(customAdded && customRemoved && customReadded) << ",\n"
             << "  \"detected_game_remove_excludes_path\": " << flag(detectedRemoval) << ",\n"
             << "  \"normal_minimize\": " << flag(minimized) << ",\n"
             << "  \"close_to_tray\": " << flag(closeToTray) << ",\n"
             << "  \"no_maximize_button\": " << flag(noMaximize) << "\n}\n";
        const bool reported = WriteReport(reportPath, json.str());
        exitCode = passed && reported ? 0 : 2;
        DestroyWindow(main);
    }
};

LRESULT CALLBACK MainProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        app->main = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, message, wParam, lParam);
    if (message == app->taskbarCreated && app->taskbarCreated) { app->trayAdded = false; app->updateTray(true); return 0; }
    if (message == app->quitMessage && app->quitMessage) { DestroyWindow(window); return 0; }
    if (message == app->showMessage && app->showMessage) { app->show(); return 0; }
    switch (message) {
    case WM_CREATE:
        app->dpi = GetDpiForWindow(window);
        app->updateTray(true);
        if (app->smoke) PostMessageW(window, kSmokeMessage, 0, 0);
        else {
            app->monitor = std::make_unique<mpg::Monitor>([window] { PostMessageW(window, kSnapshotMessage, 0, 0); });
            app->monitor->start(app->settings);
        }
        if (app->benchmark) SetTimer(window, kBenchmarkTimer, app->benchmarkSeconds * 1000u, nullptr);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        app->draw(dc);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        SetTextColor(reinterpret_cast<HDC>(wParam), kInk);
        return reinterpret_cast<LRESULT>(app->background);
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kSearch: if (HIWORD(wParam) == EN_CHANGE) app->fillList(); return 0;
        case kAdd: app->chooseExecutable(); return 0;
        case kRemove: app->removeSelected(); return 0;
        case kToTray: app->hide(); return 0;
        case IDCANCEL: app->hide(); return 0;
        case kQuit: DestroyWindow(window); return 0;
        case kApply: app->applyInterval(); return 0;
        case kDetect: {
            auto next = app->settings;
            next.detectInstalledGames = SendDlgItemMessageW(window, kDetect, BM_GETCHECK, 0, 0) == BST_CHECKED;
            app->commit(std::move(next));
            return 0;
        }
        case kServices: {
            auto next = app->settings;
            next.includeGameServices = SendDlgItemMessageW(window, kServices, BM_GETCHECK, 0, 0) == BST_CHECKED;
            app->commit(std::move(next));
            return 0;
        }
        case kStartup: {
            std::wstring error;
            const bool success = mpg::startup::setEnabled(!mpg::startup::isEnabled(), error);
            app->status = success ? L"Startup preference updated." : error;
            SetWindowTextW(app->startupButton, mpg::startup::isEnabled() ? L"Remove from startup" : L"Add to startup");
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        }
        break;
    case WM_NOTIFY: {
        const auto* header = reinterpret_cast<NMHDR*>(lParam);
        if (header->idFrom == kList && header->code == LVN_ITEMCHANGED && !app->rebuilding) {
            const auto& item = *reinterpret_cast<NMLISTVIEW*>(lParam);
            const UINT oldState = item.uOldState & LVIS_STATEIMAGEMASK;
            const UINT newState = item.uNewState & LVIS_STATEIMAGEMASK;
            if ((item.uChanged & LVIF_STATE) && oldState != newState && oldState && newState &&
                item.iItem >= 0 && item.iItem < static_cast<int>(app->rows.size())) {
                const Row row = app->rows[item.iItem];
                // Do not rebuild a list while comctl32 is dispatching its own
                // state-change notification. Apply the copied row next message.
                app->pendingToggles.emplace_back(row, newState == INDEXTOSTATEIMAGEMASK(2));
                PostMessageW(window, kToggleMessage, 0, 0);
            }
        }
        if (header->idFrom == kList && header->code == LVN_KEYDOWN &&
            reinterpret_cast<NMLVKEYDOWN*>(lParam)->wVKey == VK_DELETE) {
            PostMessageW(window, WM_COMMAND, kRemove, 0);
        }
        return 0;
    }
    case kTrayMessage: {
        const UINT event = LOWORD(lParam);
        if (event == NIN_SELECT || event == NIN_KEYSELECT || event == WM_LBUTTONUP) app->show();
        else if (event == WM_CONTEXTMENU || event == WM_RBUTTONUP) app->contextMenu();
        else if (event == WM_MBUTTONUP) DestroyWindow(window);
        return 0;
    }
    case kSnapshotMessage: app->onSnapshot(); return 0;
    case kSmokeMessage: app->runSmoke(); return 0;
    case kToggleMessage: {
        std::vector<std::pair<Row, bool>> pending;
        pending.swap(app->pendingToggles);
        for (const auto& change : pending) app->setEnabled(change.first, change.second);
        return 0;
    }
    case WM_TIMER:
        if (wParam == kBenchmarkTimer) { KillTimer(window, kBenchmarkTimer); app->reportBenchmark(); }
        else if (wParam == kTrayRetryTimer) app->updateTray(true);
        return 0;
    case WM_DPICHANGED: {
        const UINT oldDpi = app->dpi;
        app->dpi = HIWORD(wParam);
        if (app->controlsReady) {
            app->makeFonts();
            struct Resize { App* app; UINT oldDpi; } context{app, oldDpi};
            EnumChildWindows(window, [](HWND child, LPARAM value) -> BOOL {
                auto& resize = *reinterpret_cast<Resize*>(value);
                if (GetParent(child) != resize.app->main) return TRUE;
                RECT rect{};
                GetWindowRect(child, &rect);
                MapWindowPoints(nullptr, resize.app->main, reinterpret_cast<POINT*>(&rect), 2);
                MoveWindow(child, MulDiv(rect.left, resize.app->dpi, resize.oldDpi), MulDiv(rect.top, resize.app->dpi, resize.oldDpi),
                    MulDiv(rect.right - rect.left, resize.app->dpi, resize.oldDpi), MulDiv(rect.bottom - rect.top, resize.app->dpi, resize.oldDpi), TRUE);
                SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(resize.app->font), TRUE);
                return TRUE;
            }, reinterpret_cast<LPARAM>(&context));
            ListView_SetColumnWidth(app->list, 0, app->px(280));
            ListView_SetColumnWidth(app->list, 1, app->px(85));
            ListView_SetColumnWidth(app->list, 2, app->px(142));
        }
        const auto& rect = *reinterpret_cast<RECT*>(lParam);
        SetWindowPos(window, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(window, nullptr, TRUE);
        return 0;
    }
    case WM_CLOSE: app->hide(); return 0;
    case WM_QUERYENDSESSION: return TRUE;
    case WM_ENDSESSION: if (wParam) DestroyWindow(window); return 0;
    case WM_DESTROY: app->cleanup(); PostQuitMessage(app->exitCode); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(com)) CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    bool show = false, startup = false, quit = false, enableStartup = false, disableStartup = false;
    App app;
    app.instance = instance;
    for (int i = 1; arguments && i < count; ++i) {
        const std::wstring arg = arguments[i];
        if (arg == L"--show") show = true;
        else if (arg == L"--startup") startup = true;
        else if (arg == L"--quit") quit = true;
        else if (arg == L"--enable-startup") enableStartup = true;
        else if (arg == L"--disable-startup") disableStartup = true;
        else if (arg == L"--smoke-test" && i + 1 < count) { app.smoke = true; app.reportPath = arguments[++i]; }
        else if (arg == L"--benchmark" && i + 2 < count) {
            app.benchmark = true;
            const int seconds = _wtoi(arguments[++i]);
            app.benchmarkSeconds = static_cast<unsigned>(std::clamp(seconds, 1, 3600));
            app.reportPath = arguments[++i];
        } else {
            if (arguments) LocalFree(arguments);
            if (SUCCEEDED(com)) CoUninitialize();
            return 64;
        }
    }
    if (arguments) LocalFree(arguments);
    app.quitMessage = RegisterWindowMessageW(L"MaxPrioGames.Quit.1");
    app.showMessage = RegisterWindowMessageW(L"MaxPrioGames.Show.1");
    if (enableStartup || disableStartup) {
        std::wstring error;
        const bool success = mpg::startup::setEnabled(enableStartup && !disableStartup, error);
        if (SUCCEEDED(com)) CoUninitialize();
        return success ? 0 : 1;
    }
    if (quit) {
        HWND other = FindWindowW(kMainClass, L"MaxPrioGames");
        if (other) {
            DWORD pid = 0;
            GetWindowThreadProcessId(other, &pid);
            HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
            const bool posted = PostMessageW(other, app.quitMessage, 0, 0) != FALSE;
            const DWORD result = process && posted ? WaitForSingleObject(process, 15000) : WAIT_OBJECT_0;
            if (process) CloseHandle(process);
            if (SUCCEEDED(com)) CoUninitialize();
            return !posted || result == WAIT_TIMEOUT ? 1 : 0;
        }
        if (SUCCEEDED(com)) CoUninitialize();
        return 0;
    }
    std::wstring mutexName = L"Local\\MaxPrioGames.SingleInstance.1";
    if (app.smoke) mutexName += L".Smoke." + std::to_wstring(GetCurrentProcessId());
    HANDLE mutex = CreateMutexW(nullptr, FALSE, mutexName.c_str());
    if (!mutex) { if (SUCCEEDED(com)) CoUninitialize(); return 1; }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (show) {
            HWND other = FindWindowW(kMainClass, L"MaxPrioGames");
            if (other) PostMessageW(other, app.showMessage, 0, 0);
        }
        CloseHandle(mutex);
        if (SUCCEEDED(com)) CoUninitialize();
        return 0;
    }
    app.portable = mpg::portableMode();
    app.settings = app.smoke ? mpg::Settings{} : mpg::loadSettings();
    if (app.portable && !startup && !app.benchmark && !app.smoke) {
        show = show || mpg::portableFirstLaunch();
        mpg::markPortableLaunched();
    }
    if (startup || app.benchmark) show = false;
    app.taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    app.background = CreateSolidBrush(RGB(255, 255, 255));
    app.mainIcon = MakeCounterIcon(0);
    WNDCLASSEXW definition{sizeof(definition)};
    definition.lpfnWndProc = MainProc;
    definition.hInstance = instance;
    definition.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    definition.hIcon = app.mainIcon;
    definition.hIconSm = app.mainIcon;
    definition.lpszClassName = kMainClass;
    if (!RegisterClassExW(&definition)) { CloseHandle(mutex); if (SUCCEEDED(com)) CoUninitialize(); return 1; }
    app.dpi = GetDpiForSystem();
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT outer{0, 0, app.px(576), app.px(536)};
    AdjustWindowRectExForDpi(&outer, style, FALSE, 0, app.dpi);
    FILETIME created{}, exited{}, kernel{}, user{};
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    app.startCpu = FileTimeValue(kernel) + FileTimeValue(user);
    app.startTicks = GetTickCount64();
    HWND window = CreateWindowExW(0, kMainClass, app.smoke ? L"MaxPrioGames Smoke Test" : L"MaxPrioGames",
        style, CW_USEDEFAULT, CW_USEDEFAULT, outer.right - outer.left, outer.bottom - outer.top,
        nullptr, nullptr, instance, &app);
    if (!window) { CloseHandle(mutex); if (SUCCEEDED(com)) CoUninitialize(); return 1; }
    CHANGEFILTERSTRUCT filter{sizeof(filter)};
    ChangeWindowMessageFilterEx(window, app.quitMessage, MSGFLT_ALLOW, &filter);
    ChangeWindowMessageFilterEx(window, app.showMessage, MSGFLT_ALLOW, &filter);
    // Permit this specific Explorer broadcast through UIPI into the elevated
    // controller so a restarted taskbar can recreate its notification-area icon.
    if (app.taskbarCreated) ChangeWindowMessageFilterEx(window, app.taskbarCreated, MSGFLT_ALLOW, &filter);
    if (show) app.show();
    MSG message{};
    int loop = 0;
    while ((loop = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (app.controlsReady && IsWindowVisible(window) && IsDialogMessageW(window, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    const int result = loop < 0 ? 1 : static_cast<int>(message.wParam);
    app.cleanup();
    CloseHandle(mutex);
    if (SUCCEEDED(com)) CoUninitialize();
    return result;
}
