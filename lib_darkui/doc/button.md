# Button

## Overview

`darkui::Button` is a custom dark owner-draw button for Win32. It preserves the standard Win32 button notification flow while giving application code control over colors, corner radius, and host surface blending.

## Files

- `include/darkui/button.h`
- `src/button.cpp`

## Suitable Scenarios

- Primary and secondary buttons in dark windows
- Rounded buttons placed on cards or panels
- Win32 applications that still want standard `BN_CLICKED` behavior

## Main Features

- Normal, hover, pressed, and disabled states
- Custom border and text colors
- Configurable corner radius
- Host surface color support to avoid visible seams around rounded corners

## Basic Usage

```cpp
#include "darkui/button.h"

darkui::Theme theme;
theme.button = RGB(52, 61, 72);
theme.buttonHover = RGB(64, 75, 88);
theme.buttonHot = RGB(72, 86, 104);
theme.buttonDisabled = RGB(58, 62, 70);
theme.buttonDisabledText = RGB(138, 144, 152);
theme.border = RGB(84, 96, 112);
theme.text = RGB(232, 236, 241);

darkui::Button button;
button.Create(hwnd, 3001, L"Run", theme);
button.SetCornerRadius(14);
button.SetSurfaceColor(theme.panel);
MoveWindow(button.hwnd(), 20, 20, 140, 40, TRUE);
```

## Parent Message Handling

```cpp
case WM_COMMAND:
    if (LOWORD(wParam) == 3001 && HIWORD(wParam) == BN_CLICKED) {
        MessageBoxW(hwnd, L"clicked", L"info", MB_OK);
        return 0;
    }
    break;
```

## Common API

### `Create`

```cpp
bool Create(HWND parent, int controlId, const std::wstring& text, const Theme& theme = Theme{}, DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, DWORD exStyle = 0);
```

### `SetTheme`

```cpp
button.SetTheme(theme);
```

### `SetText` / `GetText`

```cpp
button.SetText(L"Processing...");
std::wstring text = button.GetText();
```

### `SetCornerRadius`

```cpp
button.SetCornerRadius(18);
```

### `SetSurfaceColor`

```cpp
button.SetSurfaceColor(theme.panel);
```

Purpose:

- Sets the background color behind the rounded button corners
- Recommended when the button sits on a card or panel instead of the main window background

## Theme Fields Used

- `button`
- `buttonHover`
- `buttonHot`
- `buttonDisabled`
- `buttonDisabledText`
- `border`
- `text`
- `textPadding`
- `uiFont`
- `background`

## Usage Notes

- For plain window backgrounds, the default surface fallback is usually enough
- For cards and panels, call `SetSurfaceColor(cardColor)`
- To disable the control, use `EnableWindow(button.hwnd(), FALSE)`

## Demo Reference

For complete examples, see:

- `../demo/src/demo_button.cpp`
- `../demo/src/demo_edit.cpp`
- `../demo/src/demo_progress.cpp`
- `../demo/src/demo_showcase.cpp`
- `../demo/src/demo_slider.cpp`

## Current Limitations

- No built-in icon support
- No drop-down arrow support
- No default-button glow behavior
- No button-group management
