#include "MainWindow.h"

#include <commctrl.h>
#include <gdiplus.h>
#include <objbase.h>
#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int nCmdShow) {
    HANDLE singleInstanceMutex = CreateMutexW(nullptr, TRUE, L"GitVisualTool_SingleInstanceMutex");
    if (singleInstanceMutex == nullptr) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existingWindow = FindWindowW(L"GitVisualToolMainWindow", nullptr);
        if (existingWindow != nullptr) {
            ShowWindow(existingWindow, IsIconic(existingWindow) ? SW_RESTORE : SW_SHOW);
            SetForegroundWindow(existingWindow);
        }
        CloseHandle(singleInstanceMutex);
        return 0;
    }

    LoadLibraryW(L"Msftedit.dll");

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Gdiplus::Ok) {
        CloseHandle(singleInstanceMutex);
        return 1;
    }

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    const HRESULT comInitResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(comInitResult)) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        CloseHandle(singleInstanceMutex);
        return 1;
    }
    const bool shouldUninitializeCom = SUCCEEDED(comInitResult);

    MainWindow window;
    if (!window.Create(instance, nCmdShow)) {
        if (shouldUninitializeCom) {
            CoUninitialize();
        }
        Gdiplus::GdiplusShutdown(gdiplusToken);
        CloseHandle(singleInstanceMutex);
        return 1;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (shouldUninitializeCom) {
        CoUninitialize();
    }
    Gdiplus::GdiplusShutdown(gdiplusToken);
    CloseHandle(singleInstanceMutex);
    return static_cast<int>(msg.wParam);
}
