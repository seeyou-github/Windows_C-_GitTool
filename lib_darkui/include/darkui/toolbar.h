#pragma once

#include "darkui/combobox.h"

#include <memory>

namespace darkui {

// Single item stored by darkui::Toolbar.
// Notes:
// - A separator item ignores most other fields and only draws a vertical divider.
// - `iconOnly` hides the visible text in the toolbar itself, but `text` should still
//   be populated so overflow popups can show a readable label.
// - `dropDown` expects `popupMenu` to point to a valid application-owned `HMENU`.
struct ToolbarItem {
    // Text shown inside the toolbar item.
    std::wstring text;
    // Command/control identifier reported to the parent window on click.
    int commandId = 0;
    // Optional small icon drawn before the text.
    HICON icon = nullptr;
    // Optional popup menu shown when the item acts as a drop-down button.
    // Usage:
    // - Set `dropDown = true`.
    // - Keep ownership of the menu in application code.
    // - Toolbar reads menu items to build a custom dark popup instead of showing the
    //   native light themed menu directly.
    // - The same menu handle can also be reused for a menu-bar style toolbar item.
    HMENU popupMenu = nullptr;
    // Optional application-owned payload value.
    std::uintptr_t userData = 0;
    // Marks the item as a separator instead of a clickable button when true.
    bool separator = false;
    // Marks the item as checked/active when true.
    bool checked = false;
    // Disables pointer and keyboard activation when true.
    bool disabled = false;
    // Places the item in the right-aligned tool group when true.
    // Right-aligned items are laid out from the far edge inward and keep their own
    // spacing independent from the main left group.
    bool alignRight = false;
    // Draws the item as an icon-only square button when true.
    // `text` is still recommended for overflow popup labels and future accessibility.
    bool iconOnly = false;
    // Draws a drop-down arrow and opens `popupMenu` on click when true.
    // If width becomes insufficient, the item is preserved inside overflow as a
    // submenu entry rather than being flattened into plain commands.
    // Keyboard Enter/Space opens the custom popup as well.
    bool dropDown = false;
    // Icon size as a percentage of the button's main dimension.
    // - `0` means use the default `80`.
    // - Values are clamped to `[1, 100]`.
    // - Icons still preserve aspect ratio and are never stretched.
    int iconScalePercent = 0;
};

// Custom dark toolbar for Win32.
// Usage:
// - Create the toolbar with Create().
// - Fill the item list with SetItems() or AddItem().
// - Position it with MoveWindow().
// - Handle WM_COMMAND in the parent window for item clicks.
class Toolbar {
public:
    struct Impl;

    // Constructs an empty toolbar wrapper.
    Toolbar();
    // Destroys the underlying toolbar window if it still exists.
    ~Toolbar();

    Toolbar(const Toolbar&) = delete;
    Toolbar& operator=(const Toolbar&) = delete;

    // Creates the toolbar as a child window.
    // Parameters:
    // - parent: Parent window that receives WM_COMMAND notifications.
    // - controlId: Child control ID used for identification.
    // - theme: Visual theme used for drawing.
    // - style: Standard child-window style flags.
    // - exStyle: Optional extended window style.
    // Returns:
    // - true on success.
    // - false if the window or drawing resources could not be created.
    bool Create(HWND parent, int controlId, const Theme& theme = Theme{}, DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP, DWORD exStyle = 0);
    // Destroys the toolbar window and resets wrapper state.
    void Destroy();

    // Returns the underlying HWND, or nullptr before Create().
    HWND hwnd() const { return toolbarHwnd_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the theme currently stored by the control.
    const Theme& theme() const { return theme_; }

    // Replaces the current theme and repaints the toolbar.
    // Notes:
    // - Theme resources are rebuilt internally.
    // - If resource creation fails, the previous theme remains active.
    void SetTheme(const Theme& theme);
    // Replaces all toolbar items at once.
    // Notes:
    // - Existing popup UI is closed before the new items become active.
    // - Use this when rebuilding the entire toolbar model.
    void SetItems(const std::vector<ToolbarItem>& items);
    // Appends a single toolbar item.
    // Useful for incremental setup during initialization.
    void AddItem(const ToolbarItem& item);
    // Removes all toolbar items.
    // Notes:
    // - Any visible drop-down or overflow popup is closed first.
    // - Useful when rebuilding a menu-bar style toolbar from scratch.
    void ClearItems();
    // Replaces one item in place.
    // Parameters:
    // - index: Zero-based item index to overwrite.
    // - item: Replacement toolbar item definition.
    void SetItem(int index, const ToolbarItem& item);
    // Updates the checked state of one item.
    // This is typically used for mode toggles or active-tool highlighting.
    void SetChecked(int index, bool checked);
    // Updates the disabled state of one item.
    // Disabled items stay visible but are skipped by pointer hit-testing.
    void SetDisabled(int index, bool disabled);
    // Returns the number of toolbar items currently stored.
    std::size_t GetCount() const;
    // Returns a copy of the item at the specified index.
    ToolbarItem GetItem(int index) const;

private:
    // Internal implementation object.
    std::unique_ptr<Impl> impl_;
    // Parent window handle.
    HWND parentHwnd_ = nullptr;
    // Underlying toolbar window handle.
    HWND toolbarHwnd_ = nullptr;
    // Popup host window used by drop-down and overflow menus.
    HWND popupHost_ = nullptr;
    // Popup list window used by drop-down and overflow menus.
    HWND popupList_ = nullptr;
    // Child control ID.
    int controlId_ = 0;
    // Theme currently used by the control.
    Theme theme_{};
};

}  // namespace darkui
