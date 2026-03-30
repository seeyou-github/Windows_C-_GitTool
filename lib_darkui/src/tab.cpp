#include "darkui/tab.h"

#include <algorithm>
#include <commctrl.h>
#include <windowsx.h>

namespace darkui {
namespace {

constexpr wchar_t kTabClassName[] = L"DarkUiTabControl";

ATOM EnsureTabClassRegistered(HINSTANCE instance);
LRESULT CALLBACK TabWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

}  // namespace

struct Tab::Impl {
    Tab* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushBackground = nullptr;
    HBRUSH brushContentBackground = nullptr;
    HBRUSH brushItem = nullptr;
    HBRUSH brushItemActive = nullptr;
    HFONT font = nullptr;
    std::vector<TabItem> items;
    std::vector<HWND> pages;
    int selection = -1;
    int hotIndex = -1;
    bool trackingMouseLeave = false;

    explicit Impl(Tab* tab) : owner(tab) {}

    ~Impl() {
        if (brushBackground) DeleteObject(brushBackground);
        if (brushContentBackground) DeleteObject(brushContentBackground);
        if (brushItem) DeleteObject(brushItem);
        if (brushItemActive) DeleteObject(brushItemActive);
        if (font) DeleteObject(font);
    }

    void UpdateThemeResources() {
        if (brushBackground) DeleteObject(brushBackground);
        if (brushContentBackground) DeleteObject(brushContentBackground);
        if (brushItem) DeleteObject(brushItem);
        if (brushItemActive) DeleteObject(brushItemActive);
        if (font) DeleteObject(font);

        brushBackground = CreateSolidBrush(owner->theme_.tabBackground);
        brushContentBackground = CreateSolidBrush(owner->theme_.background);
        brushItem = CreateSolidBrush(owner->theme_.tabItem);
        brushItemActive = CreateSolidBrush(owner->theme_.tabItemActive);
        font = CreateFont(owner->theme_.uiFont);

        if (owner->tabHwnd_) {
            InvalidateRect(owner->tabHwnd_, nullptr, TRUE);
        }
    }

    int SafeSelection() const {
        if (items.empty()) {
            return -1;
        }
        return std::clamp(selection, 0, static_cast<int>(items.size()) - 1);
    }

    RECT TabStripRect() const {
        RECT client{};
        GetClientRect(owner->tabHwnd_, &client);
        if (owner->vertical_) {
            client.right = client.left + std::max(96, owner->theme_.tabWidth);
        } else {
            client.bottom = client.top + std::max(24, owner->theme_.tabHeight);
        }
        return client;
    }

    RECT ContentRect() const {
        RECT client{};
        GetClientRect(owner->tabHwnd_, &client);
        if (owner->vertical_) {
            client.left += std::max(96, owner->theme_.tabWidth);
        } else {
            client.top += std::max(24, owner->theme_.tabHeight);
        }
        return client;
    }

    RECT ItemRect(int index) const {
        RECT strip = TabStripRect();
        if (owner->vertical_) {
            const int itemHeight = std::max(32, owner->theme_.tabHeight);
            RECT rc{
                strip.left,
                strip.top + index * itemHeight,
                strip.right,
                std::min(static_cast<int>(strip.bottom), static_cast<int>(strip.top + (index + 1) * itemHeight))
            };
            return rc;
        } else {
            const int count = std::max(1, static_cast<int>(items.size()));
            const int width = std::max(1, static_cast<int>(strip.right - strip.left) / count);
            RECT rc{strip.left + index * width, strip.top, strip.left + (index + 1) * width, strip.bottom};
            if (index == count - 1) {
                rc.right = strip.right;
            }
            return rc;
        }
    }

    int HitTest(POINT point) const {
        if (items.empty()) {
            return -1;
        }
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const RECT rc = ItemRect(i);
            if (PtInRect(&rc, point)) {
                return i;
            }
        }
        return -1;
    }

    void UpdatePageVisibility() {
        const RECT content = ContentRect();
        const int selected = SafeSelection();
        for (int i = 0; i < static_cast<int>(pages.size()); ++i) {
            HWND page = pages[i];
            if (!page) {
                continue;
            }
            MoveWindow(page,
                       content.left,
                       content.top,
                       std::max(0, static_cast<int>(content.right - content.left)),
                       std::max(0, static_cast<int>(content.bottom - content.top)),
                       FALSE);
            ShowWindow(page, i == selected ? SW_SHOW : SW_HIDE);
            if (i == selected) {
                RedrawWindow(page, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            }
        }
    }

    void InvalidateStrip() {
        if (!owner->tabHwnd_) {
            return;
        }
        RECT strip = TabStripRect();
        InvalidateRect(owner->tabHwnd_, &strip, FALSE);
    }

    void InvalidateItem(int index) {
        if (!owner->tabHwnd_ || index < 0 || index >= static_cast<int>(items.size())) {
            return;
        }
        RECT rc = ItemRect(index);
        InvalidateRect(owner->tabHwnd_, &rc, FALSE);
    }

    void NotifySelectionChanged() {
        if (!owner->parentHwnd_ || !owner->tabHwnd_) {
            return;
        }
        NMHDR hdr{};
        hdr.hwndFrom = owner->tabHwnd_;
        hdr.idFrom = static_cast<UINT_PTR>(owner->controlId_);
        hdr.code = TCN_SELCHANGE;
        SendMessageW(owner->parentHwnd_, WM_NOTIFY, hdr.idFrom, reinterpret_cast<LPARAM>(&hdr));
    }

    void StartMouseLeaveTracking() {
        if (!owner->tabHwnd_ || trackingMouseLeave) {
            return;
        }
        TRACKMOUSEEVENT track{};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = owner->tabHwnd_;
        if (TrackMouseEvent(&track)) {
            trackingMouseLeave = true;
        }
    }

    void Paint(HDC dc) {
        RECT client{};
        GetClientRect(owner->tabHwnd_, &client);
        FillRect(dc, &client, brushBackground ? brushBackground : reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

        RECT content = ContentRect();
        FillRect(dc, &content, brushContentBackground ? brushContentBackground : brushBackground);

        HFONT oldFont = font ? reinterpret_cast<HFONT>(SelectObject(dc, font)) : nullptr;
        SetBkMode(dc, TRANSPARENT);

        const int selected = SafeSelection();
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            RECT rc = ItemRect(i);
            const bool active = i == selected;
            const bool hot = i == hotIndex;
            HBRUSH fill = active ? brushItemActive : brushItem;
            FillRect(dc, &rc, fill ? fill : reinterpret_cast<HBRUSH>(GetStockObject(DKGRAY_BRUSH)));

            if (!active && hot) {
                HBRUSH overlay = CreateSolidBrush(owner->theme_.buttonHover);
                FillRect(dc, &rc, overlay);
                DeleteObject(overlay);
            }

            rc.left += owner->theme_.textPadding;
            rc.right -= owner->theme_.textPadding;
            SetTextColor(dc, active ? owner->theme_.tabTextActive : owner->theme_.tabText);
            DrawTextW(dc, items[i].text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        }

        if (oldFont) {
            SelectObject(dc, oldFont);
        }
    }

    static LRESULT CALLBACK TabWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
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
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, self->owner->theme_.text);
            SetBkColor(dc, self->owner->theme_.background);
            return reinterpret_cast<LRESULT>(self->brushContentBackground ? self->brushContentBackground : self->brushBackground);
        }
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
            self->UpdatePageVisibility();
            self->InvalidateStrip();
            return 0;
        case WM_MOUSEMOVE: {
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const int index = self->HitTest(pt);
            if (self->hotIndex != index) {
                const int previous = self->hotIndex;
                self->hotIndex = index;
                self->InvalidateItem(previous);
                self->InvalidateItem(index);
            }
            self->StartMouseLeaveTracking();
            return 0;
        }
        case WM_MOUSELEAVE:
            self->trackingMouseLeave = false;
            self->InvalidateItem(self->hotIndex);
            self->hotIndex = -1;
            return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(window);
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const int index = self->HitTest(pt);
            if (index >= 0) {
                self->owner->SetSelection(index, true);
            }
            return 0;
        }
        case WM_KEYDOWN:
            if ((self->owner->vertical_ && wParam == VK_UP) || (!self->owner->vertical_ && wParam == VK_LEFT)) {
                self->owner->SetSelection(self->selection - 1, true);
                return 0;
            }
            if ((self->owner->vertical_ && wParam == VK_DOWN) || (!self->owner->vertical_ && wParam == VK_RIGHT)) {
                self->owner->SetSelection(self->selection + 1, true);
                return 0;
            }
            if (wParam == VK_HOME) {
                self->owner->SetSelection(0, true);
                return 0;
            }
            if (wParam == VK_END) {
                self->owner->SetSelection(static_cast<int>(self->items.size()) - 1, true);
                return 0;
            }
            break;
        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
};

namespace {

ATOM EnsureTabClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = TabWindowProcThunk;
    wc.hInstance = instance;
    wc.lpszClassName = kTabClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

LRESULT CALLBACK TabWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return Tab::Impl::TabWindowProc(window, message, wParam, lParam);
}

}  // namespace

Tab::Tab() : impl_(std::make_unique<Impl>(this)) {}

Tab::~Tab() {
    Destroy();
}

bool Tab::Create(HWND parent, int controlId, const Theme& theme, DWORD style, DWORD exStyle) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    theme_ = theme;
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }

    if (!EnsureTabClassRegistered(impl_->instance)) {
        Destroy();
        return false;
    }

    tabHwnd_ = CreateWindowExW(exStyle,
                               kTabClassName,
                               L"",
                               style | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                               0,
                               0,
                               0,
                               0,
                               parent,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                               impl_->instance,
                               impl_.get());
    if (!tabHwnd_) {
        Destroy();
        return false;
    }

    impl_->UpdateThemeResources();
    if (!impl_->brushBackground || !impl_->brushContentBackground || !impl_->brushItem || !impl_->brushItemActive || !impl_->font) {
        Destroy();
        return false;
    }
    return true;
}

void Tab::Destroy() {
    if (tabHwnd_) {
        DestroyWindow(tabHwnd_);
        tabHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
}

void Tab::SetTheme(const Theme& theme) {
    theme_ = theme;
    impl_->UpdateThemeResources();
}

void Tab::SetVertical(bool enabled) {
    vertical_ = enabled;
    impl_->UpdatePageVisibility();
    if (tabHwnd_) {
        InvalidateRect(tabHwnd_, nullptr, TRUE);
    }
}

void Tab::SetItems(const std::vector<TabItem>& items) {
    std::vector<HWND> oldPages = std::move(impl_->pages);
    impl_->items = items;
    impl_->pages.assign(items.size(), nullptr);
    for (std::size_t i = 0; i < impl_->pages.size() && i < oldPages.size(); ++i) {
        impl_->pages[i] = oldPages[i];
    }
    impl_->selection = items.empty() ? -1 : std::clamp(impl_->selection, 0, static_cast<int>(items.size()) - 1);
    if (impl_->selection < 0 && !items.empty()) {
        impl_->selection = 0;
    }
    impl_->UpdatePageVisibility();
    if (tabHwnd_) {
        InvalidateRect(tabHwnd_, nullptr, TRUE);
    }
}

void Tab::AddItem(const TabItem& item) {
    impl_->items.push_back(item);
    impl_->pages.push_back(nullptr);
    if (impl_->selection < 0) {
        impl_->selection = 0;
    }
    if (tabHwnd_) {
        InvalidateRect(tabHwnd_, nullptr, TRUE);
    }
}

void Tab::ClearItems() {
    for (HWND page : impl_->pages) {
        if (page) {
            ShowWindow(page, SW_HIDE);
        }
    }
    impl_->items.clear();
    impl_->pages.clear();
    impl_->selection = -1;
    if (tabHwnd_) {
        InvalidateRect(tabHwnd_, nullptr, TRUE);
    }
}

void Tab::AttachPage(int index, HWND page) {
    if (index < 0 || index >= static_cast<int>(impl_->pages.size())) {
        return;
    }
    impl_->pages[index] = page;
    impl_->UpdatePageVisibility();
}

void Tab::SetSelection(int index, bool notify) {
    if (impl_->items.empty()) {
        impl_->selection = -1;
        return;
    }
    const int clamped = std::clamp(index, 0, static_cast<int>(impl_->items.size()) - 1);
    if (impl_->selection == clamped) {
        return;
    }
    impl_->selection = clamped;
    impl_->UpdatePageVisibility();
    if (tabHwnd_) {
        impl_->InvalidateStrip();
    }
    if (notify) {
        impl_->NotifySelectionChanged();
    }
}

int Tab::GetSelection() const {
    return impl_->SafeSelection();
}

std::size_t Tab::GetCount() const {
    return impl_->items.size();
}

TabItem Tab::GetItem(int index) const {
    if (index < 0 || index >= static_cast<int>(impl_->items.size())) {
        return {};
    }
    return impl_->items[index];
}

RECT Tab::GetContentRect() const {
    if (!tabHwnd_) {
        return RECT{0, 0, 0, 0};
    }
    return impl_->ContentRect();
}

}  // namespace darkui
