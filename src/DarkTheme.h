#pragma once

#include <windows.h>

class DarkTheme {
public:
    static COLORREF WindowBackground();
    static COLORREF PanelBackground();
    static COLORREF ControlBackground();
    static COLORREF AccentBackground();
    static COLORREF BorderColor();
    static COLORREF TextColor();
    static HBRUSH WindowBrush();
    static HBRUSH PanelBrush();
    static HBRUSH ControlBrush();
    static void ApplyTitleBar(HWND hwnd);
    static void ApplyDarkControlTheme(HWND hwnd);
    static void DisableVisualTheme(HWND hwnd);
};
