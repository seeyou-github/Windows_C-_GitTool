#pragma once

#include "darkui/combobox.h"

#include <memory>

namespace darkui {

// Custom horizontal slider control for dark Win32 UIs.
// Usage:
// - Create the control with Create().
// - Position it with MoveWindow().
// - Handle value changes through WM_HSCROLL in the parent window.
class Slider {
public:
    struct Impl;

    // Constructs an empty slider wrapper.
    Slider();
    // Destroys the underlying slider window if it still exists.
    ~Slider();

    Slider(const Slider&) = delete;
    Slider& operator=(const Slider&) = delete;

    // Creates the slider as a child window.
    // Parameters:
    // - parent: Parent window that receives WM_HSCROLL notifications.
    // - controlId: Child control ID used by the parent for identification.
    // - theme: Visual theme used for drawing.
    // - style: Standard child-window style flags.
    // - exStyle: Optional extended window style.
    // Returns:
    // - true on success.
    // - false if the window or drawing resources could not be created.
    // Notes:
    // - Position and size are controlled by your own layout code.
    bool Create(HWND parent, int controlId, const Theme& theme = Theme{}, DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP, DWORD exStyle = 0);
    // Destroys the slider window and resets wrapper state.
    void Destroy();

    // Returns the underlying slider HWND, or nullptr before Create().
    HWND hwnd() const { return sliderHwnd_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the theme currently stored by the control.
    const Theme& theme() const { return theme_; }

    // Replaces the current theme and repaints the slider.
    // Parameter:
    // - theme: New theme data to apply.
    void SetTheme(const Theme& theme);
    // Sets the logical slider range.
    // Parameters:
    // - minimum: Minimum logical value.
    // - maximum: Maximum logical value.
    // Notes:
    // - If minimum is greater than maximum, the values are swapped.
    // - The current value is clamped into the new range.
    void SetRange(int minimum, int maximum);
    // Sets the current slider value.
    // Parameters:
    // - value: New logical value.
    // - notify: When true, sends WM_HSCROLL / SB_THUMBPOSITION to the parent.
    // Notes:
    // - The value is clamped into the current range.
    void SetValue(int value, bool notify = false);
    // Enables or disables tick-mark drawing.
    // Parameter:
    // - enabled: true to show tick marks, false to hide them.
    void SetShowTicks(bool enabled);
    // Sets how many tick marks are drawn.
    // Parameter:
    // - count: Total number of ticks. Values below zero are clamped to zero.
    // Notes:
    // - Tick marks are only visible when SetShowTicks(true) is enabled.
    void SetTickCount(int count);
    // Returns the current logical value.
    int GetValue() const { return value_; }
    // Returns the current minimum logical value.
    int GetMinimum() const { return minimum_; }
    // Returns the current maximum logical value.
    int GetMaximum() const { return maximum_; }
    // Returns whether tick marks are currently enabled.
    bool show_ticks() const { return showTicks_; }
    // Returns the configured tick-count value.
    int tick_count() const { return tickCount_; }

private:
    // Internal implementation object.
    std::unique_ptr<Impl> impl_;
    // Parent window handle.
    HWND parentHwnd_ = nullptr;
    // Underlying slider window handle.
    HWND sliderHwnd_ = nullptr;
    // Child control ID.
    int controlId_ = 0;
    // Minimum logical value.
    int minimum_ = 0;
    // Maximum logical value.
    int maximum_ = 100;
    // Current logical value.
    int value_ = 0;
    // Whether tick marks should be rendered.
    bool showTicks_ = false;
    // Number of tick marks to render.
    int tickCount_ = 0;
    // Theme currently used by the control.
    Theme theme_{};
};

}  // namespace darkui
