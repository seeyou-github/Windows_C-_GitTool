#include <windows.h>
#include <commctrl.h>

#include <string>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiScrollBarDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui scrollbar demo";

enum ControlId {
    ID_SCROLL_H = 6001,
    ID_SCROLL_V = 6002
};

struct DemoState {
    darkui::Theme theme;
    darkui::ScrollBar horizontal;
    darkui::ScrollBar vertical;
    HBRUSH brushBackground = nullptr;
    HFONT titleFont = nullptr;
    HFONT textFont = nullptr;
    int horizontalValue = 24;
    int verticalValue = 56;
};

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.text = RGB(232, 236, 241);
    theme.mutedText = RGB(144, 151, 161);
    theme.scrollBarBackground = RGB(24, 27, 31);
    theme.scrollBarTrack = RGB(48, 53, 60);
    theme.scrollBarThumb = RGB(120, 128, 140);
    theme.scrollBarThumbHot = RGB(160, 170, 184);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    return theme;
}

void RecreateBackgroundBrush(DemoState* state) {
    if (state->brushBackground) {
        DeleteObject(state->brushBackground);
    }
    state->brushBackground = CreateSolidBrush(state->theme.background);
}

RECT GetInfoRect(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    return RECT{32, 328, client.right - 32, 378};
}

RECT GetDescriptionRect(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    return RECT{32, 64, client.right - 32, 116};
}

void Layout(HWND window, DemoState* state) {
    RECT client{};
    GetClientRect(window, &client);
    MoveWindow(state->horizontal.hwnd(), 32, 118, client.right - 120, state->theme.scrollBarThickness, TRUE);
    MoveWindow(state->vertical.hwnd(), client.right - 56, 118, state->theme.scrollBarThickness, 160, TRUE);
}

void CleanupState(DemoState* state) {
    if (!state) {
        return;
    }
    if (state->brushBackground) DeleteObject(state->brushBackground);
    if (state->titleFont) DeleteObject(state->titleFont);
    if (state->textFont) DeleteObject(state->textFont);
    delete state;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DemoState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* created = new DemoState();
        created->theme = MakeTheme();
        RecreateBackgroundBrush(created);

        darkui::FontSpec titleSpec = created->theme.uiFont;
        titleSpec.height = -30;
        titleSpec.weight = FW_SEMIBOLD;
        created->titleFont = darkui::CreateFont(titleSpec);
        created->textFont = darkui::CreateFont(created->theme.uiFont);
        if (!created->brushBackground || !created->titleFont || !created->textFont) {
            CleanupState(created);
            return -1;
        }

        if (!created->horizontal.Create(window, ID_SCROLL_H, false, created->theme) ||
            !created->vertical.Create(window, ID_SCROLL_V, true, created->theme)) {
            CleanupState(created);
            return -1;
        }

        created->horizontal.SetRange(0, 100);
        created->horizontal.SetPageSize(20);
        created->horizontal.SetValue(created->horizontalValue);
        created->vertical.SetRange(0, 100);
        created->vertical.SetPageSize(24);
        created->vertical.SetValue(created->verticalValue);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        return 0;
    }
    case WM_SIZE:
        if (state) {
            Layout(window, state);
        }
        return 0;
    case WM_HSCROLL:
        if (state && reinterpret_cast<HWND>(lParam) == state->horizontal.hwnd()) {
            state->horizontalValue = state->horizontal.GetValue();
            const RECT infoRect = GetInfoRect(window);
            InvalidateRect(window, &infoRect, FALSE);
            return 0;
        }
        break;
    case WM_VSCROLL:
        if (state && reinterpret_cast<HWND>(lParam) == state->vertical.hwnd()) {
            state->verticalValue = state->vertical.GetValue();
            const RECT infoRect = GetInfoRect(window);
            InvalidateRect(window, &infoRect, FALSE);
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        if (state) {
            RECT rect{};
            GetClientRect(window, &rect);
            FillRect(reinterpret_cast<HDC>(wParam), &rect, state->brushBackground);
            return 1;
        }
        break;
    case WM_PAINT:
        if (state) {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);

            RECT client{};
            GetClientRect(window, &client);
            FillRect(dc, &client, state->brushBackground);

            RECT titleRect{32, 24, client.right - 32, 56};
            RECT descRect = GetDescriptionRect(window);
            RECT infoRect = GetInfoRect(window);

            HFONT oldTitle = reinterpret_cast<HFONT>(SelectObject(dc, state->titleFont));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, state->theme.text);
            DrawTextW(dc, L"Dark ScrollBar Demo", -1, &titleRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, oldTitle);

            HFONT oldText = reinterpret_cast<HFONT>(SelectObject(dc, state->textFont));
            SetTextColor(dc, state->theme.mutedText);
            DrawTextW(dc, L"Custom horizontal and vertical scrollbars with drag, page jump, keyboard, and standard scroll notifications.", -1, &descRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

            std::wstring info = L"Horizontal: " + std::to_wstring(state->horizontalValue) +
                                L"    Vertical: " + std::to_wstring(state->verticalValue);
            SetTextColor(dc, state->theme.text);
            DrawTextW(dc, info.c_str(), -1, &infoRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, oldText);

            EndPaint(window, &ps);
            return 0;
        }
        break;
    case WM_DESTROY:
        CleanupState(state);
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kDemoClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    if (!RegisterClassExW(&wc)) {
        return 0;
    }

    HWND window = CreateWindowExW(0,
                                  kDemoClassName,
                                  kDemoTitle,
                                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  760,
                                  460,
                                  nullptr,
                                  nullptr,
                                  instance,
                                  nullptr);
    if (!window) {
        return 0;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
