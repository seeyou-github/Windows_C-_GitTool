#include "darkui/button.h"

#include <commctrl.h>

namespace darkui {

struct Button::Impl {
    static constexpr UINT_PTR kAnimationTimerId = 1;

    Button* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushButton = nullptr;
    HBRUSH brushButtonHover = nullptr;
    HBRUSH brushButtonHot = nullptr;
    HBRUSH brushButtonDisabled = nullptr;
    HBRUSH brushBorder = nullptr;
    HPEN penBorder = nullptr;
    HFONT font = nullptr;
    bool hot = false;
    bool trackingMouseLeave = false;
    int visualOffsetY = 0;
    int targetOffsetY = 0;

    explicit Impl(Button* button) : owner(button) {}

    ~Impl() {
        if (font) DeleteObject(font);
        if (brushButton) DeleteObject(brushButton);
        if (brushButtonHover) DeleteObject(brushButtonHover);
        if (brushButtonHot) DeleteObject(brushButtonHot);
        if (brushButtonDisabled) DeleteObject(brushButtonDisabled);
        if (brushBorder) DeleteObject(brushBorder);
        if (penBorder) DeleteObject(penBorder);
    }

    void UpdateThemeResources() {
        if (font) DeleteObject(font);
        if (brushButton) DeleteObject(brushButton);
        if (brushButtonHover) DeleteObject(brushButtonHover);
        if (brushButtonHot) DeleteObject(brushButtonHot);
        if (brushButtonDisabled) DeleteObject(brushButtonDisabled);
        if (brushBorder) DeleteObject(brushBorder);
        if (penBorder) DeleteObject(penBorder);

        font = CreateFont(owner->theme_.uiFont);
        brushButton = CreateSolidBrush(owner->theme_.button);
        brushButtonHover = CreateSolidBrush(owner->theme_.buttonHover);
        brushButtonHot = CreateSolidBrush(owner->theme_.buttonHot);
        brushButtonDisabled = CreateSolidBrush(owner->theme_.buttonDisabled);
        brushBorder = CreateSolidBrush(owner->theme_.border);
        penBorder = CreatePen(PS_SOLID, 1, owner->theme_.border);

        if (owner->buttonHwnd_) {
            SendMessageW(owner->buttonHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            InvalidateRect(owner->buttonHwnd_, nullptr, TRUE);
        }
    }

    HFONT SelectControlFont(HDC dc, HWND control) {
        HFONT current = reinterpret_cast<HFONT>(SendMessageW(control, WM_GETFONT, 0, 0));
        return current ? reinterpret_cast<HFONT>(SelectObject(dc, current)) : nullptr;
    }

    void StepAnimation() {
        if (!owner->buttonHwnd_) {
            return;
        }
        if (visualOffsetY == targetOffsetY) {
            KillTimer(owner->buttonHwnd_, kAnimationTimerId);
            return;
        }

        if (visualOffsetY < targetOffsetY) {
            ++visualOffsetY;
        } else {
            --visualOffsetY;
        }
        InvalidateRect(owner->buttonHwnd_, nullptr, FALSE);
        if (visualOffsetY == targetOffsetY) {
            KillTimer(owner->buttonHwnd_, kAnimationTimerId);
        }
    }

    void UpdateWindowRegion() {
        if (!owner->buttonHwnd_) {
            return;
        }

        RECT rect{};
        GetClientRect(owner->buttonHwnd_, &rect);
        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0) {
            return;
        }

        if (owner->cornerRadius_ <= 0) {
            SetWindowRgn(owner->buttonHwnd_, nullptr, TRUE);
            return;
        }

        HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, owner->cornerRadius_ * 2, owner->cornerRadius_ * 2);
        SetWindowRgn(owner->buttonHwnd_, region, TRUE);
    }

    void AnimateTo(int offsetY) {
        targetOffsetY = offsetY;
        if (!owner->buttonHwnd_) {
            visualOffsetY = targetOffsetY;
            return;
        }
        SetTimer(owner->buttonHwnd_, kAnimationTimerId, 15, nullptr);
        StepAnimation();
    }

    void DrawButton(const DRAWITEMSTRUCT* draw) {
        const bool selected = (draw->itemState & ODS_SELECTED) != 0;
        const bool enabled = IsWindowEnabled(draw->hwndItem) != FALSE;

        HBRUSH fillBrush = brushButton;
        COLORREF textColor = owner->theme_.text;
        if (!enabled) {
            fillBrush = brushButtonDisabled;
            textColor = owner->theme_.buttonDisabledText;
        } else if (selected) {
            fillBrush = brushButtonHot;
        } else if (hot) {
            fillBrush = brushButtonHover;
        }

        RECT buttonRect = draw->rcItem;

        HBRUSH parentBrush = CreateSolidBrush(owner->surfaceColor_);
        FillRect(draw->hDC, &buttonRect, parentBrush);
        DeleteObject(parentBrush);

        OffsetRect(&buttonRect, 0, visualOffsetY);

        HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(draw->hDC, penBorder));
        HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(draw->hDC, fillBrush));
        RoundRect(draw->hDC,
                  buttonRect.left,
                  buttonRect.top,
                  buttonRect.right,
                  buttonRect.bottom,
                  owner->cornerRadius_ * 2,
                  owner->cornerRadius_ * 2);
        SelectObject(draw->hDC, oldBrush);
        SelectObject(draw->hDC, oldPen);

        RECT textRect = buttonRect;
        textRect.left += owner->theme_.textPadding;
        textRect.right -= owner->theme_.textPadding;

        HFONT oldFont = SelectControlFont(draw->hDC, draw->hwndItem);
        SetBkMode(draw->hDC, TRANSPARENT);
        SetTextColor(draw->hDC, textColor);

        wchar_t text[1024] = {};
        GetWindowTextW(draw->hwndItem, text, 1024);
        DrawTextW(draw->hDC, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

        if (oldFont) {
            SelectObject(draw->hDC, oldFont);
        }
    }

    void StartMouseLeaveTracking() {
        if (!owner->buttonHwnd_ || trackingMouseLeave) {
            return;
        }

        TRACKMOUSEEVENT track{};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = owner->buttonHwnd_;
        if (TrackMouseEvent(&track)) {
            trackingMouseLeave = true;
        }
    }

    static LRESULT CALLBACK ButtonSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData) {
        auto* self = reinterpret_cast<Impl*>(refData);
        if (!self) {
            return DefSubclassProc(window, message, wParam, lParam);
        }

        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE:
            if (!self->hot) {
                self->hot = true;
                InvalidateRect(window, nullptr, FALSE);
            }
            self->StartMouseLeaveTracking();
            break;
        case WM_MOUSELEAVE:
            self->hot = false;
            self->trackingMouseLeave = false;
            InvalidateRect(window, nullptr, FALSE);
            break;
        case WM_LBUTTONDOWN:
            self->AnimateTo(2);
            break;
        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED:
            self->AnimateTo(0);
            if (!IsWindowEnabled(window)) {
                self->hot = false;
                self->trackingMouseLeave = false;
            }
            break;
        case WM_TIMER:
            if (wParam == kAnimationTimerId) {
                self->StepAnimation();
                return 0;
            }
            break;
        case WM_ENABLE:
            self->hot = false;
            self->trackingMouseLeave = false;
            self->AnimateTo(0);
            InvalidateRect(window, nullptr, FALSE);
            break;
        case WM_SIZE:
            self->UpdateWindowRegion();
            break;
        case WM_DESTROY:
            KillTimer(window, kAnimationTimerId);
            RemoveWindowSubclass(window, ButtonSubclassProc, subclassId);
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
        case WM_DRAWITEM: {
            auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (draw && draw->hwndItem == self->owner->buttonHwnd_) {
                self->DrawButton(draw);
                return TRUE;
            }
            break;
        }
        case WM_DESTROY:
            RemoveWindowSubclass(window, ParentSubclassProc, subclassId);
            break;
        default:
            break;
        }

        return DefSubclassProc(window, message, wParam, lParam);
    }
};

Button::Button() : impl_(std::make_unique<Impl>(this)) {}

Button::~Button() {
    Destroy();
}

bool Button::Create(HWND parent, int controlId, const std::wstring& text, const Theme& theme, DWORD style, DWORD exStyle) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    theme_ = theme;
    surfaceColor_ = theme.background;
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }
    impl_->UpdateThemeResources();

    style |= BS_OWNERDRAW;
    buttonHwnd_ = CreateWindowExW(exStyle,
                                  L"BUTTON",
                                  text.c_str(),
                                  style,
                                  0,
                                  0,
                                  0,
                                  0,
                                  parent,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                  impl_->instance,
                                  nullptr);
    if (!buttonHwnd_) {
        Destroy();
        return false;
    }

    SendMessageW(buttonHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(impl_->font), TRUE);
    impl_->UpdateWindowRegion();
    SetWindowSubclass(buttonHwnd_,
                      Impl::ButtonSubclassProc,
                      reinterpret_cast<UINT_PTR>(this),
                      reinterpret_cast<DWORD_PTR>(impl_.get()));
    SetWindowSubclass(parentHwnd_,
                      Impl::ParentSubclassProc,
                      reinterpret_cast<UINT_PTR>(this),
                      reinterpret_cast<DWORD_PTR>(impl_.get()));
    return true;
}

void Button::Destroy() {
    if (parentHwnd_) {
        RemoveWindowSubclass(parentHwnd_, Impl::ParentSubclassProc, reinterpret_cast<UINT_PTR>(this));
    }
    if (buttonHwnd_) {
        RemoveWindowSubclass(buttonHwnd_, Impl::ButtonSubclassProc, reinterpret_cast<UINT_PTR>(this));
        DestroyWindow(buttonHwnd_);
        buttonHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
    impl_->hot = false;
    impl_->trackingMouseLeave = false;
    impl_->visualOffsetY = 0;
    impl_->targetOffsetY = 0;
}

void Button::SetTheme(const Theme& theme) {
    theme_ = theme;
    impl_->UpdateThemeResources();
}

void Button::SetText(const std::wstring& text) {
    if (buttonHwnd_) {
        SetWindowTextW(buttonHwnd_, text.c_str());
        InvalidateRect(buttonHwnd_, nullptr, TRUE);
    }
}

std::wstring Button::GetText() const {
    if (!buttonHwnd_) {
        return L"";
    }
    const int length = GetWindowTextLengthW(buttonHwnd_);
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(buttonHwnd_, text.data(), length + 1);
    text.resize(length);
    return text;
}

void Button::SetCornerRadius(int radius) {
    cornerRadius_ = radius < 0 ? 0 : radius;
    if (buttonHwnd_) {
        impl_->UpdateWindowRegion();
        InvalidateRect(buttonHwnd_, nullptr, TRUE);
    }
}

void Button::SetSurfaceColor(COLORREF color) {
    surfaceColor_ = color;
    if (buttonHwnd_) {
        InvalidateRect(buttonHwnd_, nullptr, TRUE);
    }
}

}  // namespace darkui
