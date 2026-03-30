# Slider

## Overview

`darkui::Slider` is a custom dark slider control for range-based input. It currently supports horizontal layout and reports value changes through the standard `WM_HSCROLL` path.

## Files

- `include/darkui/slider.h`
- `src/slider.cpp`

## Suitable Scenarios

- Parameter tuning
- Volume, brightness, exposure, or zoom controls
- Dark settings panels

## Main Features

- Horizontal slider
- Custom track, fill, thumb, and tick styling
- Optional tick marks
- Mouse drag and keyboard control

## Basic Usage

```cpp
#include "darkui/slider.h"

darkui::Theme theme;
theme.sliderBackground = RGB(28, 31, 36);
theme.sliderTrack = RGB(52, 56, 62);
theme.sliderFill = RGB(78, 120, 184);
theme.sliderThumb = RGB(224, 227, 232);
theme.sliderThumbHot = RGB(245, 247, 250);
theme.sliderTick = RGB(102, 110, 122);
theme.sliderTrackHeight = 6;
theme.sliderThumbRadius = 10;

darkui::Slider slider;
slider.Create(hwnd, 4001, theme);
slider.SetRange(0, 100);
slider.SetValue(38);
slider.SetShowTicks(true);
slider.SetTickCount(11);
MoveWindow(slider.hwnd(), 20, 20, 320, 80, TRUE);
```

## Parent Message Handling

```cpp
case WM_HSCROLL:
    if (reinterpret_cast<HWND>(lParam) == slider.hwnd()) {
        int value = slider.GetValue();
        return 0;
    }
    break;
```

## Common API

### `SetRange`

```cpp
slider.SetRange(0, 100);
```

### `SetValue`

```cpp
slider.SetValue(50);
slider.SetValue(60, true);
```

### `SetShowTicks` / `SetTickCount`

```cpp
slider.SetShowTicks(true);
slider.SetTickCount(11);
```

### `SetTheme`

```cpp
slider.SetTheme(theme);
```

## Reading State

```cpp
int value = slider.GetValue();
int minValue = slider.GetMinimum();
int maxValue = slider.GetMaximum();
bool showTicks = slider.show_ticks();
int tickCount = slider.tick_count();
```

## Theme Fields Used

- `sliderBackground`
- `sliderTrack`
- `sliderFill`
- `sliderThumb`
- `sliderThumbHot`
- `sliderTick`
- `sliderTrackHeight`
- `sliderThumbRadius`

## Usage Notes

- Suitable for continuous numeric values
- Use `WM_HSCROLL` to keep application state synchronized
- Fine-grained stepping can be implemented in application code

## Demo Reference

For complete examples, see:

- `../demo/src/demo_slider.cpp`
- `../demo/src/demo_showcase.cpp`
- `../demo/src/demo_toolbar.cpp`
- `../demo/src/demo_toolbar_menubar.cpp`

## Current Limitations

- Horizontal mode only
- No value bubble
- No dedicated disabled-state visuals
