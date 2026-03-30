#pragma once

#include <windows.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace darkui {

// Describes a font used by darkui controls.
// Notes:
// - `height` follows normal Win32 LOGFONT semantics. A negative value means
//   character height in logical units.
// - Set `monospace` to true when you want a fixed-width family fallback.
struct FontSpec {
    // Preferred font family name, for example "Segoe UI".
    std::wstring family = L"Segoe UI";
    // Font height in logical units. Negative values request character height.
    int height = -20;
    // Font weight such as FW_NORMAL or FW_BOLD.
    int weight = FW_NORMAL;
    // Enables italic style when true.
    bool italic = false;
    // Requests a monospace family preference when true.
    bool monospace = false;
};

// Shared visual theme for all darkui controls.
// Usage:
// - Create one Theme instance, customize the fields you care about, then pass
//   it into each control's Create/SetTheme call.
// Notes:
// - Not every control uses every field.
// - Unused fields can stay at their defaults.
struct Theme {
    // Generic window or page background color.
    COLORREF background = RGB(34, 36, 40);
    // Generic panel surface color.
    COLORREF panel = RGB(44, 47, 52);
    // Button normal background color.
    COLORREF button = RGB(65, 72, 82);
    // Button hover background color.
    COLORREF buttonHover = RGB(72, 80, 92);
    // Button pressed or active background color.
    COLORREF buttonHot = RGB(78, 86, 98);
    // Button disabled background color.
    COLORREF buttonDisabled = RGB(50, 54, 60);
    // Button disabled text color.
    COLORREF buttonDisabledText = RGB(130, 136, 144);
    // Generic border or outline color.
    COLORREF border = RGB(61, 66, 74);
    // Primary text color.
    COLORREF text = RGB(224, 227, 232);
    // Secondary or de-emphasized text color.
    COLORREF mutedText = RGB(156, 164, 174);
    // Combo-box arrow color.
    COLORREF arrow = RGB(156, 164, 174);
    // Combo-box popup item background color.
    COLORREF popupItem = RGB(44, 47, 52);
    // Combo-box popup item hover color.
    COLORREF popupItemHot = RGB(65, 72, 82);
    // Combo-box accented item background color.
    COLORREF popupAccentItem = RGB(27, 47, 83);
    // Combo-box accented item hover color.
    COLORREF popupAccentItemHot = RGB(39, 66, 116);
    // Edit control background color.
    COLORREF editBackground = RGB(44, 47, 52);
    // Edit control text color.
    COLORREF editText = RGB(224, 227, 232);
    // Edit control placeholder text color.
    COLORREF editPlaceholder = RGB(142, 149, 160);
    // Table body background color.
    COLORREF tableBackground = RGB(34, 36, 40);
    // Table body text color.
    COLORREF tableText = RGB(224, 227, 232);
    // Table header background color.
    COLORREF tableHeaderBackground = RGB(44, 47, 52);
    // Table header text color.
    COLORREF tableHeaderText = RGB(224, 227, 232);
    // Table grid and separator color.
    COLORREF tableGrid = RGB(61, 66, 74);
    // Slider outer background color.
    COLORREF sliderBackground = RGB(34, 36, 40);
    // Slider track background color.
    COLORREF sliderTrack = RGB(52, 56, 62);
    // Slider filled progress color.
    COLORREF sliderFill = RGB(78, 120, 184);
    // Slider thumb normal color.
    COLORREF sliderThumb = RGB(224, 227, 232);
    // Slider thumb hover or drag color.
    COLORREF sliderThumbHot = RGB(245, 247, 250);
    // Slider tick mark color.
    COLORREF sliderTick = RGB(92, 100, 112);
    // Progress bar outer background color.
    COLORREF progressBackground = RGB(34, 36, 40);
    // Progress bar track color.
    COLORREF progressTrack = RGB(52, 56, 62);
    // Progress bar fill color.
    COLORREF progressFill = RGB(78, 120, 184);
    // Progress bar centered text color.
    COLORREF progressText = RGB(232, 236, 241);
    // Scrollbar outer background color.
    COLORREF scrollBarBackground = RGB(34, 36, 40);
    // Scrollbar track color.
    COLORREF scrollBarTrack = RGB(52, 56, 62);
    // Scrollbar thumb normal color.
    COLORREF scrollBarThumb = RGB(120, 128, 140);
    // Scrollbar thumb hover and drag color.
    COLORREF scrollBarThumbHot = RGB(150, 160, 174);
    // Tab control background color.
    COLORREF tabBackground = RGB(34, 36, 40);
    // Inactive tab item background color.
    COLORREF tabItem = RGB(52, 56, 62);
    // Active tab item background color.
    COLORREF tabItemActive = RGB(78, 86, 98);
    // Inactive tab text color.
    COLORREF tabText = RGB(210, 214, 220);
    // Active tab text color.
    COLORREF tabTextActive = RGB(245, 247, 250);
    // Toolbar background color.
    COLORREF toolbarBackground = RGB(34, 36, 40);
    // Toolbar item normal background color.
    COLORREF toolbarItem = RGB(52, 56, 62);
    // Toolbar item hover background color.
    COLORREF toolbarItemHot = RGB(72, 80, 92);
    // Toolbar item checked or pressed background color.
    COLORREF toolbarItemActive = RGB(78, 120, 184);
    // Toolbar item normal text color.
    COLORREF toolbarText = RGB(224, 227, 232);
    // Toolbar item active text color.
    COLORREF toolbarTextActive = RGB(245, 247, 250);
    // Toolbar separator color.
    COLORREF toolbarSeparator = RGB(61, 66, 74);
    // Font shared by controls that render text.
    FontSpec uiFont{};
    // Default combo-box item height.
    int itemHeight = 24;
    // Width reserved for the combo-box arrow area.
    int arrowWidth = 12;
    // Right padding inside the combo-box arrow area.
    int arrowRightPadding = 10;
    // Generic horizontal text padding used by several controls.
    int textPadding = 8;
    // Combo-box popup border thickness.
    int popupBorder = 1;
    // Vertical popup offset relative to the combo-box button.
    int popupOffsetY = 2;
    // Table row height.
    int tableRowHeight = 28;
    // Table header height.
    int tableHeaderHeight = 30;
    // Slider track thickness.
    int sliderTrackHeight = 6;
    // Slider thumb radius.
    int sliderThumbRadius = 9;
    // Progress bar track height.
    int progressHeight = 12;
    // Recommended scrollbar thickness for demo or layout code.
    int scrollBarThickness = 14;
    // Minimum scrollbar thumb length.
    int scrollBarMinThumbSize = 28;
    // Tab strip height in horizontal mode, and item height in vertical mode.
    int tabHeight = 36;
    // Tab strip width in vertical mode.
    int tabWidth = 180;
    // Toolbar height.
    int toolbarHeight = 40;
};

// Single item stored by darkui::ComboBox.
struct ComboItem {
    // Text shown to the user.
    std::wstring text;
    // Optional application-owned payload value.
    std::uintptr_t userData = 0;
    // Marks the item as visually accented when true.
    bool accent = false;
};

// Creates an HFONT from a FontSpec description.
// Parameters:
// - spec: Font description used to populate a Win32 LOGFONT.
// Returns:
// - A newly created HFONT on success.
// - nullptr on failure.
// Notes:
// - The returned font handle must eventually be deleted with DeleteObject by
//   the owner that stores it.
HFONT CreateFont(const FontSpec& spec);

// Custom dark combo box built from a button plus popup list.
// Usage:
// - Create the control with Create().
// - Fill items with SetItems/AddItem().
// - Move the main button with MoveWindow().
// - Listen for CBN_SELCHANGE in the parent window.
class ComboBox {
public:
    // Constructs an empty combo-box wrapper.
    ComboBox();
    // Destroys the main control and popup windows if they still exist.
    ~ComboBox();

    ComboBox(const ComboBox&) = delete;
    ComboBox& operator=(const ComboBox&) = delete;

    // Creates the combo-box button and its popup infrastructure.
    // Parameters:
    // - parent: Parent window that receives CBN_SELCHANGE notifications.
    // - controlId: Child control ID used in WM_COMMAND.
    // - theme: Visual theme used for drawing.
    // - style: Standard child style flags. Owner-draw behavior is handled internally.
    // - exStyle: Optional extended window style for the main button.
    // Returns:
    // - true if all windows and theme resources were created successfully.
    // - false on failure.
    // Notes:
    // - Position and size are still controlled by your layout code.
    bool Create(HWND parent, int controlId, const Theme& theme = Theme{}, DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, DWORD exStyle = 0);
    // Destroys the button, popup host, and popup list windows.
    void Destroy();

    // Returns the main combo-box button HWND.
    HWND hwnd() const { return comboHwnd_; }
    // Returns the popup list HWND used for the drop-down items.
    HWND popup_list() const { return popupList_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the theme currently stored by the control.
    const Theme& theme() const { return theme_; }

    // Replaces the current theme and repaints the combo box and popup.
    // Parameter:
    // - theme: New theme data to apply.
    void SetTheme(const Theme& theme);
    // Replaces all combo-box items at once.
    // Parameter:
    // - items: Full item list to display.
    // Notes:
    // - Existing items are discarded.
    // - Selection is reset according to the control implementation.
    void SetItems(const std::vector<ComboItem>& items);
    // Appends a single item to the end of the list.
    // Parameter:
    // - item: Item to append.
    void AddItem(const ComboItem& item);
    // Removes all items from the list and clears selection state.
    void ClearItems();
    // Returns the current zero-based selection index.
    // Returns:
    // - The selected index, or a negative value when nothing is selected.
    int GetSelection() const;
    // Changes the current selection.
    // Parameters:
    // - index: Zero-based item index to select.
    // - notify: When true, sends CBN_SELCHANGE to the parent window.
    // Notes:
    // - Out-of-range values are handled by the control implementation.
    void SetSelection(int index, bool notify = false);
    // Returns the number of items currently stored in the control.
    std::size_t GetCount() const;
    // Returns the currently selected text, or an empty string if no item is selected.
    std::wstring GetText() const;
    // Returns a copy of the item at the specified index.
    // Parameter:
    // - index: Zero-based item index.
    // Notes:
    // - Callers should pass a valid index obtained from GetSelection or GetCount.
    ComboItem GetItem(int index) const;

private:
    struct Impl;
    // Internal implementation object.
    std::unique_ptr<Impl> impl_;
    // Parent window handle.
    HWND parentHwnd_ = nullptr;
    // Main combo-box button handle.
    HWND comboHwnd_ = nullptr;
    // Popup host window handle.
    HWND popupHost_ = nullptr;
    // Popup list window handle.
    HWND popupList_ = nullptr;
    // Child control ID.
    int controlId_ = 0;
    // Theme currently used by the control.
    Theme theme_{};
};

}  // namespace darkui
