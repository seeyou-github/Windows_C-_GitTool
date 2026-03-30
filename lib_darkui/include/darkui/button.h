#pragma once

#include "darkui/combobox.h"

#include <memory>
#include <string>

namespace darkui {

// Owner-drawn dark button with standard Win32 BN_CLICKED behavior.
// Usage:
// - Create the button with Create().
// - Position it with MoveWindow().
// - Handle clicks through WM_COMMAND / BN_CLICKED in the parent window.
class Button {
public:
    // Constructs an empty button wrapper.
    Button();
    // Destroys the underlying window if it still exists.
    ~Button();

    Button(const Button&) = delete;
    Button& operator=(const Button&) = delete;

    // Creates the button as a child window.
    // Parameters:
    // - parent: Parent window that will receive BN_CLICKED notifications.
    // - controlId: Child control ID used in WM_COMMAND.
    // - text: Initial button caption.
    // - theme: Visual theme used for drawing.
    // - style: Standard child-button style flags. BS_OWNERDRAW is added internally.
    // - exStyle: Optional extended window style.
    // Returns:
    // - true if the window and theme resources were created successfully.
    // - false on failure.
    // Notes:
    // - After creation you still need to set size and position with MoveWindow.
    bool Create(HWND parent, int controlId, const std::wstring& text, const Theme& theme = Theme{}, DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, DWORD exStyle = 0);
    // Destroys the underlying button window and resets the wrapper state.
    void Destroy();

    // Returns the underlying HWND, or nullptr if the button has not been created.
    HWND hwnd() const { return buttonHwnd_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the theme currently stored by the control.
    const Theme& theme() const { return theme_; }
    // Returns the current corner radius in pixels.
    int corner_radius() const { return cornerRadius_; }
    // Returns the background color used to fill the area outside the rounded button body.
    COLORREF surface_color() const { return surfaceColor_; }

    // Replaces the current theme and repaints the control.
    // Parameter:
    // - theme: New theme data to apply immediately.
    void SetTheme(const Theme& theme);
    // Updates the visible button caption.
    // Parameter:
    // - text: New caption text.
    void SetText(const std::wstring& text);
    // Reads the current button caption.
    // Returns:
    // - The current window text as a std::wstring.
    std::wstring GetText() const;
    // Sets the visual corner radius.
    // Parameter:
    // - radius: Corner radius in pixels. Values below zero are clamped to zero.
    // Notes:
    // - This changes both drawing and the button window region.
    void SetCornerRadius(int radius);
    // Sets the host surface color behind the rounded button body.
    // Purpose:
    // - Fills the rectangular area outside the rounded shape before the button body is painted.
    // - Prevents visible color seams when the button is placed on a panel/card whose color differs from `theme.background`.
    // Typical usage:
    // - Leave the default value when the button sits directly on the main window background.
    // - Set it to the surrounding card or panel color when the button is placed on a custom surface.
    // Parameters:
    // - color: Host/background color expected behind the rounded button corners.
    // Notes:
    // - This only affects the corner fallback area outside the rounded body.
    // - It does not change the main button fill color. Use SetTheme() for that.
    void SetSurfaceColor(COLORREF color);

private:
    struct Impl;
    // Internal implementation object.
    std::unique_ptr<Impl> impl_;
    // Parent window handle.
    HWND parentHwnd_ = nullptr;
    // Underlying Win32 button handle.
    HWND buttonHwnd_ = nullptr;
    // Child control ID.
    int controlId_ = 0;
    // Corner radius in pixels.
    int cornerRadius_ = 8;
    // Host/background color behind the rounded button corners.
    COLORREF surfaceColor_ = RGB(34, 36, 40);
    // Theme currently used by the control.
    Theme theme_{};
};

}  // namespace darkui
