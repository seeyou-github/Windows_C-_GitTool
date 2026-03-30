#pragma once

#include "darkui/combobox.h"

#include <memory>

namespace darkui {

// Custom dark progress bar for Win32.
// Usage:
// - Create the control with Create().
// - Position it with MoveWindow().
// - Update its state with SetRange() and SetValue().
class ProgressBar {
public:
    struct Impl;

    // Constructs an empty progress-bar wrapper.
    ProgressBar();
    // Destroys the underlying progress-bar window if it still exists.
    ~ProgressBar();

    ProgressBar(const ProgressBar&) = delete;
    ProgressBar& operator=(const ProgressBar&) = delete;

    // Creates the progress bar as a child window.
    // Parameters:
    // - parent: Parent window that owns the control.
    // - controlId: Child control ID used for identification.
    // - theme: Visual theme used for drawing.
    // - style: Standard child-window style flags.
    // - exStyle: Optional extended window style.
    // Returns:
    // - true on success.
    // - false if the window or drawing resources could not be created.
    // Notes:
    // - Position and size are controlled by your layout code.
    bool Create(HWND parent, int controlId, const Theme& theme = Theme{}, DWORD style = WS_CHILD | WS_VISIBLE, DWORD exStyle = 0);
    // Destroys the progress-bar window and resets wrapper state.
    void Destroy();

    // Returns the underlying HWND, or nullptr before Create().
    HWND hwnd() const { return progressHwnd_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the theme currently stored by the control.
    const Theme& theme() const { return theme_; }
    // Returns the host surface color used outside the inner progress track.
    COLORREF surface_color() const { return surfaceColor_; }

    // Replaces the current theme and repaints the control.
    // Parameter:
    // - theme: New theme data to apply.
    void SetTheme(const Theme& theme);
    // Sets the logical progress range.
    // Parameters:
    // - minimum: Minimum logical value.
    // - maximum: Maximum logical value.
    // Notes:
    // - If minimum is greater than maximum, the values are swapped.
    // - The current value is clamped into the new range.
    void SetRange(int minimum, int maximum);
    // Sets the current progress value.
    // Parameter:
    // - value: New logical value to display.
    // Notes:
    // - The value is clamped into the current range.
    void SetValue(int value);
    // Shows or hides the centered percentage text.
    // Parameter:
    // - enabled: true to show the percentage text, false to hide it.
    void SetShowPercentage(bool enabled);
    // Sets the host surface color behind the progress track.
    // Purpose:
    // - Fills the full control rectangle before the inner progress track is painted.
    // - Prevents visible seams when the progress bar is placed on a card/panel whose color differs from the main window background.
    // Parameters:
    // - color: Host/background color expected around the inner track.
    // Notes:
    // - This does not change the inner track color. Use Theme::progressBackground / Theme::progressTrack / Theme::progressFill for that.
    void SetSurfaceColor(COLORREF color);

    // Returns the current logical value.
    int GetValue() const { return value_; }
    // Returns the current minimum logical value.
    int GetMinimum() const { return minimum_; }
    // Returns the current maximum logical value.
    int GetMaximum() const { return maximum_; }
    // Returns whether the percentage text is currently enabled.
    bool show_percentage() const { return showPercentage_; }

private:
    // Internal implementation object.
    std::unique_ptr<Impl> impl_;
    // Parent window handle.
    HWND parentHwnd_ = nullptr;
    // Underlying progress-bar window handle.
    HWND progressHwnd_ = nullptr;
    // Child control ID.
    int controlId_ = 0;
    // Minimum logical value.
    int minimum_ = 0;
    // Maximum logical value.
    int maximum_ = 100;
    // Current logical value.
    int value_ = 0;
    // Whether centered percentage text is drawn.
    bool showPercentage_ = true;
    // Host/background color behind the inner progress track.
    COLORREF surfaceColor_ = RGB(34, 36, 40);
    // Theme currently used by the control.
    Theme theme_{};
};

}  // namespace darkui
