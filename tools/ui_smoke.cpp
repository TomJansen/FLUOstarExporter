#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
static bool waitText(HWND h, const wchar_t *needle, int milliseconds = 30000) {
    wchar_t text[4096];
    for (int i = 0; i < milliseconds / 100; ++i) {
        GetWindowTextW(h, text, 4096);
        if (wcsstr(text, needle))
            return true;
        Sleep(100);
    }
    wprintf(L"Unexpected text: %ls\n", text);
    return false;
}
static void snapshot(HWND h, const wchar_t *path) {
    RECT r;
    GetClientRect(h, &r);
    HDC dc = GetDC(h), mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, r.right, r.bottom);
    HGDIOBJ old = SelectObject(mem, bmp);
    UpdateWindow(h);
    Sleep(500);
    BitBlt(mem, 0, 0, r.right, r.bottom, dc, 0, 0, SRCCOPY);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = r.right;
    bi.bmiHeader.biHeight = r.bottom;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    DWORD size = r.right * r.bottom * 4;
    void *data = HeapAlloc(GetProcessHeap(), 0, size);
    SelectObject(mem, old);
    GetDIBits(mem, bmp, 0, r.bottom, data, &bi, DIB_RGB_COLORS);
    BITMAPFILEHEADER fh{};
    fh.bfType = 0x4d42;
    fh.bfOffBits = sizeof(fh) + sizeof(BITMAPINFOHEADER);
    fh.bfSize = fh.bfOffBits + size;
    HANDLE out =
        CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD written;
    WriteFile(out, &fh, sizeof(fh), &written, nullptr);
    WriteFile(out, &bi.bmiHeader, sizeof(BITMAPINFOHEADER), &written, nullptr);
    WriteFile(out, data, size, &written, nullptr);
    CloseHandle(out);
    HeapFree(GetProcessHeap(), 0, data);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(h, dc);
}
static bool clickRow(HWND list, HANDLE process, int index, WPARAM modifiers) {
    RECT rect{};
    rect.left = LVIR_BOUNDS;
    auto remote =
        VirtualAllocEx(process, nullptr, sizeof(rect), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote)
        return false;
    WriteProcessMemory(process, remote, &rect, sizeof(rect), nullptr);
    auto ok = SendMessageW(list, LVM_GETITEMRECT, index, (LPARAM)remote);
    ReadProcessMemory(process, remote, &rect, sizeof(rect), nullptr);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    if (!ok)
        return false;
    LPARAM point = MAKELPARAM(rect.left + 50, (rect.top + rect.bottom) / 2);
    SendMessageW(list, WM_LBUTTONDOWN, MK_LBUTTON | modifiers, point);
    SendMessageW(list, WM_LBUTTONUP, modifiers, point);
    return true;
}
int wmain(int argc, wchar_t **argv) {
    if (argc == 1) {
        auto h = FindWindowW(L"FluoExporterWindow", nullptr);
        if (h)
            SendMessageW(h, WM_CLOSE, 0, 0);
        return 0;
    }
    if (argc != 9)
        return 2;
    wchar_t command[32768];
    swprintf(command, 32768, L"\"%ls\"", argv[1]);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, command, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        return 3;
    HWND h = nullptr;
    for (int i = 0; i < 300 && !h; ++i) {
        h = FindWindowW(L"FluoExporterWindow", nullptr);
        Sleep(100);
    }
    if (!h)
        return 4;
    if (!waitText(GetDlgItem(h, 204), argv[5]))
        return 16;
    wchar_t detected[32768];
    SendMessageW(GetDlgItem(h, 101), WM_GETTEXT, 32768, (LPARAM)detected);
    if (!wcsstr(detected, L"C:\\Program Files (x86)\\BMG"))
        return 17;
    SendMessageW(GetDlgItem(h, 101), WM_SETTEXT, 0, (LPARAM)argv[2]);
    SendMessageW(h, WM_COMMAND, 106, 0);
    if (!waitText(GetDlgItem(h, 204), argv[5]))
        return 5;
    if (SendMessageW(GetDlgItem(h, 111), CB_GETCURSEL, 0, 0) != 0)
        return 9;
    wchar_t expectedPath[32768];
    SendMessageW(GetDlgItem(h, 101), WM_GETTEXT, 32768, (LPARAM)expectedPath);
    snapshot(h, argv[4]);
    HWND table = GetDlgItem(h, 205);
    auto totalRuns = SendMessageW(table, LVM_GETITEMCOUNT, 0, 0);
    SendMessageW(GetDlgItem(h, 103), WM_SETTEXT, 0, (LPARAM)L"Data file missing");
    auto missingRuns = SendMessageW(table, LVM_GETITEMCOUNT, 0, 0);
    SendMessageW(GetDlgItem(h, 103), WM_SETTEXT, 0, (LPARAM)L"Ready");
    auto readyRuns = SendMessageW(table, LVM_GETITEMCOUNT, 0, 0);
    if (missingRuns < 1 || readyRuns < 1 || missingRuns + readyRuns != totalRuns)
        return 28;
    SendMessageW(GetDlgItem(h, 103), WM_SETTEXT, 0, (LPARAM)L"");
    SendMessageW(table, LVM_ENSUREVISIBLE, 0, FALSE);
    SetFocus(GetDlgItem(h, 103));
    POINT wheelPoint{20, 80};
    ClientToScreen(table, &wheelPoint);
    for (int i = 0; i < 3; ++i)
        SendMessageW(h, WM_MOUSEWHEEL, MAKEWPARAM(0, -WHEEL_DELTA),
                     MAKELPARAM(wheelPoint.x, wheelPoint.y));
    if (SendMessageW(table, LVM_GETTOPINDEX, 0, 0) == 0)
        return 25;
    SendMessageW(GetDlgItem(h, 103), WM_SETTEXT, 0, (LPARAM)argv[6]);
    if (SendMessageW(table, LVM_GETITEMCOUNT, 0, 0) < 7)
        return 18;
    SendMessageW(h, WM_COMMAND, 108, 0);
    if (!clickRow(table, pi.hProcess, 1, 0) || !clickRow(table, pi.hProcess, 4, MK_SHIFT))
        return 19;
    if (SendMessageW(table, LVM_GETSELECTEDCOUNT, 0, 0) != 4)
        return 20;
    if (!clickRow(table, pi.hProcess, 6, MK_CONTROL))
        return 21;
    if (SendMessageW(table, LVM_GETSELECTEDCOUNT, 0, 0) != 5)
        return 22;
    SendMessageW(h, WM_COMMAND, 108, 0);
    if (SendMessageW(table, LVM_GETSELECTEDCOUNT, 0, 0) != 0)
        return 23;
    SendMessageW(GetDlgItem(h, 103), WM_SETTEXT, 0, (LPARAM)argv[7]);
    if (SendMessageW(table, LVM_GETITEMCOUNT, 0, 0) < 1)
        return 24;

    SendMessageW(GetDlgItem(h, 102), WM_SETTEXT, 0, (LPARAM)argv[3]);
    // Use a known single run based on its protocol then choose a single result.
    SendMessageW(GetDlgItem(h, 103), WM_SETTEXT, 0, (LPARAM)argv[8]);
    if (SendMessageW(GetDlgItem(h, 205), LVM_GETITEMCOUNT, 0, 0) < 1)
        return 6;
    SendMessageW(h, WM_COMMAND, 107, 0);
    if (!IsWindowEnabled(GetDlgItem(h, 109)))
        return 7;
    // Keep the selected run but restore the full list so it can scroll while exporting.
    SendMessageW(GetDlgItem(h, 103), WM_SETTEXT, 0, (LPARAM)L"");
    auto hoverScrolls = [&] {
        if (!IsWindowEnabled(table))
            return false;
        SendMessageW(table, LVM_ENSUREVISIBLE, 0, FALSE);
        POINT point{20, 80};
        ClientToScreen(table, &point);
        for (int i = 0; i < 3; ++i)
            SendMessageW(h, WM_MOUSEWHEEL, MAKEWPARAM(0, -WHEEL_DELTA),
                         MAKELPARAM(point.x, point.y));
        return SendMessageW(table, LVM_GETTOPINDEX, 0, 0) > 0;
    };
    SendMessageW(h, WM_COMMAND, 109, 0);
    if (!hoverScrolls())
        return 29;
    if (!waitText(GetDlgItem(h, 206), L"0 failed.", 60000))
        return 8;
    if (!hoverScrolls())
        return 30;
    SendMessageW(GetDlgItem(h, 111), CB_SETCURSEL, 1, 0);
    SendMessageW(h, WM_COMMAND, MAKEWPARAM(111, CBN_SELCHANGE), (LPARAM)GetDlgItem(h, 111));
    SendMessageW(h, WM_COMMAND, 109, 0);
    if (!hoverScrolls())
        return 29;
    if (!waitText(GetDlgItem(h, 206), L"0 failed.", 60000))
        return 10;
    if (!hoverScrolls())
        return 31;
    SendMessageW(h, WM_CLOSE, 0, 0);
    if (WaitForSingleObject(pi.hProcess, 5000) != WAIT_OBJECT_0)
        return 26;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (!CreateProcessW(nullptr, command, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        return 11;
    h = nullptr;
    for (int i = 0; i < 300 && !h; ++i) {
        h = FindWindowW(L"FluoExporterWindow", nullptr);
        Sleep(100);
    }
    if (!h)
        return 12;
    if (!waitText(GetDlgItem(h, 204), argv[5]))
        return 13;
    if (SendMessageW(GetDlgItem(h, 111), CB_GETCURSEL, 0, 0) != 1)
        return 14;
    wchar_t savedPath[32768];
    SendMessageW(GetDlgItem(h, 101), WM_GETTEXT, 32768, (LPARAM)savedPath);
    if (wcscmp(savedPath, expectedPath))
        return 15;
    SendMessageW(GetDlgItem(h, 111), CB_SETCURSEL, 0, 0);
    SendMessageW(h, WM_COMMAND, MAKEWPARAM(111, CBN_SELCHANGE), (LPARAM)GetDlgItem(h, 111));
    SendMessageW(h, WM_CLOSE, 0, 0);
    if (WaitForSingleObject(pi.hProcess, 5000) != WAIT_OBJECT_0)
        return 27;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    wprintf(L"PASS: source load, hover scrolling before/during/after export, Shift-click range, Ctrl-click, clear, status/ID search, "
            L"select-all, XLSX/CSV exports, saved source/format on restart, clean process exit\n");
    return 0;
}
