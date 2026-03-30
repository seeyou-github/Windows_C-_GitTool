# Table

## Overview

`darkui::Table` is a custom dark table control intended for data presentation. It draws its own header, body, grid lines, and selected-row highlight so it stays visually consistent in dark interfaces.

## Files

- `include/darkui/table.h`
- `src/table.cpp`

## Suitable Scenarios

- Settings lists
- Data overview panels
- Dark admin-style display tables

## Main Features

- Custom header background and text
- Custom body background, text, and grid colors
- Column and row data management
- Selected-row highlight
- Optional grid drawing below the last row

## Core Types

```cpp
struct TableColumn {
    std::wstring text;
    int width = 120;
    int format = LVCFMT_LEFT;
};

using TableRow = std::vector<std::wstring>;
```

## Basic Usage

```cpp
#include "darkui/table.h"

darkui::Theme theme;
theme.tableBackground = RGB(25, 28, 33);
theme.tableText = RGB(228, 232, 238);
theme.tableHeaderBackground = RGB(38, 42, 48);
theme.tableHeaderText = RGB(245, 247, 250);
theme.tableGrid = RGB(69, 77, 89);
theme.tableRowHeight = 30;
theme.tableHeaderHeight = 34;

darkui::Table table;
table.Create(hwnd, 2001, theme);
table.SetColumns({
    {L"Name", 180, LVCFMT_LEFT},
    {L"Type", 120, LVCFMT_LEFT},
    {L"State", 100, LVCFMT_CENTER},
    {L"Notes", 260, LVCFMT_LEFT},
});
table.SetRows({
    {L"ComboBox", L"Control", L"Ready", L"Dark popup and button"},
    {L"Table", L"Control", L"Ready", L"Dark header, body and grid"},
});
MoveWindow(table.hwnd(), 20, 20, 640, 360, TRUE);
```

## Common API

### `SetColumns`

```cpp
table.SetColumns({
    {L"Control", 120},
    {L"Area", 120},
    {L"State", 100},
    {L"Notes", 220},
});
```

### `SetRows` / `AddRow` / `ClearRows`

```cpp
table.SetRows(rows);
table.AddRow({L"Theme", L"Config", L"Live", L"Updated at runtime"});
table.ClearRows();
```

### `SetDrawEmptyGrid`

```cpp
table.SetDrawEmptyGrid(false);
```

### `SetTheme`

```cpp
table.SetTheme(theme);
```

## Reading State

```cpp
std::size_t rowCount = table.GetRowCount();
std::size_t columnCount = table.GetColumnCount();
bool drawEmptyGrid = table.draw_empty_grid();
```

## Theme Fields Used

- `tableBackground`
- `tableText`
- `tableHeaderBackground`
- `tableHeaderText`
- `tableGrid`
- `tableRowHeight`
- `tableHeaderHeight`
- `textPadding`
- `buttonHot`

## Usage Notes

- Best suited for presentation-oriented tables rather than highly interactive grids
- Sorting, paging, and editing are expected to be implemented by application code if needed

## Demo Reference

For complete examples, see:

- `../demo/src/demo_table.cpp`
- `../demo/src/demo_showcase.cpp`

## Current Limitations

- No built-in scroll container
- No column drag or sorting support
- No in-place cell editing
- No multi-select support
