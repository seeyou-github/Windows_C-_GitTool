#include "darkui/progress.h"

#include <algorithm>
#include <string>

namespace darkui {
namespace {

constexpr wchar_t kProgressClassName[] = L"DarkUiProgressBarControl";

ATOM EnsureProgressClassRegistered(HINSTANCE instance);
LRESULT CALLBACK ProgressWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

}  // namespace

struct ProgressBar::Impl {
    ProgressBar* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushBackground = nullptr;
    HBRUSH brushTrack = nullptr;
    HBRUSH brushFill = nullptr;
    HFONT font = nullptr;

    explicit Impl(ProgressBar* progress) : owner(progress) {}

    ~Impl() {
        if (brushBackground) DeleteObject(brushBackground);
        if (brushTrack) DeleteObject(brushTrack);
        if (brushFill) DeleteObject(brushFill);
        if (font) DeleteObject(font);
    }

    void UpdateThemeResources() {
        if (brushBackground) DeleteObject(brushBackground);
        if (brushTrack) DeleteObject(brushTrack);
        if (brushFill) DeleteObject(brushFill);
        if (font) DeleteObject(font);

        brushBackground = CreateSolidBrush(owner->theme_.progressBackground);
        brushTrack = CreateSolidBrush(owner->theme_.progressTrack);
        brushFill = CreateSolidBrush(owner->theme_.progressFill);
        font = CreateFont(owner->theme_.uiFont);

        if (owner->progressHwnd_) {
            InvalidateRect(owner->progressHwnd_, nullptr, TRUE);
        }
    }

    RECT GetTrackRect() const {
        RECT client{};
        GetClientRect(owner->progressHwnd_, &client);
        const int trackHeight = std::max(4, owner->theme_.progressHeight);
        const int top = static_cast<int>(client.top) +
                        std::max(0, static_cast<int>(client.bottom - client.top - trackHeight) / 2);
        return RECT{client.left, top, client.right, top + trackHeight};
    }

    int FillRight(const RECT& track) const {
        if (owner->maximum_ <= owner->minimum_) {
            return track.left;
        }
        const double ratio = static_cast<double>(owner->value_ - owner->minimum_) /
                             static_cast<double>(owner->maximum_ - owner->minimum_);
        return track.left + static_cast<int>((track.right - track.left) * ratio + 0.5);
    }

    void Paint(HDC targetDc) {
        HBRUSH fallbackBackground = brushBackground ? nullptr : CreateSolidBrush(owner->theme_.progressBackground);
        HBRUSH fallbackTrack = brushTrack ? nullptr : CreateSolidBrush(owner->theme_.progressTrack);
        HBRUSH fallbackFill = brushFill ? nullptr : CreateSolidBrush(owner->theme_.progressFill);
        HBRUSH backgroundBrush = brushBackground ? brushBackground : fallbackBackground;
        HBRUSH trackBrush = brushTrack ? brushTrack : fallbackTrack;
        HBRUSH fillBrush = brushFill ? brushFill : fallbackFill;

        RECT client{};
        GetClientRect(owner->progressHwnd_, &client);
        HBRUSH surfaceBrush = CreateSolidBrush(owner->surfaceColor_);
        FillRect(targetDc, &client, surfaceBrush);
        DeleteObject(surfaceBrush);

        RECT track = GetTrackRect();
        FillRect(targetDc, &track, backgroundBrush);

        RECT innerTrack = track;
        InflateRect(&innerTrack, -1, -1);
        if (innerTrack.right > innerTrack.left && innerTrack.bottom > innerTrack.top) {
            FillRect(targetDc, &innerTrack, trackBrush);
        }

        RECT fill = innerTrack;
        fill.right = std::clamp(FillRight(innerTrack),
                                static_cast<int>(innerTrack.left),
                                static_cast<int>(innerTrack.right));
        if (fill.right > fill.left) {
            FillRect(targetDc, &fill, fillBrush);
        }

        if (!owner->showPercentage_) {
            return;
        }

        const int range = std::max(1, owner->maximum_ - owner->minimum_);
        const int percent = ((owner->value_ - owner->minimum_) * 100) / range;
        const std::wstring text = std::to_wstring(std::clamp(percent, 0, 100)) + L"%";

        HFONT oldFont = font ? reinterpret_cast<HFONT>(SelectObject(targetDc, font)) : nullptr;
        SetBkMode(targetDc, TRANSPARENT);
        SetTextColor(targetDc, owner->theme_.progressText);
        DrawTextW(targetDc, text.c_str(), -1, &client, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (oldFont) {
            SelectObject(targetDc, oldFont);
        }
        if (fallbackBackground) DeleteObject(fallbackBackground);
        if (fallbackTrack) DeleteObject(fallbackTrack);
        if (fallbackFill) DeleteObject(fallbackFill);
    }

    static LRESULT CALLBACK ProgressWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
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
        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
};

namespace {

ATOM EnsureProgressClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ProgressWindowProcThunk;
    wc.hInstance = instance;
    wc.lpszClassName = kProgressClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

LRESULT CALLBACK ProgressWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return ProgressBar::Impl::ProgressWindowProc(window, message, wParam, lParam);
}

}  // namespace

ProgressBar::ProgressBar() : impl_(std::make_unique<Impl>(this)) {}

ProgressBar::~ProgressBar() {
    Destroy();
}

bool ProgressBar::Create(HWND parent, int controlId, const Theme& theme, DWORD style, DWORD exStyle) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    theme_ = theme;
    surfaceColor_ = theme.background;
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }

    if (!EnsureProgressClassRegistered(impl_->instance)) {
        Destroy();
        return false;
    }
    progressHwnd_ = CreateWindowExW(exStyle,
                                    kProgressClassName,
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
    if (!progressHwnd_) {
        Destroy();
        return false;
    }

    impl_->UpdateThemeResources();
    if (!impl_->brushBackground || !impl_->brushTrack || !impl_->brushFill || !impl_->font) {
        Destroy();
        return false;
    }
    return true;
}

void ProgressBar::Destroy() {
    if (progressHwnd_) {
        DestroyWindow(progressHwnd_);
        progressHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
}

void ProgressBar::SetTheme(const Theme& theme) {
    theme_ = theme;
    impl_->UpdateThemeResources();
}

void ProgressBar::SetRange(int minimum, int maximum) {
    if (minimum > maximum) {
        std::swap(minimum, maximum);
    }
    minimum_ = minimum;
    maximum_ = maximum;
    value_ = std::clamp(value_, minimum_, maximum_);
    if (progressHwnd_) {
        InvalidateRect(progressHwnd_, nullptr, TRUE);
    }
}

void ProgressBar::SetValue(int value) {
    value_ = std::clamp(value, minimum_, maximum_);
    if (progressHwnd_) {
        InvalidateRect(progressHwnd_, nullptr, FALSE);
    }
}

void ProgressBar::SetShowPercentage(bool enabled) {
    showPercentage_ = enabled;
    if (progressHwnd_) {
        InvalidateRect(progressHwnd_, nullptr, FALSE);
    }
}

void ProgressBar::SetSurfaceColor(COLORREF color) {
    surfaceColor_ = color;
    if (progressHwnd_) {
        InvalidateRect(progressHwnd_, nullptr, TRUE);
    }
}

}  // namespace darkui
