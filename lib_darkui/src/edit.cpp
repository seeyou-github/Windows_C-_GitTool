#include "darkui/edit.h"

#include <commctrl.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>

namespace darkui {
namespace {

constexpr wchar_t kEditHostClassName[] = L"DarkUiEditHost";
constexpr int kEditPlaceholderId = 0x61A7;
constexpr wchar_t kEditDebugLogPath[] = L"demo\\build\\edit_layout_debug.log";

ATOM EnsureEditHostClassRegistered(HINSTANCE instance);
LRESULT CALLBACK EditHostWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

void AppendEditDebugLog(const wchar_t* format, ...) {
    FILE* file = nullptr;
    if (_wfopen_s(&file, kEditDebugLogPath, L"a+, ccs=UTF-8") != 0 || !file) {
        return;
    }

    va_list args;
    va_start(args, format);
    vfwprintf(file, format, args);
    va_end(args);
    fputws(L"\n", file);
    fclose(file);
}

}  // namespace

struct Edit::Impl {
    Edit* owner = nullptr;
    HINSTANCE instance = nullptr;
    HBRUSH brushBackground = nullptr;
    HFONT font = nullptr;
    // Separate placeholder STATIC keeps custom placeholder color and fallback
    // behavior independent from native EM_SETCUEBANNER support.
    HWND placeholderHwnd = nullptr;
    std::wstring cueBanner;
    std::wstring debugLayoutInfo;

    explicit Impl(Edit* edit) : owner(edit) {}

    ~Impl() {
        if (brushBackground) DeleteObject(brushBackground);
        if (font) DeleteObject(font);
    }

    bool UpdateThemeResources() {
        HBRUSH newBrushBackground = CreateSolidBrush(owner->theme_.editBackground);
        HFONT newFont = CreateFont(owner->theme_.uiFont);
        if (!newBrushBackground || !newFont) {
            if (newBrushBackground) DeleteObject(newBrushBackground);
            if (newFont) DeleteObject(newFont);
            return false;
        }

        if (brushBackground) DeleteObject(brushBackground);
        if (font) DeleteObject(font);
        brushBackground = newBrushBackground;
        font = newFont;

        if (owner->editHwnd_) {
            SendMessageW(owner->editHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            InvalidateRect(owner->editHwnd_, nullptr, TRUE);
        }
        if (placeholderHwnd) {
            SendMessageW(placeholderHwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            InvalidateRect(placeholderHwnd, nullptr, TRUE);
        }
        if (owner->hostHwnd_) {
            InvalidateRect(owner->hostHwnd_, nullptr, TRUE);
        }
        return true;
    }

    RECT ClientRect(HWND window) const {
        RECT rect{};
        GetClientRect(window, &rect);
        return rect;
    }

    void ApplyEditColors(HDC dc) const {
        SetTextColor(dc, owner->theme_.editText);
        SetBkColor(dc, owner->theme_.editBackground);
        SetBkMode(dc, OPAQUE);
    }

    SIZE CurrentTextMetrics() const {
        SIZE size{0, 0};
        if (!owner->editHwnd_) {
            return size;
        }
        HDC dc = GetDC(owner->editHwnd_);
        if (!dc) {
            return size;
        }
        HFONT drawFont = font ? font : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HFONT oldFont = drawFont ? reinterpret_cast<HFONT>(SelectObject(dc, drawFont)) : nullptr;
        TEXTMETRICW metrics{};
        if (GetTextMetricsW(dc, &metrics)) {
            size.cx = metrics.tmAveCharWidth;
            size.cy = metrics.tmHeight;
        }
        if (oldFont) {
            SelectObject(dc, oldFont);
        }
        ReleaseDC(owner->editHwnd_, dc);
        return size;
    }

    TEXTMETRICW CurrentFontMetrics() const {
        TEXTMETRICW metrics{};
        if (!owner->editHwnd_) {
            return metrics;
        }
        HDC dc = GetDC(owner->editHwnd_);
        if (!dc) {
            return metrics;
        }
        HFONT drawFont = font ? font : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HFONT oldFont = drawFont ? reinterpret_cast<HFONT>(SelectObject(dc, drawFont)) : nullptr;
        GetTextMetricsW(dc, &metrics);
        if (oldFont) {
            SelectObject(dc, oldFont);
        }
        ReleaseDC(owner->editHwnd_, dc);
        return metrics;
    }

    // Content insets are based on both the rounded host shape and the current
    // font metrics so larger fonts and multiline edits keep a safe visual margin.
    RECT ContentRect(HWND window) const {
        RECT rect = ClientRect(window);
        const SIZE textMetrics = CurrentTextMetrics();
        const TEXTMETRICW fontMetrics = CurrentFontMetrics();
        const bool multiline = owner->editHwnd_ && (GetWindowLongPtrW(owner->editHwnd_, GWL_STYLE) & ES_MULTILINE) != 0;
        const int textInsetX = textMetrics.cx > 0 ? static_cast<int>(textMetrics.cx) : 0;
        const int insetX = std::max(10, std::max(owner->cornerRadius_ / 2 + 4, textInsetX));
        rect.left += insetX;
        rect.right -= insetX;

        if (multiline) {
            rect.top += 8;
            rect.bottom -= 8;
        } else {
            const int clientHeight = std::max(1, static_cast<int>(rect.bottom - rect.top));
            const int textHeight = std::max(1, static_cast<int>(textMetrics.cy));
            const int descent = static_cast<int>(fontMetrics.tmDescent);
            const int internalLeading = static_cast<int>(fontMetrics.tmInternalLeading);
            const int desiredHeight = std::min(clientHeight, textHeight + std::max(4, descent + 2));
            const int extraSpace = std::max(0, clientHeight - desiredHeight);
            // Single-line native EDIT looks visually high when the child window is too tall.
            // Keep the child tighter and place it lower for smaller fonts; let larger fonts
            // drift upward gradually as they consume more vertical space.
            const double topBias = std::clamp(0.80 - static_cast<double>(textHeight - 20) * 0.012 - static_cast<double>(internalLeading) * 0.008,
                                              0.46,
                                              0.80);
            const int topInsetY = std::max(3, static_cast<int>(extraSpace * topBias));
            const int bottomInsetY = std::max(4, clientHeight - desiredHeight - topInsetY);
            rect.top += topInsetY;
            rect.bottom -= bottomInsetY;
        }

        if (rect.right < rect.left) rect.right = rect.left;
        if (rect.bottom < rect.top) rect.bottom = rect.top;
        return rect;
    }

    // The host itself owns the rounded silhouette. The inner EDIT stays rectangular
    // and clipped inside the padded content area.
    void UpdateWindowRegion() {
        if (!owner->hostHwnd_) {
            return;
        }
        RECT rect = ClientRect(owner->hostHwnd_);
        if (owner->cornerRadius_ <= 0) {
            SetWindowRgn(owner->hostHwnd_, nullptr, TRUE);
            return;
        }
        HRGN region = CreateRoundRectRgn(rect.left,
                                         rect.top,
                                         rect.right + 1,
                                         rect.bottom + 1,
                                         owner->cornerRadius_ * 2,
                                         owner->cornerRadius_ * 2);
        SetWindowRgn(owner->hostHwnd_, region, TRUE);
    }

    void LayoutChildren() {
        if (!owner->hostHwnd_) {
            return;
        }
        RECT rect = ContentRect(owner->hostHwnd_);
        const int width = std::max(1, static_cast<int>(rect.right - rect.left));
        const int height = std::max(1, static_cast<int>(rect.bottom - rect.top));
        if (owner->editHwnd_) {
            MoveWindow(owner->editHwnd_, rect.left, rect.top, width, height, FALSE);
        }
        if (placeholderHwnd) {
            const TEXTMETRICW metrics = CurrentFontMetrics();
            const int extraBottom = std::max(2, static_cast<int>(metrics.tmDescent) / 2);
            RECT client = ClientRect(owner->hostHwnd_);
            const int placeholderBottom = std::min(client.bottom, rect.bottom + extraBottom);
            const int placeholderHeight = std::max(1, static_cast<int>(placeholderBottom - rect.top));
            MoveWindow(placeholderHwnd, rect.left, rect.top, width, placeholderHeight, FALSE);
            InvalidateRect(placeholderHwnd, nullptr, TRUE);
        }

        const RECT client = ClientRect(owner->hostHwnd_);
        const SIZE textMetrics = CurrentTextMetrics();
        const TEXTMETRICW fontMetrics = CurrentFontMetrics();
        wchar_t buffer[256]{};
        _snwprintf_s(buffer,
                     _countof(buffer),
                     _TRUNCATE,
                     L"id=%d font=%d client=%ldx%ld content=(%ld,%ld,%ld,%ld) textH=%ld asc=%ld desc=%ld lead=%ld",
                     owner->controlId_,
                     owner->theme_.uiFont.height,
                     client.right - client.left,
                     client.bottom - client.top,
                     rect.left,
                     rect.top,
                     rect.right,
                     rect.bottom,
                     static_cast<long>(textMetrics.cy),
                     static_cast<long>(fontMetrics.tmAscent),
                     static_cast<long>(fontMetrics.tmDescent),
                     static_cast<long>(fontMetrics.tmInternalLeading));
        debugLayoutInfo = buffer;
        AppendEditDebugLog(L"[Layout] %ls", debugLayoutInfo.c_str());
    }

    bool IsFocused() const {
        HWND focus = GetFocus();
        return focus == owner->editHwnd_ || focus == placeholderHwnd || focus == owner->hostHwnd_;
    }

    bool HasText() const {
        return owner->editHwnd_ && GetWindowTextLengthW(owner->editHwnd_) > 0;
    }

    void UpdatePlaceholderVisibility() {
        if (!placeholderHwnd) {
            return;
        }
        // Placeholder is only visible when the field is empty and not focused,
        // matching the usual cue-banner behavior.
        const bool show = !cueBanner.empty() && !HasText() && !IsFocused();
        ShowWindow(placeholderHwnd, show ? SW_SHOWNOACTIVATE : SW_HIDE);
        if (show) {
            InvalidateRect(placeholderHwnd, nullptr, TRUE);
        }
    }

    // Forward standard EN_* notifications with the inner EDIT handle as lParam so
    // parent code can treat the control like a normal Win32 edit where possible.
    void ForwardEditCommand(WPARAM wParam) {
        if (!owner->parentHwnd_) {
            return;
        }
        SendMessageW(owner->parentHwnd_,
                     WM_COMMAND,
                     MAKEWPARAM(static_cast<UINT>(owner->controlId_), HIWORD(wParam)),
                     reinterpret_cast<LPARAM>(owner->editHwnd_));
    }

    void PaintHost(HDC dc, HWND window) {
        RECT rect = ClientRect(window);
        HBRUSH fillBrush = brushBackground ? brushBackground : reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        if (owner->cornerRadius_ > 0) {
            HGDIOBJ oldBrush = SelectObject(dc, fillBrush);
            HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(dc, GetStockObject(NULL_PEN)));
            RoundRect(dc, rect.left, rect.top, rect.right + 1, rect.bottom + 1, owner->cornerRadius_ * 2, owner->cornerRadius_ * 2);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
        } else {
            FillRect(dc, &rect, fillBrush);
        }
    }

    static LRESULT CALLBACK HostWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
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
            self->PaintHost(dc, window);
            EndPaint(window, &ps);
            return 0;
        }
        case WM_SIZE:
            self->UpdateWindowRegion();
            self->LayoutChildren();
            self->UpdatePlaceholderVisibility();
            RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            return 0;
        case WM_SETFOCUS:
        case WM_LBUTTONDOWN:
            if (self->owner->editHwnd_) {
                SetFocus(self->owner->editHwnd_);
            }
            return 0;
        case WM_CTLCOLOREDIT:
            if (reinterpret_cast<HWND>(lParam) == self->owner->editHwnd_) {
                HDC dc = reinterpret_cast<HDC>(wParam);
                self->ApplyEditColors(dc);
                return reinterpret_cast<LRESULT>(self->brushBackground ? self->brushBackground : GetStockObject(BLACK_BRUSH));
            }
            break;
        case WM_CTLCOLORSTATIC:
            // Native EDIT switches to CTLCOLORSTATIC in read-only mode, so both
            // editable and read-only states need to share the same dark color path.
            if (reinterpret_cast<HWND>(lParam) == self->owner->editHwnd_) {
                HDC dc = reinterpret_cast<HDC>(wParam);
                self->ApplyEditColors(dc);
                return reinterpret_cast<LRESULT>(self->brushBackground ? self->brushBackground : GetStockObject(BLACK_BRUSH));
            }
            if (reinterpret_cast<HWND>(lParam) == self->placeholderHwnd) {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetTextColor(dc, self->owner->theme_.editPlaceholder);
                SetBkColor(dc, self->owner->theme_.editBackground);
                SetBkMode(dc, OPAQUE);
                return reinterpret_cast<LRESULT>(self->brushBackground ? self->brushBackground : GetStockObject(BLACK_BRUSH));
            }
            break;
        case WM_COMMAND:
            if (reinterpret_cast<HWND>(lParam) == self->owner->editHwnd_) {
                const UINT code = HIWORD(wParam);
                if (code == EN_CHANGE || code == EN_SETFOCUS || code == EN_KILLFOCUS) {
                    self->UpdatePlaceholderVisibility();
                }
                self->ForwardEditCommand(wParam);
                return 0;
            }
            if (reinterpret_cast<HWND>(lParam) == self->placeholderHwnd && HIWORD(wParam) == STN_CLICKED) {
                if (self->owner->editHwnd_) {
                    SetFocus(self->owner->editHwnd_);
                }
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

ATOM EnsureEditHostClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = EditHostWindowProcThunk;
    wc.hInstance = instance;
    wc.lpszClassName = kEditHostClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

LRESULT CALLBACK EditHostWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return Edit::Impl::HostWindowProc(window, message, wParam, lParam);
}

}  // namespace

Edit::Edit() : impl_(std::make_unique<Impl>(this)) {}

Edit::~Edit() {
    Destroy();
}

bool Edit::Create(HWND parent, int controlId, const std::wstring& text, const Theme& theme, DWORD style, DWORD exStyle) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    theme_ = theme;
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }

    if (!EnsureEditHostClassRegistered(impl_->instance)) {
        Destroy();
        return false;
    }

    DWORD hostStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
    if ((style & WS_TABSTOP) != 0) {
        hostStyle |= WS_TABSTOP;
    }

    hostHwnd_ = CreateWindowExW(exStyle & ~WS_EX_CLIENTEDGE,
                                kEditHostClassName,
                                L"",
                                hostStyle,
                                0,
                                0,
                                0,
                                0,
                                parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                impl_->instance,
                                impl_.get());
    if (!hostHwnd_) {
        Destroy();
        return false;
    }

    DWORD editStyle = style | WS_CHILD | WS_VISIBLE;
    editStyle &= ~WS_BORDER;
    editStyle &= ~WS_TABSTOP;
    if ((editStyle & ES_MULTILINE) == 0) {
        editStyle |= ES_AUTOHSCROLL;
    }

    editHwnd_ = CreateWindowExW(0,
                                L"EDIT",
                                text.c_str(),
                                editStyle,
                                0,
                                0,
                                0,
                                0,
                                hostHwnd_,
                                nullptr,
                                impl_->instance,
                                nullptr);
    if (!editHwnd_) {
        Destroy();
        return false;
    }

    impl_->placeholderHwnd = CreateWindowExW(0,
                                             L"STATIC",
                                             L"",
                                             WS_CHILD | SS_NOTIFY | SS_LEFT | SS_NOPREFIX,
                                             0,
                                             0,
                                             0,
                                             0,
                                             hostHwnd_,
                                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditPlaceholderId)),
                                             impl_->instance,
                                             nullptr);
    if (!impl_->placeholderHwnd) {
        Destroy();
        return false;
    }

    // Disable themed borders on the native EDIT; the host surface owns all outer styling.
    SetWindowTheme(editHwnd_, L"", L"");
    SendMessageW(editHwnd_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(0, 0));

    if (!impl_->UpdateThemeResources() || !impl_->brushBackground || !impl_->font) {
        Destroy();
        return false;
    }

    impl_->UpdateWindowRegion();
    impl_->LayoutChildren();
    impl_->UpdatePlaceholderVisibility();
    return true;
}

void Edit::Destroy() {
    if (impl_) {
        impl_->cueBanner.clear();
        impl_->placeholderHwnd = nullptr;
    }
    if (editHwnd_) {
        DestroyWindow(editHwnd_);
        editHwnd_ = nullptr;
    }
    if (hostHwnd_) {
        DestroyWindow(hostHwnd_);
        hostHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
}

void Edit::SetTheme(const Theme& theme) {
    const Theme previous = theme_;
    theme_ = theme;
    if (!impl_->UpdateThemeResources()) {
        theme_ = previous;
        impl_->UpdateThemeResources();
        return;
    }
    if (hostHwnd_) {
        InvalidateRect(hostHwnd_, nullptr, TRUE);
    }
    if (impl_) {
        impl_->UpdatePlaceholderVisibility();
    }
}

void Edit::SetText(const std::wstring& text) {
    if (editHwnd_) {
        SetWindowTextW(editHwnd_, text.c_str());
    }
    if (impl_) {
        impl_->UpdatePlaceholderVisibility();
    }
}

std::wstring Edit::GetText() const {
    if (!editHwnd_) {
        return {};
    }
    const int length = GetWindowTextLengthW(editHwnd_);
    if (length <= 0) {
        return {};
    }
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(editHwnd_, text.data(), length + 1);
    text.resize(length);
    return text;
}

void Edit::SetCueBanner(const std::wstring& text) {
    if (!impl_) {
        return;
    }
    impl_->cueBanner = text;
    if (impl_->placeholderHwnd) {
        SetWindowTextW(impl_->placeholderHwnd, text.c_str());
    }
    if (editHwnd_) {
        // Native cue-banner is still updated when available, but the custom placeholder
        // overlay remains the authoritative visual path for consistent dark rendering.
        SendMessageW(editHwnd_, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    }
    impl_->UpdatePlaceholderVisibility();
}

void Edit::SetCornerRadius(int radius) {
    cornerRadius_ = std::max(0, radius);
    if (impl_) {
        impl_->UpdateWindowRegion();
        impl_->LayoutChildren();
    }
    if (hostHwnd_) {
        InvalidateRect(hostHwnd_, nullptr, TRUE);
    }
}

void Edit::SetReadOnly(bool readOnly) {
    if (editHwnd_) {
        SendMessageW(editHwnd_, EM_SETREADONLY, readOnly ? TRUE : FALSE, 0);
    }
}

std::wstring Edit::DebugLayoutInfo() const {
    return impl_ ? impl_->debugLayoutInfo : std::wstring{};
}

}  // namespace darkui
