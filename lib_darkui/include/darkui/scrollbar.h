#pragma once

#include "darkui/combobox.h"

#include <memory>

namespace darkui {

// Custom dark scrollbar for Win32.
// Usage:
// - Create either a vertical or horizontal scrollbar with Create().
// - Position it with MoveWindow().
// - Handle WM_VSCROLL or WM_HSCROLL notifications in the parent window.
class ScrollBar {
public:
    struct Impl;

    // Constructs an empty scrollbar wrapper.
    ScrollBar();
    // Destroys the underlying scrollbar window if it still exists.
    ~ScrollBar();

    ScrollBar(const ScrollBar&) = delete;
    ScrollBar& operator=(const ScrollBar&) = delete;

    // Creates the scrollbar as a child window.
    // Parameters:
    // - parent: Parent window that receives scroll notifications.
    // - controlId: Child control ID used by the parent for identification.
    // - vertical: true for a vertical scrollbar, false for a horizontal scrollbar.
    // - theme: Visual theme used for drawing.
    // - style: Standard child-window style flags.
    // - exStyle: Optional extended window style.
    // Returns:
    // - true on success.
    // - false if the window or drawing resources could not be created.
    // Notes:
    // - Position and size are controlled by your layout code.
    bool Create(HWND parent, int controlId, bool vertical, const Theme& theme = Theme{}, DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP, DWORD exStyle = 0);
    // Destroys the scrollbar window and resets wrapper state.
    void Destroy();

    // Returns the underlying HWND, or nullptr before Create().
    HWND hwnd() const { return scrollBarHwnd_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the theme currently stored by the control.
    const Theme& theme() const { return theme_; }
    // Returns true for a vertical scrollbar, false for a horizontal scrollbar.
    bool vertical() const { return vertical_; }

    // Replaces the current theme and repaints the control.
    // Parameter:
    // - theme: New theme data to apply.
    void SetTheme(const Theme& theme);
    // Sets the logical scroll range.
    // Parameters:
    // - minimum: Minimum logical value.
    // - maximum: Maximum logical value.
    // Notes:
    // - If minimum is greater than maximum, the values are swapped.
    // - The current value is clamped into the reachable range.
    void SetRange(int minimum, int maximum);
    // Sets the logical page size used for thumb length and page jumps.
    // Parameter:
    // - pageSize: Page size in logical units. Negative values are clamped to zero.
    // Notes:
    // - A larger page size produces a larger thumb.
    void SetPageSize(int pageSize);
    // Sets the current scroll position.
    // Parameters:
    // - value: New logical scroll value.
    // - notify: When true, sends WM_VSCROLL or WM_HSCROLL / SB_THUMBPOSITION to the parent.
    // Notes:
    // - The value is clamped into the reachable range determined by range and page size.
    void SetValue(int value, bool notify = false);

    // Returns the current logical value.
    int GetValue() const { return value_; }
    // Returns the current minimum logical value.
    int GetMinimum() const { return minimum_; }
    // Returns the current maximum logical value.
    int GetMaximum() const { return maximum_; }
    // Returns the current logical page size.
    int GetPageSize() const { return pageSize_; }

private:
    // Internal implementation object.
    std::unique_ptr<Impl> impl_;
    // Parent window handle.
    HWND parentHwnd_ = nullptr;
    // Underlying scrollbar window handle.
    HWND scrollBarHwnd_ = nullptr;
    // Child control ID.
    int controlId_ = 0;
    // Minimum logical value.
    int minimum_ = 0;
    // Maximum logical value.
    int maximum_ = 100;
    // Logical page size used for thumb size and page jump behavior.
    int pageSize_ = 10;
    // Current logical scroll value.
    int value_ = 0;
    // Orientation flag. true = vertical, false = horizontal.
    bool vertical_ = true;
    // Theme currently used by the control.
    Theme theme_{};
};

}  // namespace darkui
