#include "darkui/scrollbar.h"

#include <windowsx.h>

#include <algorithm>

namespace darkui {
namespace {

constexpr wchar_t kScrollBarClassName[] = L"DarkUiScrollBarControl";

ATOM EnsureScrollBarClassRegistered(HINSTANCE instance);
LRESULT CALLBACK ScrollBarWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

}  // namespace

struct ScrollBar::Impl {
    ScrollBar* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushBackground = nullptr;
    HBRUSH brushTrack = nullptr;
    HBRUSH brushThumb = nullptr;
    HBRUSH brushThumbHot = nullptr;
    bool hot = false;
    bool trackingMouseLeave = false;
    bool dragging = false;
    int dragOffset = 0;

    explicit Impl(ScrollBar* scrollBar) : owner(scrollBar) {}

    ~Impl() {
        if (brushBackground) DeleteObject(brushBackground);
        if (brushTrack) DeleteObject(brushTrack);
        if (brushThumb) DeleteObject(brushThumb);
        if (brushThumbHot) DeleteObject(brushThumbHot);
    }

    void UpdateThemeResources() {
        if (brushBackground) DeleteObject(brushBackground);
        if (brushTrack) DeleteObject(brushTrack);
        if (brushThumb) DeleteObject(brushThumb);
        if (brushThumbHot) DeleteObject(brushThumbHot);

        brushBackground = CreateSolidBrush(owner->theme_.scrollBarBackground);
        brushTrack = CreateSolidBrush(owner->theme_.scrollBarTrack);
        brushThumb = CreateSolidBrush(owner->theme_.scrollBarThumb);
        brushThumbHot = CreateSolidBrush(owner->theme_.scrollBarThumbHot);

        if (owner->scrollBarHwnd_) {
            InvalidateRect(owner->scrollBarHwnd_, nullptr, TRUE);
        }
    }

    int ClampValue(int value) const {
        const int maxValue = std::max(owner->minimum_, owner->maximum_ - std::max(0, owner->pageSize_));
        return std::clamp(value, owner->minimum_, maxValue);
    }

    RECT GetTrackRect() const {
        RECT client{};
        GetClientRect(owner->scrollBarHwnd_, &client);
        RECT track = client;
        const int inset = 2;
        if (owner->vertical_) {
            track.left += inset;
            track.right -= inset;
        } else {
            track.top += inset;
            track.bottom -= inset;
        }
        if (track.right <= track.left) {
            track.right = track.left + 1;
        }
        if (track.bottom <= track.top) {
            track.bottom = track.top + 1;
        }
        return track;
    }

    int TrackLength(const RECT& track) const {
        return owner->vertical_ ? static_cast<int>(track.bottom - track.top) : static_cast<int>(track.right - track.left);
    }

    int ThumbLength(const RECT& track) const {
        const int trackLength = std::max(1, TrackLength(track));
        const int range = std::max(1, owner->maximum_ - owner->minimum_);
        if (owner->pageSize_ <= 0) {
            return std::max(owner->theme_.scrollBarMinThumbSize, trackLength / 6);
        }
        const int size = static_cast<int>((static_cast<double>(owner->pageSize_) / static_cast<double>(range + owner->pageSize_)) * trackLength);
        return std::clamp(size, owner->theme_.scrollBarMinThumbSize, trackLength);
    }

    int ThumbOffset(const RECT& track) const {
        const int trackLength = std::max(1, TrackLength(track));
        const int thumbLength = ThumbLength(track);
        const int movable = std::max(1, trackLength - thumbLength);
        const int maxValue = std::max(owner->minimum_, owner->maximum_ - std::max(0, owner->pageSize_));
        if (maxValue <= owner->minimum_) {
            return 0;
        }
        const double ratio = static_cast<double>(owner->value_ - owner->minimum_) /
                             static_cast<double>(maxValue - owner->minimum_);
        return static_cast<int>(movable * ratio + 0.5);
    }

    RECT GetThumbRect() const {
        const RECT track = GetTrackRect();
        RECT thumb = track;
        const int offset = ThumbOffset(track);
        const int length = ThumbLength(track);
        if (owner->vertical_) {
            thumb.top += offset;
            thumb.bottom = thumb.top + length;
        } else {
            thumb.left += offset;
            thumb.right = thumb.left + length;
        }
        return thumb;
    }

    int PointPrimary(LPARAM lParam) const {
        return owner->vertical_ ? static_cast<int>(GET_Y_LPARAM(lParam)) : static_cast<int>(GET_X_LPARAM(lParam));
    }

    int ThumbStart(const RECT& thumb) const {
        return owner->vertical_ ? static_cast<int>(thumb.top) : static_cast<int>(thumb.left);
    }

    int ThumbEnd(const RECT& thumb) const {
        return owner->vertical_ ? static_cast<int>(thumb.bottom) : static_cast<int>(thumb.right);
    }

    int ValueFromThumbOffset(const RECT& track, int thumbOffset) const {
        const int trackLength = std::max(1, TrackLength(track));
        const int thumbLength = ThumbLength(track);
        const int movable = std::max(1, trackLength - thumbLength);
        const int clampedOffset = std::clamp(thumbOffset, 0, movable);
        const int maxValue = std::max(owner->minimum_, owner->maximum_ - std::max(0, owner->pageSize_));
        if (maxValue <= owner->minimum_) {
            return owner->minimum_;
        }
        const double ratio = static_cast<double>(clampedOffset) / static_cast<double>(movable);
        return owner->minimum_ + static_cast<int>((maxValue - owner->minimum_) * ratio + 0.5);
    }

    void NotifyScroll(WORD code) {
        if (!owner->parentHwnd_ || !owner->scrollBarHwnd_) {
            return;
        }
        SendMessageW(owner->parentHwnd_,
                     owner->vertical_ ? WM_VSCROLL : WM_HSCROLL,
                     MAKEWPARAM(code, owner->value_),
                     reinterpret_cast<LPARAM>(owner->scrollBarHwnd_));
    }

    void StartMouseLeaveTracking() {
        if (!owner->scrollBarHwnd_ || trackingMouseLeave) {
            return;
        }
        TRACKMOUSEEVENT track{};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = owner->scrollBarHwnd_;
        if (TrackMouseEvent(&track)) {
            trackingMouseLeave = true;
        }
    }

    void Paint(HDC targetDc) {
        HBRUSH backgroundBrush = brushBackground ? brushBackground : reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        HBRUSH trackBrush = brushTrack ? brushTrack : reinterpret_cast<HBRUSH>(GetStockObject(DKGRAY_BRUSH));
        HBRUSH thumbBrush = brushThumb ? brushThumb : reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH));
        HBRUSH thumbHotBrush = brushThumbHot ? brushThumbHot : thumbBrush;

        RECT client{};
        GetClientRect(owner->scrollBarHwnd_, &client);
        FillRect(targetDc, &client, backgroundBrush);

        RECT track = GetTrackRect();
        FillRect(targetDc, &track, trackBrush);

        RECT thumb = GetThumbRect();
        FillRect(targetDc, &thumb, (hot || dragging) ? thumbHotBrush : thumbBrush);
    }

    static LRESULT CALLBACK ScrollBarWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
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
        case WM_MOUSEMOVE: {
            const RECT thumb = self->GetThumbRect();
            const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const bool hoverThumb = PtInRect(&thumb, point) != FALSE;
            if (self->hot != hoverThumb) {
                self->hot = hoverThumb;
                InvalidateRect(window, nullptr, FALSE);
            }
            self->StartMouseLeaveTracking();
            if (self->dragging) {
                const RECT track = self->GetTrackRect();
                const int pointer = self->PointPrimary(lParam);
                const int trackStart = self->owner->vertical_ ? static_cast<int>(track.top) : static_cast<int>(track.left);
                const int newOffset = pointer - trackStart - self->dragOffset;
                self->owner->SetValue(self->ValueFromThumbOffset(track, newOffset), false);
                self->NotifyScroll(SB_THUMBTRACK);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            self->hot = false;
            self->trackingMouseLeave = false;
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(window);
            const RECT thumb = self->GetThumbRect();
            const int primary = self->PointPrimary(lParam);
            if (primary >= self->ThumbStart(thumb) && primary <= self->ThumbEnd(thumb)) {
                SetCapture(window);
                self->dragging = true;
                self->dragOffset = primary - self->ThumbStart(thumb);
                self->NotifyScroll(SB_THUMBTRACK);
            } else {
                const int delta = std::max(1, self->owner->pageSize_);
                if (primary < self->ThumbStart(thumb)) {
                    self->owner->SetValue(self->owner->value_ - delta, false);
                    self->NotifyScroll(self->owner->vertical_ ? SB_PAGEUP : SB_PAGELEFT);
                } else {
                    self->owner->SetValue(self->owner->value_ + delta, false);
                    self->NotifyScroll(self->owner->vertical_ ? SB_PAGEDOWN : SB_PAGERIGHT);
                }
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP:
            if (self->dragging) {
                self->dragging = false;
                self->dragOffset = 0;
                if (GetCapture() == window) {
                    ReleaseCapture();
                }
                self->NotifyScroll(SB_ENDSCROLL);
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_CAPTURECHANGED:
            if (self->dragging) {
                self->dragging = false;
                self->dragOffset = 0;
                self->NotifyScroll(SB_ENDSCROLL);
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_UP || wParam == VK_LEFT) {
                self->owner->SetValue(self->owner->value_ - 1, false);
                self->NotifyScroll(self->owner->vertical_ ? SB_LINEUP : SB_LINELEFT);
                return 0;
            }
            if (wParam == VK_DOWN || wParam == VK_RIGHT) {
                self->owner->SetValue(self->owner->value_ + 1, false);
                self->NotifyScroll(self->owner->vertical_ ? SB_LINEDOWN : SB_LINERIGHT);
                return 0;
            }
            if (wParam == VK_HOME) {
                self->owner->SetValue(self->owner->minimum_, false);
                self->NotifyScroll(self->owner->vertical_ ? SB_TOP : SB_LEFT);
                return 0;
            }
            if (wParam == VK_END) {
                self->owner->SetValue(self->ClampValue(self->owner->maximum_), false);
                self->NotifyScroll(self->owner->vertical_ ? SB_BOTTOM : SB_RIGHT);
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

ATOM EnsureScrollBarClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ScrollBarWindowProcThunk;
    wc.hInstance = instance;
    wc.lpszClassName = kScrollBarClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

LRESULT CALLBACK ScrollBarWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return ScrollBar::Impl::ScrollBarWindowProc(window, message, wParam, lParam);
}

}  // namespace

ScrollBar::ScrollBar() : impl_(std::make_unique<Impl>(this)) {}

ScrollBar::~ScrollBar() {
    Destroy();
}

bool ScrollBar::Create(HWND parent, int controlId, bool vertical, const Theme& theme, DWORD style, DWORD exStyle) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    vertical_ = vertical;
    theme_ = theme;
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }

    if (!EnsureScrollBarClassRegistered(impl_->instance)) {
        Destroy();
        return false;
    }

    scrollBarHwnd_ = CreateWindowExW(exStyle,
                                     kScrollBarClassName,
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
    if (!scrollBarHwnd_) {
        Destroy();
        return false;
    }

    impl_->UpdateThemeResources();
    if (!impl_->brushBackground || !impl_->brushTrack || !impl_->brushThumb || !impl_->brushThumbHot) {
        Destroy();
        return false;
    }
    return true;
}

void ScrollBar::Destroy() {
    if (scrollBarHwnd_) {
        if (GetCapture() == scrollBarHwnd_) {
            ReleaseCapture();
        }
        DestroyWindow(scrollBarHwnd_);
        scrollBarHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
}

void ScrollBar::SetTheme(const Theme& theme) {
    theme_ = theme;
    impl_->UpdateThemeResources();
}

void ScrollBar::SetRange(int minimum, int maximum) {
    if (minimum > maximum) {
        std::swap(minimum, maximum);
    }
    minimum_ = minimum;
    maximum_ = maximum;
    value_ = impl_->ClampValue(value_);
    if (scrollBarHwnd_) {
        InvalidateRect(scrollBarHwnd_, nullptr, TRUE);
    }
}

void ScrollBar::SetPageSize(int pageSize) {
    pageSize_ = std::max(0, pageSize);
    value_ = impl_->ClampValue(value_);
    if (scrollBarHwnd_) {
        InvalidateRect(scrollBarHwnd_, nullptr, FALSE);
    }
}

void ScrollBar::SetValue(int value, bool notify) {
    value_ = impl_->ClampValue(value);
    if (scrollBarHwnd_) {
        InvalidateRect(scrollBarHwnd_, nullptr, FALSE);
    }
    if (notify && parentHwnd_ && scrollBarHwnd_) {
        SendMessageW(parentHwnd_,
                     vertical_ ? WM_VSCROLL : WM_HSCROLL,
                     MAKEWPARAM(SB_THUMBPOSITION, value_),
                     reinterpret_cast<LPARAM>(scrollBarHwnd_));
    }
}

}  // namespace darkui
