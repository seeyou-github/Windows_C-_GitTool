# ComboBox

## Overview

`darkui::ComboBox` is a custom dark drop-down control. It does not directly wrap the native Win32 `COMBOBOX`; instead, it combines a button, a popup host, and a list to provide stable dark-theme rendering.

## Files

- `include/darkui/combobox.h`
- `src/combobox.cpp`

## Suitable Scenarios

- Format selectors, mode selectors, and filters in dark UIs
- Drop-downs that need accent items
- Win32 applications that still want `CBN_SELCHANGE` style notifications

## Main Features

- Custom-painted button area
- Custom popup host and list rendering
- Normal and accent item support
- Runtime replacement of the full item list
- Standard `WM_COMMAND + CBN_SELCHANGE` notification flow

## Core Type

```cpp
struct ComboItem {
    std::wstring text;
    std::uintptr_t userData = 0;
    bool accent = false;
};
```

## Basic Usage

```cpp
#include "darkui/combobox.h"

darkui::Theme theme;
theme.panel = RGB(37, 42, 49);
theme.button = RGB(57, 66, 78);
theme.buttonHot = RGB(74, 85, 99);
theme.border = RGB(71, 79, 92);
theme.text = RGB(231, 235, 240);
theme.arrow = RGB(160, 168, 178);
theme.popupItem = RGB(37, 42, 49);
theme.popupItemHot = RGB(57, 66, 78);
theme.popupAccentItem = RGB(28, 54, 95);
theme.popupAccentItemHot = RGB(42, 75, 126);

darkui::ComboBox combo;
combo.Create(hwnd, 1001, theme);
combo.SetItems({
    {L"default", 0, false},
    {L"mp4", 1, true},
    {L"mp3", 2, false},
});
combo.SetSelection(0);
MoveWindow(combo.hwnd(), 20, 20, 280, 38, TRUE);
```

## Parent Message Handling

```cpp
case WM_COMMAND:
    if (LOWORD(wParam) == 1001 && HIWORD(wParam) == CBN_SELCHANGE) {
        auto item = combo.GetItem(combo.GetSelection());
        SetWindowTextW(hwnd, item.text.c_str());
        return 0;
    }
    break;
```

## Common API

### `Create`

```cpp
bool Create(HWND parent, int controlId, const Theme& theme = Theme{}, DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, DWORD exStyle = 0);
```

### `SetItems`

```cpp
combo.SetItems({
    {L"A", 1, false},
    {L"B", 2, true},
});
```

### `AddItem`

```cpp
combo.AddItem({L"C", 3, false});
```

### `ClearItems`

```cpp
combo.ClearItems();
```

### `SetSelection` / `GetSelection`

```cpp
combo.SetSelection(1);
int index = combo.GetSelection();
```

### `GetItem` / `GetText` / `GetCount`

```cpp
auto item = combo.GetItem(index);
std::wstring text = combo.GetText();
std::size_t count = combo.GetCount();
```

### `SetTheme`

```cpp
combo.SetTheme(theme);
```

## Theme Fields Used

- `panel`
- `button`
- `buttonHot`
- `border`
- `text`
- `arrow`
- `popupItem`
- `popupItemHot`
- `popupAccentItem`
- `popupAccentItemHot`
- `uiFont`
- `itemHeight`
- `arrowWidth`
- `arrowRightPadding`
- `textPadding`
- `popupBorder`
- `popupOffsetY`

## Usage Notes

- Use `accent = true` for highlighted or recommended items
- Use `userData` for enum values, IDs, or application payloads
- Layout is still owned by the parent window, typically through `MoveWindow()`

## Demo Reference

For complete examples, see:

- `../demo/src/demo_combobox.cpp`
- `../demo/src/demo_combobox_only.cpp`
- `../demo/src/demo_showcase.cpp`

## Current Limitations

- No editable combo-box mode
- No multi-select support
- No built-in search
- No complex data binding layer
