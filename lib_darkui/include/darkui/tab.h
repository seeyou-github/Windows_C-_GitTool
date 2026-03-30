#pragma once

#include "darkui/combobox.h"

#include <memory>

namespace darkui {

// Single tab item stored by darkui::Tab.
struct TabItem {
    // Tab caption shown in the strip.
    std::wstring text;
    // Optional application-owned payload value.
    std::uintptr_t userData = 0;
};

// Custom dark tab control for Win32.
// Usage:
// - Create the control with Create().
// - Fill the tab list with SetItems() or AddItem().
// - Optionally attach child page HWNDs with AttachPage().
// - Handle TCN_SELCHANGE in the parent window if you need notification.
class Tab {
public:
    struct Impl;

    // Constructs an empty tab wrapper.
    Tab();
    // Destroys the underlying tab window if it still exists.
    ~Tab();

    Tab(const Tab&) = delete;
    Tab& operator=(const Tab&) = delete;

    // Creates the tab control as a child window.
    // Parameters:
    // - parent: Parent window that receives WM_NOTIFY / TCN_SELCHANGE notifications.
    // - controlId: Child control ID used in NMHDR.idFrom.
    // - theme: Visual theme used for drawing.
    // - style: Standard child-window style flags.
    // - exStyle: Optional extended window style.
    // Returns:
    // - true on success.
    // - false if the window or resources could not be created.
    bool Create(HWND parent, int controlId, const Theme& theme = Theme{}, DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP, DWORD exStyle = 0);
    // Destroys the tab window and resets wrapper state.
    void Destroy();

    // Returns the underlying HWND, or nullptr before Create().
    HWND hwnd() const { return tabHwnd_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the theme currently stored by the control.
    const Theme& theme() const { return theme_; }
    // Returns whether the tab strip is currently vertical.
    bool vertical() const { return vertical_ ; }

    // Replaces the current theme and repaints the control.
    void SetTheme(const Theme& theme);
    // Switches between horizontal and vertical tab-strip layout.
    // Parameter:
    // - enabled: true for a vertical left-side strip, false for a horizontal top strip.
    void SetVertical(bool enabled);
    // Replaces all tab items at once.
    // Notes:
    // - Existing attached pages remain stored by index where possible.
    void SetItems(const std::vector<TabItem>& items);
    // Appends a single tab item.
    void AddItem(const TabItem& item);
    // Removes all tab items and detaches all pages.
    void ClearItems();
    // Attaches a child page window to a tab index.
    // Parameters:
    // - index: Zero-based tab index.
    // - page: Child page HWND to show when that tab is selected.
    // Notes:
    // - The page HWND must be owned and laid out by the caller.
    void AttachPage(int index, HWND page);
    // Changes the current selection.
    // Parameters:
    // - index: Zero-based tab index.
    // - notify: When true, sends TCN_SELCHANGE to the parent window.
    void SetSelection(int index, bool notify = false);
    // Returns the current zero-based selection index, or -1 when empty.
    int GetSelection() const;
    // Returns the number of tab items.
    std::size_t GetCount() const;
    // Returns a copy of the tab item at the specified index.
    TabItem GetItem(int index) const;
    // Returns the recommended page/content area below the tab strip, in client coordinates.
    RECT GetContentRect() const;

private:
    // Internal implementation object.
    std::unique_ptr<Impl> impl_;
    // Parent window handle.
    HWND parentHwnd_ = nullptr;
    // Underlying tab window handle.
    HWND tabHwnd_ = nullptr;
    // Child control ID.
    int controlId_ = 0;
    // Whether the tab strip is vertical.
    bool vertical_ = false;
    // Theme currently used by the control.
    Theme theme_{};
};

}  // namespace darkui
