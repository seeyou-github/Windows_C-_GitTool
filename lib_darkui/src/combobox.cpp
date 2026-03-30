#include "darkui/combobox.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <algorithm>

namespace darkui {
namespace {

constexpr wchar_t kPopupClassName[] = L"DarkUiComboPopupHost";
constexpr int kPopupListId = 0x5D11;

void StripClientEdge(HWND window) {
    const LONG_PTR exStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    const LONG_PTR newStyle = exStyle & ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE) & ~static_cast<LONG_PTR>(WS_EX_STATICEDGE);
    if (newStyle != exStyle) {
        SetWindowLongPtrW(window, GWL_EXSTYLE, newStyle);
        SetWindowPos(window, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

ATOM EnsurePopupClassRegistered(HINSTANCE instance, WNDPROC proc) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = instance;
    wc.lpszClassName = kPopupClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

}  // namespace

HFONT CreateFont(const FontSpec& spec) {
    return ::CreateFontW(spec.height,
                         0,
                         0,
                         0,
                         spec.weight,
                         spec.italic ? TRUE : FALSE,
                         FALSE,
                         FALSE,
                         DEFAULT_CHARSET,
                         OUT_OUTLINE_PRECIS,
                         CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY,
                         spec.monospace ? FIXED_PITCH : VARIABLE_PITCH,
                         spec.family.c_str());
}

struct ComboBox::Impl {
    ComboBox* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushPanel = nullptr;
    HBRUSH brushButton = nullptr;
    HBRUSH brushButtonHot = nullptr;
    HBRUSH brushBorder = nullptr;
    HBRUSH brushPopupItem = nullptr;
    HBRUSH brushPopupItemHot = nullptr;
    HBRUSH brushPopupAccentItem = nullptr;
    HBRUSH brushPopupAccentItemHot = nullptr;
    HBRUSH brushText = nullptr;
    HBRUSH brushArrow = nullptr;
    HPEN penText = nullptr;
    HPEN penArrow = nullptr;
    HFONT font = nullptr;
    std::vector<ComboItem> items;
    int selection = -1;
    explicit Impl(ComboBox* combo) : owner(combo) {}

    ~Impl() {
        if (font) DeleteObject(font);
        if (brushPanel) DeleteObject(brushPanel);
        if (brushButton) DeleteObject(brushButton);
        if (brushButtonHot) DeleteObject(brushButtonHot);
        if (brushBorder) DeleteObject(brushBorder);
        if (brushPopupItem) DeleteObject(brushPopupItem);
        if (brushPopupItemHot) DeleteObject(brushPopupItemHot);
        if (brushPopupAccentItem) DeleteObject(brushPopupAccentItem);
        if (brushPopupAccentItemHot) DeleteObject(brushPopupAccentItemHot);
        if (brushText) DeleteObject(brushText);
        if (brushArrow) DeleteObject(brushArrow);
        if (penText) DeleteObject(penText);
        if (penArrow) DeleteObject(penArrow);
    }

    void UpdateThemeResources() {
        if (font) DeleteObject(font);
        if (brushPanel) DeleteObject(brushPanel);
        if (brushButton) DeleteObject(brushButton);
        if (brushButtonHot) DeleteObject(brushButtonHot);
        if (brushBorder) DeleteObject(brushBorder);
        if (brushPopupItem) DeleteObject(brushPopupItem);
        if (brushPopupItemHot) DeleteObject(brushPopupItemHot);
        if (brushPopupAccentItem) DeleteObject(brushPopupAccentItem);
        if (brushPopupAccentItemHot) DeleteObject(brushPopupAccentItemHot);
        if (brushText) DeleteObject(brushText);
        if (brushArrow) DeleteObject(brushArrow);
        if (penText) DeleteObject(penText);
        if (penArrow) DeleteObject(penArrow);
        font = CreateFont(owner->theme_.uiFont);
        brushPanel = CreateSolidBrush(owner->theme_.panel);
        brushButton = CreateSolidBrush(owner->theme_.button);
        brushButtonHot = CreateSolidBrush(owner->theme_.buttonHot);
        brushBorder = CreateSolidBrush(owner->theme_.border);
        brushPopupItem = CreateSolidBrush(owner->theme_.popupItem);
        brushPopupItemHot = CreateSolidBrush(owner->theme_.popupItemHot);
        brushPopupAccentItem = CreateSolidBrush(owner->theme_.popupAccentItem);
        brushPopupAccentItemHot = CreateSolidBrush(owner->theme_.popupAccentItemHot);
        brushText = CreateSolidBrush(owner->theme_.text);
        brushArrow = CreateSolidBrush(owner->theme_.arrow);
        penText = CreatePen(PS_SOLID, 1, owner->theme_.text);
        penArrow = CreatePen(PS_SOLID, 1, owner->theme_.arrow);
        if (owner->comboHwnd_) {
            SendMessageW(owner->comboHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            InvalidateRect(owner->comboHwnd_, nullptr, TRUE);
        }
        if (owner->popupList_) {
            SendMessageW(owner->popupList_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            InvalidateRect(owner->popupList_, nullptr, TRUE);
        }
    }

    HFONT SelectControlFont(HDC dc, HWND control) {
        HFONT current = reinterpret_cast<HFONT>(SendMessageW(control, WM_GETFONT, 0, 0));
        return current ? reinterpret_cast<HFONT>(SelectObject(dc, current)) : nullptr;
    }

    void RefreshText() {
        const std::wstring text = (selection >= 0 && selection < static_cast<int>(items.size())) ? items[selection].text : L"";
        SetWindowTextW(owner->comboHwnd_, text.c_str());
        InvalidateRect(owner->comboHwnd_, nullptr, TRUE);
    }

    void NotifySelection() {
        SendMessageW(owner->parentHwnd_,
                     WM_COMMAND,
                     MAKEWPARAM(static_cast<UINT>(owner->controlId_), CBN_SELCHANGE),
                     reinterpret_cast<LPARAM>(owner->comboHwnd_));
    }

    void HidePopup() {
        if (owner->popupHost_) {
            ShowWindow(owner->popupHost_, SW_HIDE);
        }
    }

    bool IsPopupVisible() const {
        return owner->popupHost_ && IsWindowVisible(owner->popupHost_);
    }

    void RepositionPopup() {
        if (!owner->comboHwnd_ || !owner->popupHost_ || !owner->popupList_) {
            return;
        }
        RECT comboRect{};
        GetWindowRect(owner->comboHwnd_, &comboRect);
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        RECT workArea = comboRect;
        if (const HMONITOR monitor = MonitorFromRect(&comboRect, MONITOR_DEFAULTTONEAREST);
            monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
            workArea = monitorInfo.rcWork;
        }

        POINT popupOrigin{comboRect.left, comboRect.bottom + owner->theme_.popupOffsetY};
        const int desiredRows = std::max(1, static_cast<int>(items.size()));
        const int desiredHeight = std::max(owner->theme_.itemHeight,
                                           desiredRows * owner->theme_.itemHeight + owner->theme_.popupBorder * 2);
        const int availableHeight = std::max(owner->theme_.itemHeight,
                                             static_cast<int>(workArea.bottom - popupOrigin.y - 8));
        const int hostHeight = std::max(owner->theme_.itemHeight,
                                        std::min(desiredHeight, std::max(owner->theme_.itemHeight, availableHeight)));
        const int width = comboRect.right - comboRect.left;
        popupOrigin.x = std::max(workArea.left, std::min(popupOrigin.x, workArea.right - width));
        popupOrigin.y = std::max(workArea.top, std::min(popupOrigin.y, workArea.bottom - hostHeight));

        SetWindowPos(owner->popupHost_,
                     HWND_TOPMOST,
                     popupOrigin.x,
                     popupOrigin.y,
                     width,
                     hostHeight,
                     SWP_SHOWWINDOW | SWP_NOACTIVATE);
        MoveWindow(owner->popupList_,
                   owner->theme_.popupBorder,
                   owner->theme_.popupBorder,
                   std::max(1, width - owner->theme_.popupBorder * 2),
                   std::max(1, hostHeight - owner->theme_.popupBorder * 2),
                   TRUE);
        RedrawWindow(owner->popupHost_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
    }

    void ShowPopup() {
        SendMessageW(owner->popupList_, LB_RESETCONTENT, 0, 0);
        for (const auto& item : items) {
            SendMessageW(owner->popupList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.text.c_str()));
        }
        if (selection >= 0) {
            SendMessageW(owner->popupList_, LB_SETCURSEL, selection, 0);
            SendMessageW(owner->popupList_, LB_SETTOPINDEX, selection, 0);
        }
        RepositionPopup();
    }

    void TogglePopup() {
        if (IsPopupVisible()) {
            HidePopup();
        } else {
            ShowPopup();
        }
    }

    void ApplyPopupSelection() {
        const int selected = static_cast<int>(SendMessageW(owner->popupList_, LB_GETCURSEL, 0, 0));
        if (selected >= 0) {
            owner->SetSelection(selected, true);
        }
        HidePopup();
    }

    void CloseOnOutsideClick(POINT pt) {
        if (!IsPopupVisible()) {
            return;
        }
        RECT popupRect{};
        GetWindowRect(owner->popupHost_, &popupRect);
        RECT comboRect{};
        GetWindowRect(owner->comboHwnd_, &comboRect);
        if (!PtInRect(&popupRect, pt) && !PtInRect(&comboRect, pt)) {
            HidePopup();
        }
    }

    static LRESULT CALLBACK ComboSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData) {
        auto* self = reinterpret_cast<Impl*>(refData);
        if (!self) {
            return DefSubclassProc(window, message, wParam, lParam);
        }

        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            self->TogglePopup();
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_SPACE || wParam == VK_RETURN || wParam == VK_DOWN || wParam == VK_F4) {
                self->TogglePopup();
                return 0;
            }
            if (wParam == VK_ESCAPE && self->IsPopupVisible()) {
                self->HidePopup();
                return 0;
            }
            break;
        case WM_DESTROY:
            RemoveWindowSubclass(window, ComboSubclassProc, subclassId);
            break;
        default:
            break;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }

    static LRESULT CALLBACK PopupWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            if (self) {
                PAINTSTRUCT ps{};
                HDC dc = BeginPaint(window, &ps);
                RECT rect{};
                GetClientRect(window, &rect);
                FillRect(dc, &rect, self->brushPanel);
                FrameRect(dc, &rect, self->brushBorder);
                EndPaint(window, &ps);
                return 0;
            }
            break;
        case WM_CTLCOLORLISTBOX:
            if (self) {
                SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
                SetTextColor(reinterpret_cast<HDC>(wParam), self->owner->theme_.text);
                SetBkColor(reinterpret_cast<HDC>(wParam), self->owner->theme_.panel);
                return reinterpret_cast<LRESULT>(self->brushPanel);
            }
            break;
        case WM_MEASUREITEM:
            if (self) {
                auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
                if (measure && measure->CtlID == kPopupListId) {
                    measure->itemHeight = self->owner->theme_.itemHeight;
                    return TRUE;
                }
            }
            break;
        case WM_DRAWITEM:
            if (self) {
                auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
                if (draw && draw->hwndItem == self->owner->popupList_) {
                    self->DrawPopupListItem(draw);
                    return TRUE;
                }
            }
            break;
        case WM_COMMAND:
            if (self && LOWORD(wParam) == kPopupListId && (HIWORD(wParam) == LBN_SELCHANGE || HIWORD(wParam) == LBN_DBLCLK)) {
                self->ApplyPopupSelection();
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (self) {
                if (wParam == VK_RETURN) {
                    self->ApplyPopupSelection();
                    return 0;
                }
                if (wParam == VK_ESCAPE) {
                    self->HidePopup();
                    return 0;
                }
            }
            break;
        default:
            break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    static LRESULT CALLBACK ParentSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData) {
        auto* self = reinterpret_cast<Impl*>(refData);
        if (!self) {
            return DefSubclassProc(window, message, wParam, lParam);
        }

        switch (message) {
        case WM_DRAWITEM: {
            auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (draw && draw->hwndItem == self->owner->comboHwnd_) {
                self->DrawComboButton(draw);
                return TRUE;
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            POINT pt{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
            ClientToScreen(window, &pt);
            self->CloseOnOutsideClick(pt);
            break;
        }
        case WM_PARENTNOTIFY:
            switch (LOWORD(wParam)) {
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDBLCLK: {
                const DWORD pos = GetMessagePos();
                POINT pt{static_cast<short>(LOWORD(pos)), static_cast<short>(HIWORD(pos))};
                self->CloseOnOutsideClick(pt);
                break;
            }
            default:
                break;
            }
            break;
        case WM_NCLBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            self->HidePopup();
            break;
        case WM_SIZE:
        case WM_MOVE:
        case WM_MOVING:
            if (self->IsPopupVisible()) {
                self->RepositionPopup();
            }
            break;
        case WM_DESTROY:
            RemoveWindowSubclass(window, ParentSubclassProc, subclassId);
            break;
        default:
            break;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }

    void DrawComboButton(const DRAWITEMSTRUCT* draw) {
        const bool selected = (draw->itemState & ODS_SELECTED) != 0;
        FillRect(draw->hDC, &draw->rcItem, selected ? brushButtonHot : brushPanel);
        FrameRect(draw->hDC, &draw->rcItem, brushBorder);
        HFONT oldFont = SelectControlFont(draw->hDC, draw->hwndItem);
        SetBkMode(draw->hDC, TRANSPARENT);
        SetTextColor(draw->hDC, owner->theme_.text);

        wchar_t text[1024] = {};
        GetWindowTextW(draw->hwndItem, text, 1024);
        RECT textRect = draw->rcItem;
        textRect.left += owner->theme_.textPadding;
        textRect.right -= owner->theme_.arrowWidth + owner->theme_.arrowRightPadding + 6;
        DrawTextW(draw->hDC, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

        RECT arrowRect = draw->rcItem;
        arrowRect.right -= owner->theme_.arrowRightPadding;
        arrowRect.left = arrowRect.right - owner->theme_.arrowWidth;
        const int centerY = arrowRect.top + ((arrowRect.bottom - arrowRect.top) / 2);
        POINT pts[3] = {
            {arrowRect.left, centerY - 2},
            {arrowRect.right, centerY - 2},
            {(arrowRect.left + arrowRect.right) / 2, centerY + 4},
        };
        HGDIOBJ oldBrush = SelectObject(draw->hDC, brushArrow);
        HGDIOBJ oldPen = SelectObject(draw->hDC, penArrow);
        Polygon(draw->hDC, pts, 3);
        SelectObject(draw->hDC, oldPen);
        SelectObject(draw->hDC, oldBrush);
        if (oldFont) {
            SelectObject(draw->hDC, oldFont);
        }
    }

    void DrawPopupListItem(const DRAWITEMSTRUCT* draw) {
        if (draw->itemID == static_cast<UINT>(-1) || draw->itemID >= items.size()) {
            return;
        }
        const ComboItem& item = items[draw->itemID];
        const bool selected = (draw->itemState & ODS_SELECTED) != 0;
        HBRUSH normalBrush = brushPopupItem;
        HBRUSH hotBrush = brushPopupItemHot;
        HBRUSH accentBrush = nullptr;
        HBRUSH accentHotBrush = nullptr;
        if (item.accent) {
            accentBrush = brushPopupAccentItem;
            accentHotBrush = brushPopupAccentItemHot;
            normalBrush = accentBrush;
            hotBrush = accentHotBrush;
        }
        FillRect(draw->hDC, &draw->rcItem, selected ? hotBrush : normalBrush);
        HFONT oldFont = SelectControlFont(draw->hDC, draw->hwndItem);
        SetBkMode(draw->hDC, TRANSPARENT);
        SetTextColor(draw->hDC, owner->theme_.text);
        const int length = static_cast<int>(SendMessageW(draw->hwndItem, LB_GETTEXTLEN, draw->itemID, 0));
        std::wstring text(length + 1, L'\0');
        SendMessageW(draw->hwndItem, LB_GETTEXT, draw->itemID, reinterpret_cast<LPARAM>(text.data()));
        text.resize(length);
        RECT rect = draw->rcItem;
        rect.left += owner->theme_.textPadding;
        rect.right -= owner->theme_.textPadding;
        DrawTextW(draw->hDC, text.c_str(), -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        if (oldFont) {
            SelectObject(draw->hDC, oldFont);
        }
    }
};

ComboBox::ComboBox() : impl_(std::make_unique<Impl>(this)) {}

ComboBox::~ComboBox() {
    Destroy();
}

bool ComboBox::Create(HWND parent, int controlId, const Theme& theme, DWORD style, DWORD exStyle) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    theme_ = theme;
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }
    impl_->UpdateThemeResources();

    style |= BS_OWNERDRAW;
    comboHwnd_ = CreateWindowExW(exStyle,
                                 L"BUTTON",
                                 L"",
                                 style,
                                 0,
                                 0,
                                 0,
                                 0,
                                 parent,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                 impl_->instance,
                                 nullptr);
    if (!comboHwnd_) {
        Destroy();
        return false;
    }
    SendMessageW(comboHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(impl_->font), TRUE);

    EnsurePopupClassRegistered(impl_->instance, Impl::PopupWindowProc);
    popupHost_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                 kPopupClassName,
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
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                                 0,
                                 0,
                                 0,
                                 0,
                                 popupHost_,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPopupListId)),
                                 impl_->instance,
                                 nullptr);
    if (!popupList_) {
        Destroy();
        return false;
    }
    SendMessageW(popupList_, WM_SETFONT, reinterpret_cast<WPARAM>(impl_->font), TRUE);
    SetWindowTheme(popupList_, L"", L"");
    StripClientEdge(popupList_);
    SetWindowSubclass(comboHwnd_,
                      Impl::ComboSubclassProc,
                      reinterpret_cast<UINT_PTR>(this),
                      reinterpret_cast<DWORD_PTR>(impl_.get()));

    SetWindowSubclass(parentHwnd_,
                      Impl::ParentSubclassProc,
                      reinterpret_cast<UINT_PTR>(this),
                      reinterpret_cast<DWORD_PTR>(impl_.get()));
    return true;
}

void ComboBox::Destroy() {
    if (parentHwnd_) {
        RemoveWindowSubclass(parentHwnd_, Impl::ParentSubclassProc, reinterpret_cast<UINT_PTR>(this));
    }
    if (comboHwnd_) {
        RemoveWindowSubclass(comboHwnd_, Impl::ComboSubclassProc, reinterpret_cast<UINT_PTR>(this));
    }
    if (popupList_) {
        DestroyWindow(popupList_);
        popupList_ = nullptr;
    }
    if (popupHost_) {
        DestroyWindow(popupHost_);
        popupHost_ = nullptr;
    }
    if (comboHwnd_) {
        DestroyWindow(comboHwnd_);
        comboHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
    impl_->items.clear();
    impl_->selection = -1;
}

void ComboBox::SetTheme(const Theme& theme) {
    theme_ = theme;
    impl_->UpdateThemeResources();
}

void ComboBox::SetItems(const std::vector<ComboItem>& items) {
    impl_->items = items;
    impl_->selection = items.empty() ? -1 : 0;
    if (comboHwnd_) {
        impl_->RefreshText();
    }
}

void ComboBox::AddItem(const ComboItem& item) {
    impl_->items.push_back(item);
    if (impl_->selection < 0) {
        impl_->selection = 0;
    }
    if (comboHwnd_) {
        impl_->RefreshText();
    }
}

void ComboBox::ClearItems() {
    impl_->items.clear();
    impl_->selection = -1;
    if (comboHwnd_) {
        impl_->RefreshText();
    }
}

int ComboBox::GetSelection() const {
    return impl_->selection;
}

void ComboBox::SetSelection(int index, bool notify) {
    if (index < 0 || index >= static_cast<int>(impl_->items.size())) {
        impl_->selection = -1;
    } else {
        impl_->selection = index;
    }
    if (comboHwnd_) {
        impl_->RefreshText();
    }
    if (notify && parentHwnd_) {
        impl_->NotifySelection();
    }
}

std::size_t ComboBox::GetCount() const {
    return impl_->items.size();
}

std::wstring ComboBox::GetText() const {
    return (impl_->selection >= 0 && impl_->selection < static_cast<int>(impl_->items.size())) ? impl_->items[impl_->selection].text : L"";
}

ComboItem ComboBox::GetItem(int index) const {
    if (index < 0 || index >= static_cast<int>(impl_->items.size())) {
        return {};
    }
    return impl_->items[index];
}

}  // namespace darkui
