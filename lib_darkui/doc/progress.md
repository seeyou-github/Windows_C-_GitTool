# ProgressBar

## Overview

`darkui::ProgressBar` is a custom dark progress bar. It separates host background, outer shell, inner track, fill, and percentage text so the control fits naturally into dark cards and panels.

## Files

- `include/darkui/progress.h`
- `src/progress.cpp`

## Suitable Scenarios

- Download progress
- Task execution status
- Stats bars inside dark cards or dashboards

## Main Features

- Configurable logical range
- Configurable current value
- Optional percentage text
- Host surface color support

## Basic Usage

```cpp
#include "darkui/progress.h"

darkui::Theme theme;
theme.progressBackground = RGB(49, 54, 61);
theme.progressTrack = RGB(36, 40, 46);
theme.progressFill = RGB(78, 120, 184);
theme.progressText = RGB(240, 244, 248);
theme.progressHeight = 18;

darkui::ProgressBar progress;
progress.Create(hwnd, 5001, theme);
progress.SetSurfaceColor(theme.panel);
progress.SetRange(0, 100);
progress.SetValue(64);
MoveWindow(progress.hwnd(), 20, 20, 300, 40, TRUE);
```

## Common API

### `SetRange`

```cpp
progress.SetRange(0, 100);
```

### `SetValue`

```cpp
progress.SetValue(72);
```

### `SetShowPercentage`

```cpp
progress.SetShowPercentage(true);
```

### `SetSurfaceColor`

```cpp
progress.SetSurfaceColor(theme.panel);
```

### `SetTheme`

```cpp
progress.SetTheme(theme);
```

## Reading State

```cpp
int value = progress.GetValue();
int minValue = progress.GetMinimum();
int maxValue = progress.GetMaximum();
bool showText = progress.show_percentage();
```

## Theme Fields Used

- `progressBackground`
- `progressTrack`
- `progressFill`
- `progressText`
- `progressHeight`
- `uiFont`
- `background`

## Usage Notes

- When the control sits on a custom card, call `SetSurfaceColor(cardColor)`
- Changing the logical range automatically clamps the current value

## Demo Reference

For complete examples, see:

- `../demo/src/demo_progress.cpp`
- `../demo/src/demo_showcase.cpp`

## Current Limitations

- Horizontal mode only
- No indeterminate animation
- No gradient fill
- No parent notification flow
