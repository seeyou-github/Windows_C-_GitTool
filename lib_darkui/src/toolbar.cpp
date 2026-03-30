#include "darkui/toolbar.h"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>

namespace darkui {
namespace {

constexpr wchar_t kToolbarClassName[] = L"DarkUiToolbarControl";
constexpr wchar_t kToolbarPopupClassName[] = L"DarkUiToolbarPopupHost";
constexpr int kToolbarIconSize = 16;
constexpr int kToolbarIconScalePercent = 80;
constexpr int kToolbarArrowWidth = 14;
constexpr int kOverflowCommandId = 0x7F41;
constexpr int kToolbarPopupListId = 0x7F42;
constexpr int kToolbarButtonMinWidth = 46;
constexpr int kToolbarButtonSideInset = 14;
constexpr int kToolbarButtonGap = 6;
constexpr int kToolbarPopupMinWidth = 180;
constexpr int kToolbarPopupArrowInset = 18;
constexpr int kToolbarPopupOuterPadding = 4;
constexpr int kToolbarPopupIndicatorHeight = 14;
constexpr UINT_PTR kToolbarPopupScrollTimerId = 0x6124;
ATOM EnsureToolbarClassRegistered(HINSTANCE instance);
ATOM EnsureToolbarPopupClassRegistered(HINSTANCE instance);
LRESULT CALLBACK ToolbarWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ToolbarPopupWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

SIZE QueryIconSize(HICON icon) {
    SIZE size{kToolbarIconSize, kToolbarIconSize};
    if (!icon) {
        return size;
    }

    ICONINFO info{};
    if (!GetIconInfo(icon, &info)) {
        return size;
    }

    BITMAP bitmap{};
    if (info.hbmColor && GetObjectW(info.hbmColor, sizeof(bitmap), &bitmap) == sizeof(bitmap)) {
        size.cx = std::max(1L, bitmap.bmWidth);
        size.cy = std::max(1L, bitmap.bmHeight);
    } else if (info.hbmMask && GetObjectW(info.hbmMask, sizeof(bitmap), &bitmap) == sizeof(bitmap)) {
        size.cx = std::max(1L, bitmap.bmWidth);
        size.cy = std::max(1L, bitmap.bmHeight / 2);
    }

    if (info.hbmColor) {
        DeleteObject(info.hbmColor);
    }
    if (info.hbmMask) {
        DeleteObject(info.hbmMask);
    }
    return size;
}

int ClampIconScalePercent(int percent) {
    if (percent <= 0) {
        return kToolbarIconScalePercent;
    }
    return std::max(1, std::min(percent, 100));
}

}  // namespace

struct Toolbar::Impl {
    struct ResolvedIconLayout {
        RECT bounds{0, 0, 0, 0};
        int drawWidth = 0;
        int drawHeight = 0;
    };

    struct PopupEntry {
        std::wstring text;
        int commandId = 0;
        int sourceIndex = -1;
        HMENU popupMenu = nullptr;
        bool separator = false;
        bool checked = false;
        bool disabled = false;
        bool opensSubmenu = false;
    };

    Toolbar* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushBackground = nullptr;
    HBRUSH brushItem = nullptr;
    HBRUSH brushItemHot = nullptr;
    HBRUSH brushItemActive = nullptr;
    HBRUSH brushPopupPanel = nullptr;
    HBRUSH brushPopupItem = nullptr;
    HBRUSH brushPopupItemHot = nullptr;
    HBRUSH brushPopupItemChecked = nullptr;
    HPEN penSeparator = nullptr;
    HPEN penPopupBorder = nullptr;
    HFONT font = nullptr;
    std::vector<ToolbarItem> items;
    std::vector<RECT> itemRects;
    std::vector<bool> itemVisible;
    std::vector<int> overflowIndices;
    RECT overflowRect{0, 0, 0, 0};
    std::vector<PopupEntry> popupEntries;
    int popupHoverIndex = -1;
    bool popupScrollable = false;
    bool popupCanScrollUp = false;
    bool popupCanScrollDown = false;
    int popupAutoScrollDirection = 0;
    // Tracks which toolbar trigger currently owns the custom popup so a repeated
    // click on the same trigger can close the popup instead of reopening it.
    int popupSourceIndex = -1;
    bool popupFromOverflow = false;
    // True when the current popup is showing a submenu opened from an overflow entry.
    bool popupEntriesFromSubmenu = false;
    // Temporary suppression absorbs the second half of a "close popup" click.
    // Without this, focus changes can close the popup first and the matching
    // toolbar mouse-up would immediately reopen it.
    int suppressPopupToggleIndex = -2;
    bool suppressPopupToggleOverflow = false;
    // Separate hover suppression avoids a one-frame hot-state flash while the popup closes.
    int suppressHotIndex = -2;
    bool suppressHotOverflow = false;
    int hotIndex = -1;
    int pressedIndex = -1;
    bool trackingMouseLeave = false;
    bool layoutDirty = true;

    explicit Impl(Toolbar* toolbar) : owner(toolbar) {}

    ~Impl() {
        if (brushBackground) DeleteObject(brushBackground);
        if (brushItem) DeleteObject(brushItem);
        if (brushItemHot) DeleteObject(brushItemHot);
        if (brushItemActive) DeleteObject(brushItemActive);
        if (brushPopupPanel) DeleteObject(brushPopupPanel);
        if (brushPopupItem) DeleteObject(brushPopupItem);
        if (brushPopupItemHot) DeleteObject(brushPopupItemHot);
        if (brushPopupItemChecked) DeleteObject(brushPopupItemChecked);
        if (penSeparator) DeleteObject(penSeparator);
        if (penPopupBorder) DeleteObject(penPopupBorder);
        if (font) DeleteObject(font);
    }

    bool UpdateThemeResources() {
        HBRUSH newBrushBackground = CreateSolidBrush(owner->theme_.toolbarBackground);
        HBRUSH newBrushItem = CreateSolidBrush(owner->theme_.toolbarItem);
        HBRUSH newBrushItemHot = CreateSolidBrush(owner->theme_.toolbarItemHot);
        HBRUSH newBrushItemActive = CreateSolidBrush(owner->theme_.toolbarItemActive);
        HBRUSH newBrushPopupPanel = CreateSolidBrush(owner->theme_.panel);
        HBRUSH newBrushPopupItem = CreateSolidBrush(owner->theme_.popupItem);
        HBRUSH newBrushPopupItemHot = CreateSolidBrush(owner->theme_.popupItemHot);
        HBRUSH newBrushPopupItemChecked = CreateSolidBrush(owner->theme_.popupAccentItem);
        HPEN newPenSeparator = CreatePen(PS_SOLID, 1, owner->theme_.toolbarSeparator);
        HPEN newPenPopupBorder = CreatePen(PS_SOLID, 1, owner->theme_.border);
        HFONT newFont = CreateFont(owner->theme_.uiFont);

        if (!newBrushBackground || !newBrushItem || !newBrushItemHot || !newBrushItemActive ||
            !newBrushPopupPanel || !newBrushPopupItem || !newBrushPopupItemHot || !newBrushPopupItemChecked ||
            !newPenSeparator || !newPenPopupBorder || !newFont) {
            if (newBrushBackground) DeleteObject(newBrushBackground);
            if (newBrushItem) DeleteObject(newBrushItem);
            if (newBrushItemHot) DeleteObject(newBrushItemHot);
            if (newBrushItemActive) DeleteObject(newBrushItemActive);
            if (newBrushPopupPanel) DeleteObject(newBrushPopupPanel);
            if (newBrushPopupItem) DeleteObject(newBrushPopupItem);
            if (newBrushPopupItemHot) DeleteObject(newBrushPopupItemHot);
            if (newBrushPopupItemChecked) DeleteObject(newBrushPopupItemChecked);
            if (newPenSeparator) DeleteObject(newPenSeparator);
            if (newPenPopupBorder) DeleteObject(newPenPopupBorder);
            if (newFont) DeleteObject(newFont);
            return false;
        }

        if (brushBackground) DeleteObject(brushBackground);
        if (brushItem) DeleteObject(brushItem);
        if (brushItemHot) DeleteObject(brushItemHot);
        if (brushItemActive) DeleteObject(brushItemActive);
        if (brushPopupPanel) DeleteObject(brushPopupPanel);
        if (brushPopupItem) DeleteObject(brushPopupItem);
        if (brushPopupItemHot) DeleteObject(brushPopupItemHot);
        if (brushPopupItemChecked) DeleteObject(brushPopupItemChecked);
        if (penSeparator) DeleteObject(penSeparator);
        if (penPopupBorder) DeleteObject(penPopupBorder);
        if (font) DeleteObject(font);

        brushBackground = newBrushBackground;
        brushItem = newBrushItem;
        brushItemHot = newBrushItemHot;
        brushItemActive = newBrushItemActive;
        brushPopupPanel = newBrushPopupPanel;
        brushPopupItem = newBrushPopupItem;
        brushPopupItemHot = newBrushPopupItemHot;
        brushPopupItemChecked = newBrushPopupItemChecked;
        penSeparator = newPenSeparator;
        penPopupBorder = newPenPopupBorder;
        font = newFont;
        layoutDirty = true;

        if (owner->toolbarHwnd_) {
            InvalidateRect(owner->toolbarHwnd_, nullptr, TRUE);
        }
        if (owner->popupList_) {
            SendMessageW(owner->popupList_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
        return true;
    }

    RECT ClientRect() const {
        RECT rect{};
        GetClientRect(owner->toolbarHwnd_, &rect);
        return rect;
    }

    RECT RectToScreen(HWND source, RECT rect) const {
        POINT topLeft{rect.left, rect.top};
        POINT bottomRight{rect.right, rect.bottom};
        ClientToScreen(source, &topLeft);
        ClientToScreen(source, &bottomRight);
        rect.left = topLeft.x;
        rect.top = topLeft.y;
        rect.right = bottomRight.x;
        rect.bottom = bottomRight.y;
        return rect;
    }

    ResolvedIconLayout ResolveIconLayout(const ToolbarItem& item, RECT rect) const {
        ResolvedIconLayout layout{};
        if (!item.icon) {
            return layout;
        }

        if (rect.right <= rect.left || rect.bottom <= rect.top) {
            return layout;
        }

        const int availableWidth = std::max(0, static_cast<int>(rect.right - rect.left));
        const int availableHeight = std::max(0, static_cast<int>(rect.bottom - rect.top));
        if (availableWidth <= 0 || availableHeight <= 0) {
            return layout;
        }

        const SIZE sourceSize = QueryIconSize(item.icon);
        const double sourceWidth = static_cast<double>(std::max(1, static_cast<int>(sourceSize.cx)));
        const double sourceHeight = static_cast<double>(std::max(1, static_cast<int>(sourceSize.cy)));

        const double scaleX = static_cast<double>(availableWidth) / sourceWidth;
        const double scaleY = static_cast<double>(availableHeight) / sourceHeight;
        const double scale = std::max(0.0, std::min(scaleX, scaleY));
        layout.drawWidth = std::max(1, std::min(availableWidth, static_cast<int>(sourceWidth * scale + 0.5)));
        layout.drawHeight = std::max(1, std::min(availableHeight, static_cast<int>(sourceHeight * scale + 0.5)));

        layout.bounds = rect;
        return layout;
    }

    int IconBoxExtent(const ToolbarItem& item, int baseHeight) const {
        const int scalePercent = ClampIconScalePercent(item.iconScalePercent);
        return std::max(1, (baseHeight * scalePercent + 50) / 100);
    }

    RECT IconBoundsForMeasurement(const ToolbarItem& item, int baseHeight) const {
        RECT bounds{0, 0, 0, baseHeight};
        const int iconExtent = IconBoxExtent(item, baseHeight);
        if (item.iconOnly) {
            bounds.top = (baseHeight - iconExtent) / 2;
            bounds.bottom = bounds.top + iconExtent;
            bounds.left = 0;
            bounds.right = iconExtent;
            return bounds;
        }

        bounds.top = (baseHeight - iconExtent) / 2;
        bounds.bottom = bounds.top + iconExtent;
        bounds.right = iconExtent;
        return bounds;
    }

    int CurrentItemHeight() const {
        if (!owner->toolbarHwnd_) {
            return std::max(owner->theme_.toolbarHeight - 12, 28);
        }
        RECT client{};
        GetClientRect(owner->toolbarHwnd_, &client);
        return std::max(28, static_cast<int>(client.bottom - client.top) - 12);
    }

    int ItemWidth(HDC dc, const ToolbarItem& item) const {
        if (item.separator) {
            return 14;
        }
        // Measure against the active font so icon/text/drop-down combinations scale
        // consistently across different DPI settings and font families.
        const int baseHeight = CurrentItemHeight();
        const int sideInset = std::max(owner->theme_.textPadding, kToolbarButtonSideInset);
        int iconFootprintWidth = 0;
        if (item.icon) {
            ResolvedIconLayout iconLayout = ResolveIconLayout(item, IconBoundsForMeasurement(item, baseHeight));
            iconFootprintWidth = iconLayout.drawWidth;
        }
        const int base = item.iconOnly ? std::max(baseHeight, iconFootprintWidth + sideInset * 2) : std::max(kToolbarButtonMinWidth, baseHeight);
        if (item.iconOnly) {
            return base;
        }
        RECT textRect{0, 0, 0, 0};
        DrawTextW(dc, item.text.c_str(), -1, &textRect, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
        int textWidth = std::max(0, static_cast<int>(textRect.right - textRect.left));
        if (item.icon) {
            textWidth += iconFootprintWidth + 8;
        }
        if (item.dropDown) {
            textWidth += kToolbarArrowWidth + 6;
        }
        return std::max(base, textWidth + sideInset * 2);
    }

    int OverflowWidth() const {
        return std::max(40, std::max(owner->theme_.toolbarHeight - 8, 34));
    }

    bool HasOverflow() const {
        return !overflowIndices.empty();
    }

    void RebuildItemRects() {
        itemRects.clear();
        itemRects.resize(items.size());
        itemVisible.assign(items.size(), true);
        overflowIndices.clear();
        overflowRect = RECT{0, 0, 0, 0};
        if (!owner->toolbarHwnd_) {
            layoutDirty = false;
            return;
        }

        HDC dc = GetDC(owner->toolbarHwnd_);
        if (!dc) {
            layoutDirty = false;
            return;
        }
        HFONT drawFont = font ? font : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HFONT oldFont = drawFont ? reinterpret_cast<HFONT>(SelectObject(dc, drawFont)) : nullptr;

        RECT client = ClientRect();
        const int top = client.top + 6;
        const int bottom = client.bottom - 6;
        const int leftStart = client.left + 8;
        const int rightStart = client.right - 8;
        int leftX = leftStart;
        int rightX = rightStart;

        std::vector<int> leftIndices;
        std::vector<int> rightIndices;
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            if (items[i].alignRight) {
                rightIndices.push_back(i);
            } else {
                leftIndices.push_back(i);
            }
        }

        // Lay out the right-aligned tool group first so the left group can use the
        // remaining width and decide whether overflow is needed.
        for (int index : rightIndices) {
            const int width = ItemWidth(dc, items[index]);
            rightX -= width;
            itemRects[index] = RECT{rightX, top, rightX + width, bottom};
            rightX -= kToolbarButtonGap;
        }

        const int maxLeftBoundary = std::max(leftStart, rightX);
        for (int pos = 0; pos < static_cast<int>(leftIndices.size()); ++pos) {
            const int index = leftIndices[pos];
            const int width = ItemWidth(dc, items[index]);
            const int nextRight = leftX + width;
            if (nextRight <= maxLeftBoundary) {
                itemRects[index] = RECT{leftX, top, nextRight, bottom};
                leftX = nextRight + kToolbarButtonGap;
                itemVisible[index] = true;
                continue;
            }

            itemVisible[index] = false;
            overflowIndices.push_back(index);
        }

        if (!overflowIndices.empty()) {
            const int overflowWidth = OverflowWidth();
            int overflowLeft = maxLeftBoundary - overflowWidth;
            if (overflowLeft < leftStart) {
                overflowLeft = leftStart;
            }
            overflowRect = RECT{overflowLeft, top, overflowLeft + overflowWidth, bottom};

            while (!overflowIndices.empty() && overflowRect.right > rightX) {
                int recovered = -1;
                for (int pos = static_cast<int>(leftIndices.size()) - 1; pos >= 0; --pos) {
                    const int index = leftIndices[pos];
                    if (itemVisible[index]) {
                        recovered = index;
                        break;
                    }
                }
                if (recovered < 0) {
                    break;
                }
                itemVisible[recovered] = false;
                overflowIndices.insert(overflowIndices.begin(), recovered);
                overflowRect.left -= (itemRects[recovered].right - itemRects[recovered].left) + kToolbarButtonGap;
                overflowRect.right = overflowRect.left + overflowWidth;
            }
        }

        if (oldFont) {
            SelectObject(dc, oldFont);
        }
        ReleaseDC(owner->toolbarHwnd_, dc);
        layoutDirty = false;
    }

    void EnsureLayout() {
        if (layoutDirty) {
            RebuildItemRects();
        }
    }

    RECT ItemRect(int index) {
        EnsureLayout();
        if (index < 0 || index >= static_cast<int>(itemRects.size())) {
            return RECT{0, 0, 0, 0};
        }
        return itemRects[index];
    }

    int HitTest(POINT point) {
        EnsureLayout();
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            if (!itemVisible[i]) {
                continue;
            }
            if (items[i].separator || items[i].disabled) {
                continue;
            }
            RECT rc = itemRects[i];
            if (PtInRect(&rc, point)) {
                return i;
            }
        }
        if (HasOverflow() && PtInRect(&overflowRect, point)) {
            return kOverflowCommandId;
        }
        return -1;
    }

    bool ArrowHit(int index, POINT point) {
        if (index < 0 || index >= static_cast<int>(items.size()) || !items[index].dropDown || !itemVisible[index]) {
            return false;
        }
        RECT rc = itemRects[index];
        rc.left = std::max(rc.left, rc.right - kToolbarArrowWidth - owner->theme_.textPadding);
        return PtInRect(&rc, point) != FALSE;
    }

    void NotifyClick(int index) {
        if (!owner->parentHwnd_ || !owner->toolbarHwnd_ || index < 0 || index >= static_cast<int>(items.size())) {
            return;
        }
        const ToolbarItem& item = items[index];
        SendMessageW(owner->parentHwnd_,
                     WM_COMMAND,
                     MAKEWPARAM(static_cast<UINT>(item.commandId), 0),
                     reinterpret_cast<LPARAM>(owner->toolbarHwnd_));
    }

    bool IsPopupVisible() const {
        return owner->popupHost_ && IsWindowVisible(owner->popupHost_);
    }

    int PopupEntryHeight(int index) const {
        if (index < 0 || index >= static_cast<int>(popupEntries.size())) {
            return owner->theme_.itemHeight;
        }
        return popupEntries[index].separator ? 8 : owner->theme_.itemHeight;
    }

    int PopupListHeight() const {
        if (!owner->popupList_) {
            return 0;
        }
        RECT rect{};
        GetClientRect(owner->popupList_, &rect);
        return std::max(0, static_cast<int>(rect.bottom - rect.top));
    }

    void UpdatePopupScrollState() {
        popupCanScrollUp = false;
        popupCanScrollDown = false;
        if (!owner->popupList_ || popupEntries.empty()) {
            popupScrollable = false;
            return;
        }

        const int visibleHeight = PopupListHeight();
        const int topIndex = static_cast<int>(SendMessageW(owner->popupList_, LB_GETTOPINDEX, 0, 0));
        int consumed = 0;
        int index = std::max(0, topIndex);
        while (index < static_cast<int>(popupEntries.size()) && consumed + PopupEntryHeight(index) <= visibleHeight) {
            consumed += PopupEntryHeight(index);
            ++index;
        }

        popupCanScrollUp = topIndex > 0;
        popupCanScrollDown = index < static_cast<int>(popupEntries.size());
    }

    void StopPopupAutoScroll() {
        popupAutoScrollDirection = 0;
        if (owner->popupHost_) {
            KillTimer(owner->popupHost_, kToolbarPopupScrollTimerId);
        }
    }

    void StartPopupAutoScroll(int direction) {
        if (!popupScrollable || direction == 0 || !owner->popupHost_) {
            StopPopupAutoScroll();
            return;
        }
        if (popupAutoScrollDirection != direction) {
            popupAutoScrollDirection = direction;
            SetTimer(owner->popupHost_, kToolbarPopupScrollTimerId, 120, nullptr);
        }
    }

    void ScrollPopupByItems(int delta) {
        if (!owner->popupList_ || delta == 0) {
            return;
        }
        const int count = static_cast<int>(popupEntries.size());
        if (count <= 0) {
            return;
        }
        int topIndex = static_cast<int>(SendMessageW(owner->popupList_, LB_GETTOPINDEX, 0, 0));
        topIndex = std::clamp(topIndex + delta, 0, std::max(0, count - 1));
        SendMessageW(owner->popupList_, LB_SETTOPINDEX, static_cast<WPARAM>(topIndex), 0);
        UpdatePopupScrollState();
        RedrawWindow(owner->popupHost_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    }

    void HidePopup() {
        if (owner->popupHost_) {
            ShowWindow(owner->popupHost_, SW_HIDE);
        }
        StopPopupAutoScroll();
        popupEntries.clear();
        popupHoverIndex = -1;
        popupScrollable = false;
        popupCanScrollUp = false;
        popupCanScrollDown = false;
        popupSourceIndex = -1;
        popupFromOverflow = false;
        popupEntriesFromSubmenu = false;
    }

    void SuppressToggleForCurrentClick(int hitIndex) {
        suppressPopupToggleIndex = hitIndex;
        suppressPopupToggleOverflow = (hitIndex == kOverflowCommandId);
        suppressHotIndex = hitIndex;
        suppressHotOverflow = (hitIndex == kOverflowCommandId);
    }

    bool ConsumeToggleSuppression(int releasedIndex) {
        const bool suppressed = suppressPopupToggleOverflow
            ? (releasedIndex == kOverflowCommandId)
            : (releasedIndex == suppressPopupToggleIndex);
        suppressPopupToggleIndex = -2;
        suppressPopupToggleOverflow = false;
        return suppressed;
    }

    int FilterSuppressedHotIndex(int hitIndex) {
        const bool suppressed = suppressHotOverflow
            ? (hitIndex == kOverflowCommandId)
            : (hitIndex == suppressHotIndex);
        if (suppressed) {
            return -1;
        }
        suppressHotIndex = -2;
        suppressHotOverflow = false;
        return hitIndex;
    }

    void SuppressToggleFromCursorIfNeeded() {
        if (!owner->toolbarHwnd_) {
            return;
        }
        POINT pt{};
        if (!GetCursorPos(&pt)) {
            return;
        }
        ScreenToClient(owner->toolbarHwnd_, &pt);
        // If the popup is losing activation because the user clicked its owning
        // toolbar trigger, remember that hit so the matching mouse-up does not reopen it.
        const int hitIndex = HitTest(pt);
        const bool sameOverflow = hitIndex == kOverflowCommandId && popupFromOverflow;
        const bool sameDropdown = hitIndex >= 0 && !popupFromOverflow && popupSourceIndex == hitIndex;
        if (sameOverflow || sameDropdown) {
            SuppressToggleForCurrentClick(hitIndex);
        }
    }

    std::vector<PopupEntry> BuildEntriesFromMenu(HMENU menu) {
        std::vector<PopupEntry> entries;
        if (!menu) {
            return entries;
        }
        const int count = GetMenuItemCount(menu);
        for (int i = 0; i < count; ++i) {
            MENUITEMINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_STRING;
            info.dwTypeData = nullptr;
            info.cch = 0;
            if (!GetMenuItemInfoW(menu, i, TRUE, &info)) {
                continue;
            }
            std::wstring text;
            if ((info.fType & MFT_SEPARATOR) == 0) {
                text.resize(static_cast<size_t>(info.cch) + 1, L'\0');
                info.dwTypeData = text.data();
                info.cch = static_cast<UINT>(text.size());
                if (GetMenuItemInfoW(menu, i, TRUE, &info)) {
                    text.resize(wcsnlen(text.c_str(), text.size()));
                } else {
                    text.clear();
                }
            }
            PopupEntry entry{};
            entry.separator = (info.fType & MFT_SEPARATOR) != 0;
            entry.checked = (info.fState & MFS_CHECKED) != 0;
            entry.disabled = (info.fState & (MFS_DISABLED | MFS_GRAYED)) != 0;
            entry.commandId = static_cast<int>(info.wID);
            entry.popupMenu = nullptr;
            entry.text = entry.separator ? L"" : text;
            entries.push_back(entry);
        }
        return entries;
    }

    std::vector<PopupEntry> BuildOverflowEntries() {
        std::vector<PopupEntry> entries;
        for (int index : overflowIndices) {
            const ToolbarItem& item = items[index];
            if (item.separator) {
                PopupEntry separator{};
                separator.separator = true;
                entries.push_back(separator);
                continue;
            }
            if (item.dropDown && item.popupMenu) {
                // Preserve drop-down hierarchy inside overflow instead of flattening
                // all submenu commands into a single unstructured list.
                PopupEntry entry{};
                entry.text = item.text.empty() ? L"Menu" : item.text;
                entry.sourceIndex = index;
                entry.popupMenu = item.popupMenu;
                entry.checked = item.checked;
                entry.disabled = item.disabled;
                entry.opensSubmenu = true;
                entries.push_back(entry);
                continue;
            }
            PopupEntry entry{};
            entry.text = item.text;
            entry.commandId = item.commandId;
            entry.sourceIndex = index;
            entry.checked = item.checked;
            entry.disabled = item.disabled;
            entries.push_back(entry);
        }
        return entries;
    }

    void ShowPopupEntries(const std::vector<PopupEntry>& entries, RECT anchorRectScreen, bool fromSubmenu = false) {
        if (entries.empty() || !owner->popupHost_ || !owner->popupList_ || !owner->toolbarHwnd_) {
            return;
        }
        popupEntries = entries;
        popupHoverIndex = -1;
        popupEntriesFromSubmenu = fromSubmenu;

        SendMessageW(owner->popupList_, WM_SETREDRAW, FALSE, 0);
        SendMessageW(owner->popupList_, LB_RESETCONTENT, 0, 0);
        for (const auto& entry : popupEntries) {
            const wchar_t* label = entry.separator ? L"" : entry.text.c_str();
            SendMessageW(owner->popupList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
        }
        SendMessageW(owner->popupList_, WM_SETREDRAW, TRUE, 0);

        HDC dc = GetDC(owner->toolbarHwnd_);
        HFONT drawFont = font ? font : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HFONT oldFont = drawFont ? reinterpret_cast<HFONT>(SelectObject(dc, drawFont)) : nullptr;
        int width = kToolbarPopupMinWidth;
        for (const auto& entry : popupEntries) {
            if (entry.separator) {
                continue;
            }
            RECT textRect{0, 0, 0, 0};
            DrawTextW(dc, entry.text.c_str(), -1, &textRect, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
            int entryWidth = std::max(0, static_cast<int>(textRect.right - textRect.left)) + owner->theme_.textPadding * 2 + 16;
            if (entry.checked) {
                entryWidth += 12;
            }
            if (entry.opensSubmenu) {
                entryWidth += kToolbarPopupArrowInset;
            }
            width = std::max(width, entryWidth);
        }
        if (oldFont) {
            SelectObject(dc, oldFont);
        }
        ReleaseDC(owner->toolbarHwnd_, dc);

        const int measuredItemHeight = std::max(1, static_cast<int>(SendMessageW(owner->popupList_, LB_GETITEMHEIGHT, 0, 0)));
        int contentHeight = measuredItemHeight * static_cast<int>(popupEntries.size());
        int clientHeight = contentHeight + kToolbarPopupOuterPadding;

        RECT parentRect{};
        RECT workArea{};
        GetWindowRect(owner->parentHwnd_, &parentRect);
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

        RECT bounds = parentRect;
        bounds.left = std::max(bounds.left, workArea.left);
        bounds.top = std::max(bounds.top, workArea.top);
        bounds.right = std::min(bounds.right, workArea.right);
        bounds.bottom = std::min(bounds.bottom, workArea.bottom);

        const int belowAvailable = std::max(0, static_cast<int>(bounds.bottom - (anchorRectScreen.bottom + 4)));
        const int aboveAvailable = std::max(0, static_cast<int>((anchorRectScreen.top - 4) - bounds.top));
        const int fallbackMaxHeight = std::max(owner->theme_.itemHeight + kToolbarPopupOuterPadding,
                                               static_cast<int>((bounds.bottom - bounds.top) - 12));
        const int availableHeight = std::max(owner->theme_.itemHeight + kToolbarPopupOuterPadding,
                                             std::max(belowAvailable, aboveAvailable));
        const int maxHeight = std::min(fallbackMaxHeight, availableHeight);
        popupScrollable = clientHeight > maxHeight;
        if (popupScrollable) {
            clientHeight = maxHeight;
        }
        RECT popupWindowRect{0, 0, width, clientHeight};
        AdjustWindowRectEx(&popupWindowRect, WS_POPUP | WS_CLIPSIBLINGS | WS_BORDER, FALSE, 0);
        const int windowWidth = popupWindowRect.right - popupWindowRect.left;
        const int windowHeight = popupWindowRect.bottom - popupWindowRect.top;
        // The popup host is a true popup window, not a child window, so it can extend
        // beyond the toolbar client area like a standard menu while staying custom drawn.
        int x = anchorRectScreen.left;
        int y = anchorRectScreen.bottom + 4;
        if (x + windowWidth > bounds.right) {
            x = std::max(bounds.left, bounds.right - windowWidth);
        }
        const bool preferBelow = !popupScrollable
            ? (windowHeight <= belowAvailable || belowAvailable >= aboveAvailable)
            : (belowAvailable >= aboveAvailable);
        if (!preferBelow) {
            y = anchorRectScreen.top - 4 - windowHeight;
        }
        if (y < bounds.top) {
            y = bounds.top;
        }
        if (y + windowHeight > bounds.bottom) {
            y = std::max(bounds.top, bounds.bottom - windowHeight);
        }
        SetWindowPos(owner->popupHost_, HWND_TOPMOST, x, y, windowWidth, windowHeight, SWP_SHOWWINDOW);
        const int indicatorTop = popupScrollable ? kToolbarPopupIndicatorHeight : 1;
        const int indicatorBottom = popupScrollable ? kToolbarPopupIndicatorHeight : 1;
        MoveWindow(owner->popupList_,
                   1,
                   indicatorTop,
                   std::max(1, width - 2),
                   std::max(1, clientHeight - indicatorTop - indicatorBottom),
                   TRUE);
        SendMessageW(owner->popupList_, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
        SendMessageW(owner->popupList_, LB_SETTOPINDEX, 0, 0);
        UpdatePopupScrollState();
        StopPopupAutoScroll();
        SetFocus(owner->popupList_);
        RedrawWindow(owner->popupHost_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    }

    void ApplyPopupSelection() {
        const int selected = static_cast<int>(SendMessageW(owner->popupList_, LB_GETCURSEL, 0, 0));
        if (selected < 0 || selected >= static_cast<int>(popupEntries.size())) {
            HidePopup();
            return;
        }
        const PopupEntry entry = popupEntries[selected];
        if (entry.separator || entry.disabled) {
            return;
        }
        if (entry.opensSubmenu && entry.popupMenu) {
            // Re-anchor nested submenu popups to the active row in the current popup.
            RECT itemRect{};
            SendMessageW(owner->popupList_, LB_GETITEMRECT, static_cast<WPARAM>(selected), reinterpret_cast<LPARAM>(&itemRect));
            RECT popupRect{};
            GetWindowRect(owner->popupHost_, &popupRect);
            OffsetRect(&itemRect, popupRect.left + 1, popupRect.top + 1);
            ShowPopupEntries(BuildEntriesFromMenu(entry.popupMenu), itemRect, true);
            return;
        }
        HidePopup();
        if (owner->parentHwnd_) {
            SendMessageW(owner->parentHwnd_,
                         WM_COMMAND,
                         MAKEWPARAM(static_cast<UINT>(entry.commandId), 0),
                         reinterpret_cast<LPARAM>(owner->toolbarHwnd_));
        }
    }

    void CloseOnOutsideClick(short x, short y) {
        if (!IsPopupVisible()) {
            return;
        }
        POINT pt{x, y};
        ClientToScreen(owner->parentHwnd_, &pt);
        RECT popupRect{};
        RECT toolbarRect{};
        GetWindowRect(owner->popupHost_, &popupRect);
        GetWindowRect(owner->toolbarHwnd_, &toolbarRect);
        if (!PtInRect(&popupRect, pt) && !PtInRect(&toolbarRect, pt)) {
            HidePopup();
        }
    }

    void ShowItemDropDown(int index) {
        if (index < 0 || index >= static_cast<int>(items.size())) {
            return;
        }
        if (IsPopupVisible() && !popupFromOverflow && popupSourceIndex == index) {
            HidePopup();
            return;
        }
        popupSourceIndex = index;
        popupFromOverflow = false;
        ShowPopupEntries(BuildEntriesFromMenu(items[index].popupMenu), RectToScreen(owner->toolbarHwnd_, itemRects[index]));
    }

    void ShowOverflowMenu() {
        if (!HasOverflow()) {
            return;
        }
        if (IsPopupVisible() && popupFromOverflow) {
            HidePopup();
            return;
        }
        popupSourceIndex = -1;
        popupFromOverflow = true;
        ShowPopupEntries(BuildOverflowEntries(), RectToScreen(owner->toolbarHwnd_, overflowRect));
    }

    void StartMouseLeaveTracking() {
        if (!owner->toolbarHwnd_ || trackingMouseLeave) {
            return;
        }
        TRACKMOUSEEVENT track{};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = owner->toolbarHwnd_;
        if (TrackMouseEvent(&track)) {
            trackingMouseLeave = true;
        }
    }

    void Paint(HDC dc) {
        EnsureLayout();
        RECT client = ClientRect();
        FillRect(dc, &client, brushBackground ? brushBackground : reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

        HFONT drawFont = font ? font : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HFONT oldFont = drawFont ? reinterpret_cast<HFONT>(SelectObject(dc, drawFont)) : nullptr;
        SetBkMode(dc, TRANSPARENT);

        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const ToolbarItem& item = items[i];
            if (!itemVisible[i]) {
                continue;
            }
            RECT rc = itemRects[i];

            if (item.separator) {
                HPEN drawPen = penSeparator ? penSeparator : reinterpret_cast<HPEN>(GetStockObject(DC_PEN));
                HPEN oldPen = drawPen ? reinterpret_cast<HPEN>(SelectObject(dc, drawPen)) : nullptr;
                const int x = (rc.left + rc.right) / 2;
                MoveToEx(dc, x, rc.top + 4, nullptr);
                LineTo(dc, x, rc.bottom - 4);
                if (oldPen) {
                    SelectObject(dc, oldPen);
                }
                continue;
            }

            HBRUSH fill = brushItem;
            COLORREF textColor = owner->theme_.toolbarText;
            const bool popupOwnerActive = IsPopupVisible() && !popupFromOverflow && popupSourceIndex == i;
            if (item.checked || i == pressedIndex || popupOwnerActive) {
                fill = brushItemActive;
                textColor = owner->theme_.toolbarTextActive;
            } else if (i == hotIndex) {
                fill = brushItemHot;
            }

            if (item.disabled) {
                fill = brushItem;
                textColor = owner->theme_.buttonDisabledText;
            }

            HGDIOBJ oldBrush = SelectObject(dc, fill ? fill : reinterpret_cast<HBRUSH>(GetStockObject(DKGRAY_BRUSH)));
            HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(dc, GetStockObject(NULL_PEN)));
            RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, 12, 12);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);

            RECT contentRect = rc;
            contentRect.left += owner->theme_.textPadding;
            contentRect.right -= owner->theme_.textPadding;
            if (item.dropDown) {
                contentRect.right -= kToolbarArrowWidth;
            }

            if (item.icon) {
                const int iconExtent = IconBoxExtent(item, static_cast<int>(rc.bottom - rc.top));
                RECT iconRect = item.iconOnly
                                    ? RECT{rc.left + (static_cast<int>(rc.right - rc.left) - iconExtent) / 2,
                                           rc.top + (static_cast<int>(rc.bottom - rc.top) - iconExtent) / 2,
                                           rc.left + (static_cast<int>(rc.right - rc.left) - iconExtent) / 2 + iconExtent,
                                           rc.top + (static_cast<int>(rc.bottom - rc.top) - iconExtent) / 2 + iconExtent}
                                    : RECT{contentRect.left,
                                           rc.top + (static_cast<int>(rc.bottom - rc.top) - iconExtent) / 2,
                                           contentRect.left + iconExtent,
                                           rc.top + (static_cast<int>(rc.bottom - rc.top) - iconExtent) / 2 + iconExtent};
                ResolvedIconLayout iconLayout = ResolveIconLayout(item, iconRect);
                const int iconX = item.iconOnly ? (iconLayout.bounds.left + iconLayout.bounds.right - iconLayout.drawWidth) / 2 : iconLayout.bounds.left;
                const int iconY = iconLayout.bounds.top +
                                  std::max(0, static_cast<int>(iconLayout.bounds.bottom - iconLayout.bounds.top - iconLayout.drawHeight) / 2);
                DrawIconEx(dc, iconX, iconY, item.icon, iconLayout.drawWidth, iconLayout.drawHeight, 0, nullptr, DI_NORMAL);
                if (!item.iconOnly) {
                    contentRect.left += iconLayout.drawWidth + 8;
                }
            }

            if (!item.iconOnly) {
                SetTextColor(dc, textColor);
                DrawTextW(dc, item.text.c_str(), -1, &contentRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            }

            if (item.dropDown) {
                RECT arrowRect = rc;
                arrowRect.left = std::max(arrowRect.left, rc.right - kToolbarArrowWidth - owner->theme_.textPadding + 2);
                arrowRect.right -= owner->theme_.textPadding / 2;
                const int centerX = (arrowRect.left + arrowRect.right) / 2;
                const int centerY = (arrowRect.top + arrowRect.bottom) / 2;
                POINT pts[3]{
                    {centerX - 4, centerY - 2},
                    {centerX + 4, centerY - 2},
                    {centerX, centerY + 3}
                };
                HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
                HBRUSH arrowBrush = CreateSolidBrush(textColor);
                HGDIOBJ oldBrushArrow = SelectObject(dc, arrowBrush);
                Polygon(dc, pts, 3);
                SelectObject(dc, oldBrushArrow);
                SelectObject(dc, oldPen);
                DeleteObject(arrowBrush);
            }
        }

        if (HasOverflow()) {
            HBRUSH overflowFill = brushItem;
            COLORREF overflowText = owner->theme_.toolbarText;
            const bool popupOverflowActive = IsPopupVisible() && popupFromOverflow;
            if (popupOverflowActive || hotIndex == kOverflowCommandId || pressedIndex == kOverflowCommandId) {
                overflowFill = (pressedIndex == kOverflowCommandId || popupOverflowActive) ? brushItemActive : brushItemHot;
                overflowText = (pressedIndex == kOverflowCommandId || popupOverflowActive) ? owner->theme_.toolbarTextActive : owner->theme_.toolbarText;
            }
            HGDIOBJ oldBrush = SelectObject(dc, overflowFill ? overflowFill : reinterpret_cast<HBRUSH>(GetStockObject(DKGRAY_BRUSH)));
            HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(dc, GetStockObject(NULL_PEN)));
            RoundRect(dc, overflowRect.left, overflowRect.top, overflowRect.right, overflowRect.bottom, 12, 12);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);

            RECT dotsRect = overflowRect;
            SetTextColor(dc, overflowText);
            DrawTextW(dc, L"...", -1, &dotsRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        if (oldFont) {
            SelectObject(dc, oldFont);
        }
    }

    void DrawPopupItem(const DRAWITEMSTRUCT* draw) {
        if (draw->itemID == static_cast<UINT>(-1) || draw->itemID >= popupEntries.size()) {
            return;
        }
        const PopupEntry& entry = popupEntries[draw->itemID];
        RECT rc = draw->rcItem;
        if (entry.separator) {
            FillRect(draw->hDC, &rc, brushPopupPanel);
            HPEN oldPen = penSeparator ? reinterpret_cast<HPEN>(SelectObject(draw->hDC, penSeparator)) : nullptr;
            const int y = (rc.top + rc.bottom) / 2;
            MoveToEx(draw->hDC, rc.left + 8, y, nullptr);
            LineTo(draw->hDC, rc.right - 8, y);
            if (oldPen) {
                SelectObject(draw->hDC, oldPen);
            }
            return;
        }

        HBRUSH fill = entry.checked ? brushPopupItemChecked : brushPopupItem;
        const bool isHot = static_cast<int>(draw->itemID) == popupHoverIndex;
        if ((draw->itemState & ODS_SELECTED) != 0 || isHot) {
            fill = brushPopupItemHot;
        }
        FillRect(draw->hDC, &rc, fill ? fill : brushPopupPanel);
        HFONT oldFont = font ? reinterpret_cast<HFONT>(SelectObject(draw->hDC, font)) : nullptr;
        SetBkMode(draw->hDC, TRANSPARENT);
        SetTextColor(draw->hDC, entry.disabled ? owner->theme_.buttonDisabledText : owner->theme_.text);
        rc.left += owner->theme_.textPadding;
        rc.right -= owner->theme_.textPadding;
        if (entry.checked) {
            RECT markRect = rc;
            markRect.right = markRect.left + 12;
            DrawTextW(draw->hDC, L"\u2713", -1, &markRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            rc.left = markRect.right + 6;
        }
        RECT textRect = rc;
        if (entry.opensSubmenu) {
            textRect.right -= kToolbarPopupArrowInset;
        }
        DrawTextW(draw->hDC, entry.text.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        if (entry.opensSubmenu) {
            RECT arrowRect = rc;
            arrowRect.left = arrowRect.right - kToolbarPopupArrowInset;
            const int centerX = (arrowRect.left + arrowRect.right) / 2;
            const int centerY = (arrowRect.top + arrowRect.bottom) / 2;
            POINT pts[3]{
                {centerX - 3, centerY - 4},
                {centerX - 3, centerY + 4},
                {centerX + 2, centerY}
            };
            HGDIOBJ oldPen = SelectObject(draw->hDC, GetStockObject(NULL_PEN));
            HBRUSH arrowBrush = CreateSolidBrush(entry.disabled ? owner->theme_.buttonDisabledText : owner->theme_.text);
            HGDIOBJ oldBrush = SelectObject(draw->hDC, arrowBrush);
            Polygon(draw->hDC, pts, 3);
            SelectObject(draw->hDC, oldBrush);
            SelectObject(draw->hDC, oldPen);
            DeleteObject(arrowBrush);
        }
        if (oldFont) {
            SelectObject(draw->hDC, oldFont);
        }
    }

    static LRESULT CALLBACK ToolbarWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));

        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = reinterpret_cast<Impl*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            return TRUE;
        }

        if (!self) {
            return DefWindowProcW(window, message, wParam, lParam);
        }

        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);
            self->Paint(dc);
            EndPaint(window, &ps);
            return 0;
        }
        case WM_SIZE:
            self->layoutDirty = true;
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_MOUSEMOVE: {
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const int index = self->FilterSuppressedHotIndex(self->HitTest(pt));
            if (self->hotIndex != index) {
                self->hotIndex = index;
                InvalidateRect(window, nullptr, FALSE);
            }
            self->StartMouseLeaveTracking();
            return 0;
        }
        case WM_MOUSELEAVE:
            self->trackingMouseLeave = false;
            self->hotIndex = -1;
            self->suppressHotIndex = -2;
            self->suppressHotOverflow = false;
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(window);
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            self->pressedIndex = self->HitTest(pt);
            if (self->IsPopupVisible()) {
                const bool sameOverflow = self->pressedIndex == kOverflowCommandId && self->popupFromOverflow;
                const bool sameDropdown = self->pressedIndex >= 0 && !self->popupFromOverflow && self->popupSourceIndex == self->pressedIndex;
                if (sameOverflow || sameDropdown) {
                    self->SuppressToggleForCurrentClick(self->pressedIndex);
                    self->pressedIndex = -1;
                    self->hotIndex = -1;
                    self->HidePopup();
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
            }
            if (self->pressedIndex >= 0) {
                SetCapture(window);
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const int releasedIndex = self->HitTest(pt);
            const int pressedIndex = self->pressedIndex;
            self->pressedIndex = -1;
            if (GetCapture() == window) {
                ReleaseCapture();
            }
            if (self->ConsumeToggleSuppression(releasedIndex)) {
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            InvalidateRect(window, nullptr, FALSE);
            if (pressedIndex == kOverflowCommandId && releasedIndex == kOverflowCommandId) {
                self->ShowOverflowMenu();
                return 0;
            }
            if (pressedIndex >= 0 && pressedIndex == releasedIndex) {
                if (self->ArrowHit(pressedIndex, pt) || self->items[pressedIndex].dropDown) {
                    self->ShowItemDropDown(pressedIndex);
                    return 0;
                }
                self->NotifyClick(pressedIndex);
            }
            return 0;
        }
        case WM_CAPTURECHANGED:
            self->pressedIndex = -1;
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_SPACE || wParam == VK_RETURN) {
                const int index = self->hotIndex >= 0 ? self->hotIndex : 0;
                if (index >= 0 && index < static_cast<int>(self->items.size()) && !self->items[index].separator && !self->items[index].disabled) {
                    if (self->items[index].dropDown) {
                        self->ShowItemDropDown(index);
                    } else {
                        self->NotifyClick(index);
                    }
                    return 0;
                }
            }
            break;
        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }

    static LRESULT CALLBACK PopupWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (!self) {
            return DefWindowProcW(window, message, wParam, lParam);
        }

        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);
            RECT rect{};
            GetClientRect(window, &rect);
            FillRect(dc, &rect, self->brushPopupPanel);
            if (self->popupScrollable) {
                RECT topRect{1, 1, rect.right - 1, 1 + kToolbarPopupIndicatorHeight};
                RECT bottomRect{1, rect.bottom - 1 - kToolbarPopupIndicatorHeight, rect.right - 1, rect.bottom - 1};
                FillRect(dc, &topRect, self->brushPopupPanel);
                FillRect(dc, &bottomRect, self->brushPopupPanel);

                const COLORREF arrowColor = self->owner->theme_.mutedText;
                HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
                HBRUSH arrowBrush = CreateSolidBrush(arrowColor);
                HGDIOBJ oldBrushArrow = SelectObject(dc, arrowBrush);
                if (self->popupCanScrollUp) {
                    const int centerX = (topRect.left + topRect.right) / 2;
                    const int centerY = (topRect.top + topRect.bottom) / 2 + 1;
                    POINT pts[3]{{centerX - 5, centerY + 2}, {centerX + 5, centerY + 2}, {centerX, centerY - 3}};
                    Polygon(dc, pts, 3);
                }
                if (self->popupCanScrollDown) {
                    const int centerX = (bottomRect.left + bottomRect.right) / 2;
                    const int centerY = (bottomRect.top + bottomRect.bottom) / 2 - 1;
                    POINT pts[3]{{centerX - 5, centerY - 2}, {centerX + 5, centerY - 2}, {centerX, centerY + 3}};
                    Polygon(dc, pts, 3);
                }
                SelectObject(dc, oldBrushArrow);
                SelectObject(dc, oldPen);
                DeleteObject(arrowBrush);
            }
            HPEN oldPen = self->penPopupBorder ? reinterpret_cast<HPEN>(SelectObject(dc, self->penPopupBorder)) : nullptr;
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
            SelectObject(dc, oldBrush);
            if (oldPen) {
                SelectObject(dc, oldPen);
            }
            EndPaint(window, &ps);
            return 0;
        }
        case WM_MEASUREITEM: {
            auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
            if (measure && measure->CtlID == kToolbarPopupListId) {
                const UINT itemId = measure->itemID;
                measure->itemHeight = (itemId < self->popupEntries.size() && self->popupEntries[itemId].separator)
                    ? 8
                    : self->owner->theme_.itemHeight;
                return TRUE;
            }
            break;
        }
        case WM_DRAWITEM: {
            auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (draw && draw->hwndItem == self->owner->popupList_) {
                self->DrawPopupItem(draw);
                return TRUE;
            }
            break;
        }
        case WM_CTLCOLORLISTBOX:
            if (reinterpret_cast<HWND>(lParam) == self->owner->popupList_) {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetBkColor(dc, self->owner->theme_.panel);
                SetTextColor(dc, self->owner->theme_.text);
                return reinterpret_cast<LRESULT>(self->brushPopupPanel ? self->brushPopupPanel : GetStockObject(BLACK_BRUSH));
            }
            break;
        case WM_COMMAND:
            if (LOWORD(wParam) == kToolbarPopupListId && HIWORD(wParam) == LBN_DBLCLK) {
                self->ApplyPopupSelection();
                return 0;
            }
            break;
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                self->SuppressToggleFromCursorIfNeeded();
                self->HidePopup();
                return 0;
            }
            break;
        case WM_MOUSEWHEEL:
            self->ScrollPopupByItems(GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? -1 : 1);
            return 0;
        case WM_TIMER:
            if (wParam == kToolbarPopupScrollTimerId) {
                if (self->popupAutoScrollDirection != 0) {
                    self->ScrollPopupByItems(self->popupAutoScrollDirection);
                }
                return 0;
            }
            break;
        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }

    static LRESULT CALLBACK PopupListSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData) {
        auto* self = reinterpret_cast<Impl*>(refData);
        if (!self) {
            return DefSubclassProc(window, message, wParam, lParam);
        }

        switch (message) {
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT track{};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = window;
            TrackMouseEvent(&track);

            RECT client{};
            GetClientRect(window, &client);
            const int y = GET_Y_LPARAM(lParam);
            if (self->popupScrollable) {
                if (y <= kToolbarPopupIndicatorHeight) {
                    self->StartPopupAutoScroll(-1);
                } else if (y >= client.bottom - kToolbarPopupIndicatorHeight) {
                    self->StartPopupAutoScroll(1);
                } else {
                    self->StopPopupAutoScroll();
                }
            }

            const LRESULT hit = SendMessageW(window, LB_ITEMFROMPOINT, 0, lParam);
            int hoverIndex = -1;
            if (HIWORD(hit) == 0) {
                const int index = static_cast<int>(LOWORD(hit));
                if (index >= 0 && index < static_cast<int>(self->popupEntries.size()) &&
                    !self->popupEntries[index].separator && !self->popupEntries[index].disabled) {
                    hoverIndex = index;
                }
            }
            if (self->popupHoverIndex != hoverIndex) {
                self->popupHoverIndex = hoverIndex;
                InvalidateRect(window, nullptr, FALSE);
            }
            return DefSubclassProc(window, message, wParam, lParam);
        }
        case WM_LBUTTONUP: {
            const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
            const LRESULT hit = SendMessageW(window, LB_ITEMFROMPOINT, 0, lParam);
            if (HIWORD(hit) == 0) {
                SendMessageW(window, LB_SETCURSEL, static_cast<WPARAM>(LOWORD(hit)), 0);
                self->ApplyPopupSelection();
            }
            return result;
        }
        case WM_MOUSELEAVE:
            self->StopPopupAutoScroll();
            if (self->popupHoverIndex != -1) {
                self->popupHoverIndex = -1;
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_RETURN || wParam == VK_SPACE || wParam == VK_RIGHT) {
                self->ApplyPopupSelection();
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                self->HidePopup();
                return 0;
            }
            if (wParam == VK_UP) {
                self->ScrollPopupByItems(-1);
                return 0;
            }
            if (wParam == VK_DOWN) {
                self->ScrollPopupByItems(1);
                return 0;
            }
            break;
        case WM_MOUSEWHEEL:
            self->ScrollPopupByItems(GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? -1 : 1);
            return 0;
        case WM_NCDESTROY:
            self->StopPopupAutoScroll();
            RemoveWindowSubclass(window, PopupListSubclassProc, subclassId);
            break;
        default:
            break;
        }

        return DefSubclassProc(window, message, wParam, lParam);
    }

    static LRESULT CALLBACK ParentSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData) {
        auto* self = reinterpret_cast<Impl*>(refData);
        if (!self) {
            return DefSubclassProc(window, message, wParam, lParam);
        }

        switch (message) {
        case WM_LBUTTONDOWN:
            self->CloseOnOutsideClick(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
            break;
        case WM_NCLBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            self->HidePopup();
            break;
        case WM_SIZE:
        case WM_MOVE:
        case WM_MOVING:
            self->HidePopup();
            break;
        case WM_DESTROY:
            RemoveWindowSubclass(window, ParentSubclassProc, subclassId);
            break;
        default:
            break;
        }

        return DefSubclassProc(window, message, wParam, lParam);
    }
};

namespace {

ATOM EnsureToolbarClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ToolbarWindowProcThunk;
    wc.hInstance = instance;
    wc.lpszClassName = kToolbarClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

ATOM EnsureToolbarPopupClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ToolbarPopupWindowProcThunk;
    wc.hInstance = instance;
    wc.lpszClassName = kToolbarPopupClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

LRESULT CALLBACK ToolbarWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return Toolbar::Impl::ToolbarWindowProc(window, message, wParam, lParam);
}

LRESULT CALLBACK ToolbarPopupWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return Toolbar::Impl::PopupWindowProc(window, message, wParam, lParam);
}

}  // namespace

Toolbar::Toolbar() : impl_(std::make_unique<Impl>(this)) {}

Toolbar::~Toolbar() {
    Destroy();
}

bool Toolbar::Create(HWND parent, int controlId, const Theme& theme, DWORD style, DWORD exStyle) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    theme_ = theme;
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }

    if (!EnsureToolbarClassRegistered(impl_->instance) || !EnsureToolbarPopupClassRegistered(impl_->instance)) {
        Destroy();
        return false;
    }

    toolbarHwnd_ = CreateWindowExW(exStyle,
                                   kToolbarClassName,
                                   L"",
                                   style | WS_CLIPSIBLINGS,
                                   0,
                                   0,
                                   0,
                                   0,
                                   parent,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                   impl_->instance,
                                   impl_.get());
    if (!toolbarHwnd_) {
        Destroy();
        return false;
    }

    popupHost_ = CreateWindowExW(0,
                                 kToolbarPopupClassName,
                                 nullptr,
                                 WS_POPUP | WS_CLIPSIBLINGS | WS_BORDER,
                                 0,
                                 0,
                                 0,
                                 0,
                                 parent,
                                 nullptr,
                                 impl_->instance,
                                 nullptr);
    if (!popupHost_) {
        Destroy();
        return false;
    }
    SetWindowLongPtrW(popupHost_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl_.get()));

    popupList_ = CreateWindowExW(0,
                                 L"LISTBOX",
                                 nullptr,
                                 WS_CHILD | WS_VISIBLE | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                                 0,
                                 0,
                                 0,
                                 0,
                                 popupHost_,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolbarPopupListId)),
                                 impl_->instance,
                                 nullptr);
    if (!popupList_) {
        Destroy();
        return false;
    }
    SetWindowSubclass(popupList_, Impl::PopupListSubclassProc, reinterpret_cast<UINT_PTR>(this), reinterpret_cast<DWORD_PTR>(impl_.get()));

    if (!impl_->UpdateThemeResources() || !impl_->brushBackground || !impl_->brushItem || !impl_->brushItemHot ||
        !impl_->brushItemActive || !impl_->brushPopupPanel || !impl_->brushPopupItem || !impl_->brushPopupItemHot ||
        !impl_->brushPopupItemChecked || !impl_->penSeparator || !impl_->penPopupBorder || !impl_->font) {
        Destroy();
        return false;
    }
    SendMessageW(popupList_, WM_SETFONT, reinterpret_cast<WPARAM>(impl_->font), TRUE);
    SetWindowSubclass(parentHwnd_, Impl::ParentSubclassProc, reinterpret_cast<UINT_PTR>(this), reinterpret_cast<DWORD_PTR>(impl_.get()));
    return true;
}

void Toolbar::Destroy() {
    if (parentHwnd_) {
        RemoveWindowSubclass(parentHwnd_, Impl::ParentSubclassProc, reinterpret_cast<UINT_PTR>(this));
    }
    if (popupList_) {
        RemoveWindowSubclass(popupList_, Impl::PopupListSubclassProc, reinterpret_cast<UINT_PTR>(this));
        DestroyWindow(popupList_);
        popupList_ = nullptr;
    }
    if (popupHost_) {
        DestroyWindow(popupHost_);
        popupHost_ = nullptr;
    }
    if (toolbarHwnd_) {
        if (GetCapture() == toolbarHwnd_) {
            ReleaseCapture();
        }
        DestroyWindow(toolbarHwnd_);
        toolbarHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
    impl_->items.clear();
    impl_->itemRects.clear();
    impl_->itemVisible.clear();
    impl_->overflowIndices.clear();
    impl_->overflowRect = RECT{0, 0, 0, 0};
    impl_->popupEntries.clear();
    impl_->popupSourceIndex = -1;
    impl_->popupFromOverflow = false;
    impl_->suppressPopupToggleIndex = -2;
    impl_->suppressPopupToggleOverflow = false;
    impl_->suppressHotIndex = -2;
    impl_->suppressHotOverflow = false;
    impl_->hotIndex = -1;
    impl_->pressedIndex = -1;
    impl_->layoutDirty = true;
}

void Toolbar::SetTheme(const Theme& theme) {
    const Theme previous = theme_;
    theme_ = theme;
    if (!impl_->UpdateThemeResources()) {
        theme_ = previous;
        impl_->UpdateThemeResources();
    }
}

void Toolbar::SetItems(const std::vector<ToolbarItem>& items) {
    impl_->HidePopup();
    impl_->items = items;
    impl_->layoutDirty = true;
    if (toolbarHwnd_) {
        InvalidateRect(toolbarHwnd_, nullptr, TRUE);
    }
}

void Toolbar::AddItem(const ToolbarItem& item) {
    impl_->items.push_back(item);
    impl_->layoutDirty = true;
    if (toolbarHwnd_) {
        InvalidateRect(toolbarHwnd_, nullptr, TRUE);
    }
}

void Toolbar::ClearItems() {
    impl_->HidePopup();
    impl_->items.clear();
    impl_->itemRects.clear();
    impl_->itemVisible.clear();
    impl_->overflowIndices.clear();
    impl_->layoutDirty = true;
    if (toolbarHwnd_) {
        InvalidateRect(toolbarHwnd_, nullptr, TRUE);
    }
}

void Toolbar::SetItem(int index, const ToolbarItem& item) {
    if (index < 0 || index >= static_cast<int>(impl_->items.size())) {
        return;
    }
    impl_->items[index] = item;
    impl_->layoutDirty = true;
    if (toolbarHwnd_) {
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }
}

void Toolbar::SetChecked(int index, bool checked) {
    if (index < 0 || index >= static_cast<int>(impl_->items.size())) {
        return;
    }
    impl_->items[index].checked = checked;
    if (toolbarHwnd_) {
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }
}

void Toolbar::SetDisabled(int index, bool disabled) {
    if (index < 0 || index >= static_cast<int>(impl_->items.size())) {
        return;
    }
    impl_->items[index].disabled = disabled;
    if (toolbarHwnd_) {
        InvalidateRect(toolbarHwnd_, nullptr, FALSE);
    }
}

std::size_t Toolbar::GetCount() const {
    return impl_->items.size();
}

ToolbarItem Toolbar::GetItem(int index) const {
    if (index < 0 || index >= static_cast<int>(impl_->items.size())) {
        return {};
    }
    return impl_->items[index];
}

}  // namespace darkui
