# Toolbar

## Overview

`darkui::Toolbar` is a custom dark toolbar control. It supports standard buttons, icon buttons, separators, right-aligned items, drop-down menus, and overflow handling for compact layouts.

## Files

- `include/darkui/toolbar.h`
- `src/toolbar.cpp`

## Suitable Scenarios

- Top application toolbars
- File-manager style action bars
- Menu-bar style drop-down command surfaces

## Main Features

- Standard text buttons
- Icon buttons and icon-only buttons
- Separator items
- Right-aligned item group
- Drop-down menus
- Automatic overflow handling
- Standard `WM_COMMAND` notification flow

## Core Type

```cpp
struct ToolbarItem {
    std::wstring text;
    int commandId = 0;
    HICON icon = nullptr;
    HMENU popupMenu = nullptr;
    std::uintptr_t userData = 0;
    bool separator = false;
    bool checked = false;
    bool disabled = false;
    bool alignRight = false;
    bool iconOnly = false;
    bool dropDown = false;
    int iconScalePercent = 0;
};
```

## Basic Usage

```cpp
#include "darkui/toolbar.h"

darkui::Theme theme;
theme.toolbarBackground = RGB(25, 28, 32);
theme.toolbarItem = RGB(46, 51, 58);
theme.toolbarItemHot = RGB(64, 71, 82);
theme.toolbarItemActive = RGB(78, 120, 184);
theme.toolbarText = RGB(228, 232, 238);
theme.toolbarTextActive = RGB(248, 250, 252);
theme.toolbarSeparator = RGB(70, 76, 86);
theme.toolbarHeight = 44;

darkui::Toolbar toolbar;
toolbar.Create(hwnd, 8001, theme);
toolbar.SetItems({
    {L"New", 8101, iconNew},
    {L"Open", 8102, iconOpen},
    {L"", 0, nullptr, nullptr, 0, true},
    {L"", 8104, iconSearch, nullptr, 0, false, false, false, false, true},
    {L"Share", 8103, iconShare, nullptr, 0, false, false, false, true},
    {L"Layout", 8105, iconLayout, layoutMenu, 0, false, false, false, true, false, true},
});
MoveWindow(toolbar.hwnd(), 0, 0, 900, 44, TRUE);
```

## Parent Message Handling

```cpp
case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case 8101:
        return 0;
    case 8102:
        return 0;
    case 8103:
        return 0;
    }
    break;
```

## Common API

### `SetItems` / `AddItem` / `ClearItems`

```cpp
toolbar.SetItems(items);
toolbar.AddItem({L"Export", 8104});
toolbar.ClearItems();
```

### `SetItem`

```cpp
toolbar.SetItem(1, {L"Open", 8102, iconOpen});
```

### `SetChecked` / `SetDisabled`

```cpp
toolbar.SetChecked(0, true);
toolbar.SetDisabled(3, true);
```

### `SetTheme`

```cpp
toolbar.SetTheme(theme);
```

### `GetCount` / `GetItem`

```cpp
std::size_t count = toolbar.GetCount();
auto item = toolbar.GetItem(0);
```

## Theme Fields Used

- `toolbarBackground`
- `toolbarItem`
- `toolbarItemHot`
- `toolbarItemActive`
- `toolbarText`
- `toolbarTextActive`
- `toolbarSeparator`
- `toolbarHeight`
- `buttonDisabledText`
- `textPadding`
- `uiFont`

## Usage Notes

- When `iconOnly = true`, still provide `text` so overflow popups can show a readable label
- Toolbar icons default to `80%` of the button's main dimension
- `iconScalePercent` overrides that percentage per item
- Icons always preserve aspect ratio and are never stretched
- When `dropDown = true`, `popupMenu` must be a valid application-owned `HMENU`
- `alignRight = true` is useful for secondary tools or far-edge actions
- The same control can be used as a menu-bar style surface

## Drop-Down and Overflow Behavior

- Drop-down menus are rendered through a custom dark popup instead of the native light menu UI
- When space becomes insufficient, hidden items move into an overflow button
- Overflow tries to preserve drop-down hierarchy instead of flattening every submenu entry

## Demo Reference

For complete examples, see:

- `../demo/src/demo_toolbar.cpp`
- `../demo/src/demo_toolbar_menubar.cpp`
- `../demo/src/demo_showcase.cpp`

## Current Limitations

- No full keyboard navigation model yet
- No drag-based toolbar customization
- No built-in icon resource management
