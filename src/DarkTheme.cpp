#include "DarkTheme.h"

#include <dwmapi.h>
#include <uxtheme.h>

namespace {
constexpr COLORREF kWindow = RGB(24, 26, 31);
constexpr COLORREF kPanel = RGB(30, 33, 39);
constexpr COLORREF kControl = RGB(40, 44, 52);
constexpr COLORREF kAccent = RGB(58, 64, 76);
constexpr COLORREF kBorder = RGB(72, 78, 92);
constexpr COLORREF kText = RGB(225, 228, 232);
}

COLORREF DarkTheme::WindowBackground() { return kWindow; }
COLORREF DarkTheme::PanelBackground() { return kPanel; }
COLORREF DarkTheme::ControlBackground() { return kControl; }
COLORREF DarkTheme::AccentBackground() { return kAccent; }
COLORREF DarkTheme::BorderColor() { return kBorder; }
COLORREF DarkTheme::TextColor() { return kText; }

HBRUSH DarkTheme::WindowBrush() {
    static HBRUSH brush = CreateSolidBrush(kWindow);
    return brush;
}

HBRUSH DarkTheme::PanelBrush() {
    static HBRUSH brush = CreateSolidBrush(kPanel);
    return brush;
}

HBRUSH DarkTheme::ControlBrush() {
    static HBRUSH brush = CreateSolidBrush(kControl);
    return brush;
}

void DarkTheme::ApplyTitleBar(HWND hwnd) {
    const BOOL enabled = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &enabled, sizeof(enabled));
    DwmSetWindowAttribute(hwnd, 19, &enabled, sizeof(enabled));
}

void DarkTheme::ApplyDarkControlTheme(HWND hwnd) {
    HMODULE themeModule = LoadLibraryW(L"uxtheme.dll");
    if (themeModule == nullptr) {
        return;
    }

    using SetWindowThemeProc = HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR);
    auto setWindowTheme = reinterpret_cast<SetWindowThemeProc>(
        GetProcAddress(themeModule, "SetWindowTheme"));
    if (setWindowTheme != nullptr) {
        setWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
    }
    FreeLibrary(themeModule);
}

void DarkTheme::DisableVisualTheme(HWND hwnd) {
    HMODULE themeModule = LoadLibraryW(L"uxtheme.dll");
    if (themeModule == nullptr) {
        return;
    }

    using SetWindowThemeProc = HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR);
    auto setWindowTheme = reinterpret_cast<SetWindowThemeProc>(
        GetProcAddress(themeModule, "SetWindowTheme"));
    if (setWindowTheme != nullptr) {
        setWindowTheme(hwnd, L"", L"");
    }
    FreeLibrary(themeModule);
}
