#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "core.hpp"
#include <atomic>
#include <commctrl.h>
#include <cwctype>
#include <set>
#include <shellapi.h>
#include <shlobj.h>
#include <thread>
#include <windows.h>
using namespace fluo;
static HWND mainWindow, list, sourceEdit, outputEdit, searchEdit, statusLabel, titleLabel,
    countLabel, formatCombo;
static HFONT font, titleFont;
static std::vector<Run> runs;
static std::vector<size_t> visible;
static fs::path executableDir, sourceDir, settingsPath;
static ExportFormat exportFormat = ExportFormat::Xlsx;
static bool busy = false, sortAscending = false, rebuilding = false;
static int sortColumn = 0;
static std::thread worker;
static std::atomic<bool> cancelled{false};
static const UINT PROGRESS = WM_APP + 1, FINISHED = WM_APP + 2;
static std::vector<HWND> actions;
enum {
    SOURCE = 101,
    OUTPUT,
    SEARCH,
    BROWSESOURCE,
    BROWSEOUTPUT,
    REFRESH,
    SELECTALL,
    CLEAR,
    EXPORT,
    CANCEL,
    FORMAT
};
static std::wstring wide(const std::string &s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring r(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), r.data(), n);
    return r;
}
static std::wstring textOf(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s(n + 1, 0);
    GetWindowTextW(h, s.data(), n + 1);
    s.resize(n);
    return s;
}
static void error(const std::exception &e);
static void releasePortableDirectory() {
    wchar_t windowsDirectory[MAX_PATH];
    if (GetWindowsDirectoryW(windowsDirectory, MAX_PATH))
        SetCurrentDirectoryW(windowsDirectory);
    SetDllDirectoryW(L"");
}
static std::wstring setting(const wchar_t *key, const wchar_t *fallback = L"") {
    wchar_t value[32768];
    GetPrivateProfileStringW(L"Settings", key, fallback, value, 32768, settingsPath.c_str());
    return value;
}
static bool saveSettings() {
    try {
        if (!fs::exists(settingsPath)) {
            std::ofstream file(settingsPath, std::ios::binary);
            file << "\xff\xfe";
            if (!file)
                throw std::runtime_error("Cannot create settings file beside the executable");
        }
        auto output = textOf(outputEdit);
        if (fs::path(output) == executableDir / L"Exports")
            output.clear();
        bool ok = WritePrivateProfileStringW(L"Settings", L"ExportFormat",
                                             exportFormat == ExportFormat::Xlsx ? L"XLSX" : L"CSV",
                                             settingsPath.c_str());
        ok = WritePrivateProfileStringW(L"Settings", L"DatabasePath", sourceDir.c_str(),
                                        settingsPath.c_str()) &&
             ok;
        ok = WritePrivateProfileStringW(L"Settings", L"OutputPath", output.c_str(),
                                        settingsPath.c_str()) &&
             ok;
        if (!ok)
            throw std::runtime_error(
                "Cannot save settings beside the executable; check that the USB stick is writable");
        return true;
    } catch (const std::exception &e) {
        error(e);
        return false;
    }
}
static void formatLabel() {
    SetWindowTextW(GetDlgItem(mainWindow, EXPORT), exportFormat == ExportFormat::Xlsx
                                                       ? L"Export selected to XLSX"
                                                       : L"Export selected to CSV");
}
static std::wstring lower(std::wstring s) {
    for (auto &c : s)
        c = towlower(c);
    return s;
}
static void error(const std::exception &e) {
    MessageBoxW(mainWindow, wide(e.what()).c_str(), L"FLUOstar Exporter", MB_OK | MB_ICONERROR);
}
static HWND control(const wchar_t *cls, const wchar_t *text, DWORD style, int id) {
    HWND h = CreateWindowExW(cls == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0, cls, text,
                             WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, mainWindow,
                             (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(h, WM_SETFONT, (WPARAM)font, TRUE);
    return h;
}
static HWND button(const wchar_t *text, int id) {
    auto h = control(L"BUTTON", text, WS_TABSTOP | BS_PUSHBUTTON, id);
    actions.push_back(h);
    return h;
}
static void position(int id, int x, int y, int w, int h) {
    MoveWindow(GetDlgItem(mainWindow, id), x, y, w, h, TRUE);
}
static void layout() {
    RECT r;
    GetClientRect(mainWindow, &r);
    int w = r.right, h = r.bottom, m = 24;
    MoveWindow(titleLabel, m, 18, w - 2 * m, 34, TRUE);
    position(201, m, 64, 90, 22);
    MoveWindow(sourceEdit, m + 94, 60, w - 2 * m - 302, 28, TRUE);
    position(BROWSESOURCE, w - m - 198, 60, 94, 28);
    position(REFRESH, w - m - 94, 60, 94, 28);
    position(202, m, 100, 90, 22);
    MoveWindow(outputEdit, m + 94, 96, w - 2 * m - 198, 28, TRUE);
    position(BROWSEOUTPUT, w - m - 94, 96, 94, 28);
    position(203, m, 148, 60, 22);
    MoveWindow(searchEdit, m + 64, 143, w - 2 * m - 284, 30, TRUE);
    position(207, w - m - 208, 148, 52, 22);
    MoveWindow(formatCombo, w - m - 150, 143, 150, 160, TRUE);
    MoveWindow(countLabel, m, 184, w - 2 * m - 220, 24, TRUE);
    position(SELECTALL, w - m - 208, 179, 100, 28);
    position(CLEAR, w - m - 100, 179, 100, 28);
    MoveWindow(list, m, 217, w - 2 * m, h - 309, TRUE);
    MoveWindow(statusLabel, m, h - 78, w - 2 * m - 340, 58, TRUE);
    position(CANCEL, w - m - 322, h - 72, 102, 36);
    position(EXPORT, w - m - 208, h - 72, 208, 36);
}
static size_t selectedCount() {
    return size_t(ListView_GetSelectedCount(list));
}
static void counts() {
    auto s = std::to_wstring(visible.size()) + L" of " + std::to_wstring(runs.size()) +
             L" runs  |  " + std::to_wstring(selectedCount()) + L" selected";
    SetWindowTextW(countLabel, s.c_str());
    EnableWindow(GetDlgItem(mainWindow, EXPORT), !busy && selectedCount() > 0);
}
static void fill(bool preserve = true) {
    std::set<std::string> checked;
    if (preserve)
        for (size_t i = 0; i < visible.size(); ++i)
            if ((ListView_GetItemState(list, int(i), LVIS_SELECTED) & LVIS_SELECTED))
                checked.insert(runs[visible[i]].id);
    rebuilding = true;
    visible.clear();
    auto query = lower(textOf(searchEdit));
    for (size_t i = 0; i < runs.size(); ++i) {
        auto &r = runs[i];
        auto hay = lower(wide(r.protocol + " " + r.stamp + " " + r.technique + " " + r.user + " " +
                              r.plate + " " + r.protocolComment + " " + r.id1 + " " + r.id2 + " " +
                              r.id3 + " " + r.id + " " + (r.available ? "Ready" : "Data file missing")));
        if (query.empty() || hay.find(query) != hay.npos)
            visible.push_back(i);
    }
    std::stable_sort(visible.begin(), visible.end(), [](size_t a, size_t b) {
        auto value = [](const Run &r) {
            switch (sortColumn) {
            case 1:
                return r.protocol;
            case 2:
                return r.id1;
            case 3:
                return r.id2;
            case 4:
                return r.id3;
            case 5:
                return r.user;
            case 6:
                return r.technique;
            case 7:
                return r.plate;
            case 8:
                return r.protocolComment;
            case 9:
                return r.id;
            case 10:
                return r.available ? std::string("Ready") : std::string("Missing");
            default:
                return r.stamp;
            }
        };
        auto x = value(runs[a]), y = value(runs[b]);
        return sortAscending ? x < y : x > y;
    });
    SendMessageW(list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(list);
    for (size_t i = 0; i < visible.size(); ++i) {
        auto &r = runs[visible[i]];
        std::vector<std::string> vals = {r.stamp,
                                         r.protocol,
                                         r.id1,
                                         r.id2,
                                         r.id3,
                                         r.user,
                                         r.technique,
                                         r.plate,
                                         r.protocolComment,
                                         r.id,
                                         r.available ? "Ready" : "Data file missing"};
        LVITEMW item{};
        auto s = wide(vals[0]);
        item.mask = LVIF_TEXT;
        item.iItem = int(i);
        item.pszText = s.data();
        ListView_InsertItem(list, &item);
        for (size_t c = 1; c < vals.size(); ++c) {
            auto v = wide(vals[c]);
            ListView_SetItemText(list, int(i), int(c), v.data());
        }
        if (checked.count(r.id))
            ListView_SetItemState(list, int(i), LVIS_SELECTED, LVIS_SELECTED);
    }
    SendMessageW(list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list, nullptr, TRUE);
    rebuilding = false;
    counts();
}
static fs::path chooseFolder(const wchar_t *title, const fs::path &initial) {
    IFileDialog *dialog = nullptr;
    fs::path chosen;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dialog)))) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        dialog->SetTitle(title);
        IShellItem *start = nullptr;
        if (!initial.empty() && SUCCEEDED(SHCreateItemFromParsingName(initial.c_str(), nullptr,
                                                                      IID_PPV_ARGS(&start)))) {
            dialog->SetFolder(start);
            start->Release();
        }
        if (SUCCEEDED(dialog->Show(mainWindow))) {
            IShellItem *result = nullptr;
            if (SUCCEEDED(dialog->GetResult(&result))) {
                PWSTR p = nullptr;
                if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
                    chosen = p;
                    CoTaskMemFree(p);
                }
                result->Release();
            }
        }
        dialog->Release();
    }
    return chosen;
}
static void load() {
    try {
        auto folder = locate(fs::path(textOf(sourceEdit)));
        auto loaded = loadRuns(folder);
        sourceDir = folder;
        runs = std::move(loaded);
        SetWindowTextW(sourceEdit, folder.c_str());
        fill(false);
        saveSettings();
        size_t missing = 0;
        for (auto &r : runs)
            if (!r.available)
                ++missing;
        auto msg = std::to_wstring(runs.size()) + L" runs loaded. " + std::to_wstring(missing) +
                   L" missing data files. Shift-click selects a range; Ctrl-click adds rows.";
        SetWindowTextW(statusLabel, msg.c_str());
    } catch (const std::exception &e) {
        error(e);
    }
}
struct Completion {
    size_t succeeded = 0, failed = 0, records = 0;
    bool cancelled = false;
    std::string details;
};
static void exporting() {
    std::vector<Run> chosen;
    for (size_t i = 0; i < visible.size(); ++i)
        if ((ListView_GetItemState(list, int(i), LVIS_SELECTED) & LVIS_SELECTED))
            chosen.push_back(runs[visible[i]]);
    if (chosen.empty())
        return;
    auto output = fs::path(textOf(outputEdit));
    try {
        if (output.empty() || !output.is_absolute())
            throw std::runtime_error("Choose an absolute output folder");
        fs::create_directories(output);
        if (fs::equivalent(sourceDir, output))
            throw std::runtime_error("Choose an output folder separate from the source database");
    } catch (const std::exception &e) {
        error(e);
        return;
    }
    if (!saveSettings())
        return;
    auto format = exportFormat;
    if (worker.joinable())
        worker.join();
    busy = true;
    cancelled = false;
    for (auto h : actions)
        EnableWindow(h, FALSE);
    EnableWindow(sourceEdit, FALSE);
    EnableWindow(outputEdit, FALSE);
    EnableWindow(searchEdit, FALSE);
    // The worker uses a snapshot of the selection; keep the list available for scrolling.
    EnableWindow(formatCombo, FALSE);
    EnableWindow(GetDlgItem(mainWindow, CANCEL), TRUE);
    SetWindowTextW(statusLabel, L"Exporting... Files appear after each run finishes.");
    worker = std::thread([chosen = std::move(chosen), output, format] {
        auto result = new Completion;
        try {
            for (size_t i = 0; i < chosen.size(); ++i) {
                if (cancelled) {
                    result->cancelled = true;
                    break;
                }
                auto &r = chosen[i];
                try {
                    auto exported = exportRun(r, output, format);
                    ++result->succeeded;
                    result->records += exported.records;
                } catch (const std::exception &e) {
                    ++result->failed;
                    result->details +=
                        "Run " + r.id + " (" + r.protocol + "): " + e.what() + "\r\n";
                }
                auto msg = new std::wstring(L"Processed " + std::to_wstring(i + 1) + L" / " +
                                            std::to_wstring(chosen.size()) + L" runs  |  " +
                                            std::to_wstring(result->failed) + L" failed");
                PostMessageW(mainWindow, PROGRESS, 0, (LPARAM)msg);
            }
        } catch (const std::exception &e) {
            ++result->failed;
            result->details += e.what();
        }
        PostMessageW(mainWindow, FINISHED, 0, (LPARAM)result);
    });
}
static LRESULT CALLBACK windowProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        mainWindow = h;
        HDC dc = GetDC(h);
        int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(h, dc);
        font = CreateFontW(-MulDiv(10, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        titleFont = CreateFontW(-MulDiv(19, dpi, 72), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        titleLabel = control(L"STATIC", L"FLUOstar Exporter", SS_LEFT, 200);
        SendMessageW(titleLabel, WM_SETFONT, (WPARAM)titleFont, TRUE);
        control(L"STATIC", L"Data folder", SS_LEFT, 201);
        sourceEdit = control(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, SOURCE);
        button(L"Browse...", BROWSESOURCE);
        button(L"Refresh", REFRESH);
        control(L"STATIC", L"Export to", SS_LEFT, 202);
        outputEdit = control(L"EDIT", (executableDir / L"Exports").c_str(),
                             WS_TABSTOP | ES_AUTOHSCROLL, OUTPUT);
        button(L"Browse...", BROWSEOUTPUT);
        auto savedOutput = setting(L"OutputPath");
        if (!savedOutput.empty())
            SetWindowTextW(outputEdit, savedOutput.c_str());
        control(L"STATIC", L"Search", SS_LEFT, 203);
        searchEdit = control(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, SEARCH);
        SendMessageW(searchEdit, EM_SETCUEBANNER, TRUE,
                     (LPARAM)L"Protocol, ID1 / ID2 / ID3, date, user or comment...");
        control(L"STATIC", L"Format", SS_LEFT, 207);
        formatCombo = control(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, FORMAT);
        SendMessageW(formatCombo, CB_ADDSTRING, 0, (LPARAM)L"Excel (.xlsx)");
        SendMessageW(formatCombo, CB_ADDSTRING, 0, (LPARAM)L"CSV (.csv)");
        exportFormat =
            setting(L"ExportFormat", L"XLSX") == L"CSV" ? ExportFormat::Csv : ExportFormat::Xlsx;
        SendMessageW(formatCombo, CB_SETCURSEL, exportFormat == ExportFormat::Xlsx ? 0 : 1, 0);
        countLabel = control(L"STATIC", L"No runs loaded", SS_LEFT, 204);
        button(L"Select all", SELECTALL);
        button(L"Clear", CLEAR);
        list = control(WC_LISTVIEWW, L"", WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
                       205);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER |
                                                    LVS_EX_GRIDLINES);
        const wchar_t *labels[] = {L"Measurement date",
                                   L"Protocol",
                                   L"ID1",
                                   L"ID2",
                                   L"ID3",
                                   L"User",
                                   L"Technique",
                                   L"Plate",
                                   L"Protocol comment",
                                   L"Run ID",
                                   L"Status"};
        int widths[] = {174, 235, 190, 220, 170, 90, 165, 150, 240, 74, 135};
        for (int i = 0; i < 11; ++i) {
            LVCOLUMNW col{};
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            col.pszText = (LPWSTR)labels[i];
            col.cx = widths[i];
            ListView_InsertColumn(list, i, &col);
        }
        statusLabel =
            control(L"STATIC", L"Choose the OMEGA data folder to get started.", SS_LEFT, 206);
        button(L"Stop", CANCEL);
        button(L"Export selected to XLSX", EXPORT);
        formatLabel();
        EnableWindow(GetDlgItem(h, CANCEL), FALSE);
        EnableWindow(GetDlgItem(h, EXPORT), FALSE);
        for (auto p :
             {fs::path(setting(L"DatabasePath")), fs::path(L"C:\\Program Files (x86)\\BMG"),
              executableDir, executableDir.parent_path(), fs::path(L"C:\\Program Files\\BMG")}) {
            try {
                if (p.empty())
                    continue;
                auto found = locate(p);
                SetWindowTextW(sourceEdit, found.c_str());
                PostMessageW(h, WM_COMMAND, REFRESH, 0);
                break;
            } catch (...) {
            }
        }
        return 0;
    }
    case WM_SIZE:
        layout();
        return 0;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lp)->ptMinTrackSize = {960, 580};
        return 0;
    case WM_MOUSEWHEEL: {
        POINT point{(short)LOWORD(lp), (short)HIWORD(lp)};
        HWND hovered = WindowFromPoint(point);
        if (hovered == list || IsChild(list, hovered)) {
            SendMessageW(list, WM_MOUSEWHEEL, wp, lp);
            return 0;
        }
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (busy && id != CANCEL)
            return 0;
        if (id == BROWSESOURCE) {
            auto p = chooseFolder(L"Choose BMG OMEGA data folder", fs::path(textOf(sourceEdit)));
            if (!p.empty()) {
                SetWindowTextW(sourceEdit, p.c_str());
                load();
            }
        } else if (id == BROWSEOUTPUT) {
            auto p = chooseFolder(L"Choose export output folder", fs::path(textOf(outputEdit)));
            if (!p.empty()) {
                SetWindowTextW(outputEdit, p.c_str());
                saveSettings();
            }
        } else if (id == FORMAT && HIWORD(wp) == CBN_SELCHANGE) {
            exportFormat = SendMessageW(formatCombo, CB_GETCURSEL, 0, 0) == 0 ? ExportFormat::Xlsx
                                                                              : ExportFormat::Csv;
            formatLabel();
            saveSettings();
        } else if (id == REFRESH)
            load();
        else if (id == SEARCH && HIWORD(wp) == EN_CHANGE) {
            fill();
            SetWindowTextW(statusLabel, L"Shift-click selects a range; Ctrl-click adds rows. "
                                        L"Export uses the highlighted rows.");
        } else if (id == SELECTALL || id == CLEAR) {
            rebuilding = true;
            SendMessageW(list, WM_SETREDRAW, FALSE, 0);
            for (int i = 0; i < ListView_GetItemCount(list); ++i)
                ListView_SetItemState(list, i, id == SELECTALL ? LVIS_SELECTED : 0, LVIS_SELECTED);
            SendMessageW(list, WM_SETREDRAW, TRUE, 0);
            InvalidateRect(list, nullptr, TRUE);
            rebuilding = false;
            counts();
        } else if (id == EXPORT)
            exporting();
        else if (id == CANCEL) {
            cancelled = true;
            EnableWindow(GetDlgItem(h, CANCEL), FALSE);
            SetWindowTextW(statusLabel, L"Stopping after the current run...");
        }
        return 0;
    }
    case WM_NOTIFY: {
        auto hdr = (NMHDR *)lp;
        if (hdr->hwndFrom == list && hdr->code == LVN_ITEMCHANGED) {
            if (!rebuilding)
                counts();
            return 0;
        }
        if (hdr->hwndFrom == list && hdr->code == LVN_KEYDOWN && !busy) {
            auto key = (NMLVKEYDOWN *)lp;
            if (key->wVKey == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                SendMessageW(h, WM_COMMAND, SELECTALL, 0);
                return 0;
            }
        }
        if (hdr->hwndFrom == list && hdr->code == LVN_COLUMNCLICK && !busy) {
            int col = ((NMLISTVIEW *)lp)->iSubItem;
            sortAscending = sortColumn == col ? !sortAscending : true;
            sortColumn = col;
            fill();
            return 0;
        }
        break;
    }
    case PROGRESS: {
        auto s = (std::wstring *)lp;
        SetWindowTextW(statusLabel, s->c_str());
        delete s;
        return 0;
    }
    case FINISHED: {
        auto r = (Completion *)lp;
        if (worker.joinable())
            worker.join();
        busy = false;
        for (auto a : actions)
            EnableWindow(a, TRUE);
        EnableWindow(GetDlgItem(h, CANCEL), FALSE);
        EnableWindow(sourceEdit, TRUE);
        EnableWindow(outputEdit, TRUE);
        EnableWindow(searchEdit, TRUE);
        EnableWindow(formatCombo, TRUE);
        counts();
        auto s = std::to_wstring(r->succeeded) + L" files exported (" +
                 std::to_wstring(r->records) + L" plate-well records). " + std::to_wstring(r->failed) +
                 L" failed." + (r->cancelled ? L" Stopped." : L"");
        SetWindowTextW(statusLabel, s.c_str());
        if (r->failed) {
            auto detail = wide(r->details);
            if (detail.size() > 8000)
                detail = detail.substr(0, 8000) + L"\r\n[More failures omitted from this dialog]";
            MessageBoxW(h, (s + L"\r\n\r\n" + detail).c_str(), L"Export results",
                        MB_OK | MB_ICONWARNING);
        }
        delete r;
        return 0;
    }
    case WM_CLOSE:
        if (busy) {
            cancelled = true;
            SetWindowTextW(statusLabel,
                           L"Stopping after the current run. Close the window when it finishes.");
            return 0;
        }
        saveSettings();
        // Flush the profile cache before the removable drive is ejected.
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, settingsPath.c_str());
        releasePortableDirectory();
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        if (worker.joinable())
            worker.join();
        DeleteObject(font);
        DeleteObject(titleFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX ic{sizeof(ic), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&ic);
    wchar_t path[32768];
    GetModuleFileNameW(nullptr, path, 32768);
    executableDir = fs::path(path).parent_path();
    settingsPath = executableDir / L"FLUOstarExporter.ini";
    // Explorer commonly starts portable applications with the USB folder as their current
    // directory. Move it to Windows so that the process does not keep the USB directory open.
    releasePortableDirectory();
    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FluoExporterWindow";
    RegisterClassW(&wc);
    auto h = CreateWindowExW(0, wc.lpszClassName, L"FLUOstar Exporter", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1240, 780, nullptr, nullptr, instance,
                             nullptr);
    if (!h)
        return 1;
    ShowWindow(h, show);
    UpdateWindow(h);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_MOUSEWHEEL && msg.hwnd != list) {
            POINT point{(short)LOWORD(msg.lParam), (short)HIWORD(msg.lParam)};
            HWND hovered = WindowFromPoint(point);
            if (hovered == list || IsChild(list, hovered)) {
                SendMessageW(list, WM_MOUSEWHEEL, msg.wParam, msg.lParam);
                continue;
            }
        }
        if (!IsDialogMessageW(h, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    CoUninitialize();
    return int(msg.wParam);
}
