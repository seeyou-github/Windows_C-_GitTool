#pragma once

#include "darkui/combobox.h"

#include <memory>
#include <string>

namespace darkui {

// Custom dark edit control built from a lightweight host window plus an inner borderless EDIT.
// Usage:
// - Create the control with Create().
// - Position the host window with MoveWindow().
// - Handle WM_COMMAND notifications such as EN_CHANGE in the parent window.
// Notes:
// - The host paints the dark background so no native border line is visible.
// - The inner EDIT keeps native text input, caret, selection, and IME behavior.
// - Placeholder text is rendered by darkui itself so color and visibility remain
//   controllable even when native cue-banner support is unavailable.
class Edit {
public:
    struct Impl;

    // Constructs an empty edit wrapper.
    Edit();
    // Destroys the host and inner edit windows if they still exist.
    ~Edit();

    Edit(const Edit&) = delete;
    Edit& operator=(const Edit&) = delete;

    // Creates the dark edit control as a child window.
    // Parameters:
    // - parent: Parent window that receives forwarded EN_* notifications through WM_COMMAND.
    // - controlId: Child control ID used in WM_COMMAND.
    // - text: Initial edit text.
    // - theme: Visual theme used for background, text, and font.
    // - style: Standard EDIT style flags. Border styles are stripped internally.
    // - exStyle: Optional extended style for the host window. Native client-edge styles are stripped.
    // Returns:
    // - true on success.
    // - false if the host/edit window or theme resources could not be created.
    bool Create(HWND parent, int controlId, const std::wstring& text = L"", const Theme& theme = Theme{}, DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, DWORD exStyle = 0);
    // Destroys the control window hierarchy and resets wrapper state.
    void Destroy();

    // Returns the dark host HWND.
    HWND hwnd() const { return hostHwnd_; }
    // Returns the inner native EDIT HWND.
    HWND edit_hwnd() const { return editHwnd_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the theme currently stored by the control.
    const Theme& theme() const { return theme_; }
    // Returns the current corner radius in pixels.
    int corner_radius() const { return cornerRadius_; }

    // Replaces the current theme and repaints the host/edit pair.
    // Parameters:
    // - theme: New colors and font used by the host surface, inner edit, and placeholder text.
    // Notes:
    // - If theme resource creation fails, the previous theme remains active.
    void SetTheme(const Theme& theme);
    // Updates the visible text.
    void SetText(const std::wstring& text);
    // Returns the current text value.
    std::wstring GetText() const;
    // Sets the placeholder text shown while the control is empty and unfocused.
    // Notes:
    // - The placeholder is drawn by darkui itself so color and visibility stay
    //   consistent even on systems without EM_SETCUEBANNER support.
    void SetCueBanner(const std::wstring& text);
    // Sets the outer corner radius of the dark host surface.
    // Notes:
    // - Values below zero are clamped to zero.
    // - This updates both painting and the host window region.
    void SetCornerRadius(int radius);
    // Toggles native read-only mode.
    // Parameters:
    // - readOnly: When true, the inner EDIT becomes non-editable but keeps the same dark colors.
    void SetReadOnly(bool readOnly);
    // Returns the latest internal layout debug string for this control.
    std::wstring DebugLayoutInfo() const;

private:
    std::unique_ptr<Impl> impl_;
    HWND parentHwnd_ = nullptr;
    HWND hostHwnd_ = nullptr;
    HWND editHwnd_ = nullptr;
    int controlId_ = 0;
    int cornerRadius_ = 10;
    Theme theme_{};
};

}  // namespace darkui
