#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <string>

#include "darkui/button.h"
#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiEditDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui edit demo";

enum ControlId {
    ID_EDIT_PRIMARY = 9001,
    ID_EDIT_SECONDARY = 9002,
    ID_BUTTON_APPLY = 9003,
    ID_BUTTON_INCREASE = 9004,
    ID_BUTTON_DECREASE = 9005
};

struct DemoState {
    darkui::Theme theme;
    darkui::Edit primaryEdit;
    darkui::Edit secondaryEdit;
    darkui::Button applyButton;
    darkui::Button increaseButton;
    darkui::Button decreaseButton;
    HBRUSH brushBackground = nullptr;
    HFONT titleFont = nullptr;
    HFONT textFont = nullptr;
    std::wstring status = L"Ready";
    std::wstring debugInfo;
};

void CleanupState(DemoState* state) {
    if (!state) return;
    if (state->brushBackground) DeleteObject(state->brushBackground);
    if (state->titleFont) DeleteObject(state->titleFont);
    if (state->textFont) DeleteObject(state->textFont);
    delete state;
}

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.panel = RGB(30, 33, 38);
    theme.border = RGB(58, 64, 74);
    theme.text = RGB(236, 239, 244);
    theme.mutedText = RGB(150, 156, 166);
    theme.editBackground = RGB(43, 47, 54);
    theme.editText = RGB(236, 239, 244);
    theme.editPlaceholder = RGB(128, 137, 150);
    theme.button = RGB(69, 95, 148);
    theme.buttonHover = RGB(82, 110, 168);
    theme.buttonHot = RGB(92, 122, 184);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    theme.textPadding = 10;
    return theme;
}

int GetEditPixelHeight(const DemoState* state) {
    const int fontHeight = state ? -state->theme.uiFont.height : 20;
    return std::max(42, fontHeight + 30);
}

void UpdateEditFont(DemoState* state, int newHeight) {
    if (!state) return;
    state->theme.uiFont.height = newHeight;
    state->primaryEdit.SetTheme(state->theme);
    state->secondaryEdit.SetTheme(state->theme);
}

void UpdateDebugInfo(DemoState* state) {
    if (!state) return;
    state->debugInfo = L"P: " + state->primaryEdit.DebugLayoutInfo() + L"\nS: " + state->secondaryEdit.DebugLayoutInfo();
}

void Layout(HWND window, DemoState* state) {
    RECT client{};
    GetClientRect(window, &client);

    const int left = 32;
    const int width = client.right - 64;
    const int editHeight = GetEditPixelHeight(state);
    const int secondTop = 104 + editHeight + 18;
    const int buttonTop = secondTop + editHeight + 22;

    MoveWindow(state->primaryEdit.hwnd(), left, 104, width, editHeight, TRUE);
    MoveWindow(state->secondaryEdit.hwnd(), left, secondTop, width, editHeight, TRUE);
    MoveWindow(state->increaseButton.hwnd(), left, buttonTop, 132, 40, TRUE);
    MoveWindow(state->decreaseButton.hwnd(), left + 148, buttonTop, 132, 40, TRUE);
    MoveWindow(state->applyButton.hwnd(), left + 296, buttonTop, 132, 40, TRUE);
    UpdateDebugInfo(state);
}

void DrawLine(HDC dc, HFONT font, COLORREF color, RECT rect, const wchar_t* text, UINT format) {
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, format | DT_NOPREFIX);
    if (oldFont) SelectObject(dc, oldFont);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DemoState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* created = new DemoState();
        created->theme = MakeTheme();
        created->brushBackground = CreateSolidBrush(created->theme.background);

        darkui::FontSpec titleSpec = created->theme.uiFont;
        titleSpec.height = -30;
        titleSpec.weight = FW_SEMIBOLD;
        created->titleFont = darkui::CreateFont(titleSpec);
        created->textFont = darkui::CreateFont(created->theme.uiFont);
        if (!created->brushBackground || !created->titleFont || !created->textFont) {
            CleanupState(created);
            return -1;
        }

        if (!created->primaryEdit.Create(window, ID_EDIT_PRIMARY, L"A borderless dark edit control", created->theme) ||
            !created->secondaryEdit.Create(window, ID_EDIT_SECONDARY, L"", created->theme) ||
            !created->increaseButton.Create(window, ID_BUTTON_INCREASE, L"Increase", created->theme) ||
            !created->decreaseButton.Create(window, ID_BUTTON_DECREASE, L"Decrease", created->theme) ||
            !created->applyButton.Create(window, ID_BUTTON_APPLY, L"Apply", created->theme)) {
            CleanupState(created);
            return -1;
        }

        created->secondaryEdit.SetCueBanner(L"Type here. No border line should be visible.");
        created->primaryEdit.SetCornerRadius(16);
        created->secondaryEdit.SetCornerRadius(16);
        created->increaseButton.SetCornerRadius(14);
        created->decreaseButton.SetCornerRadius(14);
        created->applyButton.SetCornerRadius(14);
        created->increaseButton.SetSurfaceColor(created->theme.background);
        created->decreaseButton.SetSurfaceColor(created->theme.background);
        created->applyButton.SetSurfaceColor(created->theme.background);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        UpdateDebugInfo(created);
        return 0;
    }
    case WM_SIZE:
        if (state) {
            Layout(window, state);
        }
        return 0;
    case WM_COMMAND:
        if (!state) break;
        if (LOWORD(wParam) == ID_BUTTON_INCREASE) {
            const int nextHeight = std::max(-40, state->theme.uiFont.height - 2);
            UpdateEditFont(state, nextHeight);
            state->status = L"Edit font increased to " + std::to_wstring(-nextHeight) + L" px";
            Layout(window, state);
            UpdateDebugInfo(state);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (LOWORD(wParam) == ID_BUTTON_DECREASE) {
            const int nextHeight = std::min(-12, state->theme.uiFont.height + 2);
            UpdateEditFont(state, nextHeight);
            state->status = L"Edit font decreased to " + std::to_wstring(-nextHeight) + L" px";
            Layout(window, state);
            UpdateDebugInfo(state);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (LOWORD(wParam) == ID_BUTTON_APPLY) {
            state->status = L"Primary: " + state->primaryEdit.GetText() + L" | Secondary: " + state->secondaryEdit.GetText();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (LOWORD(wParam) == ID_EDIT_PRIMARY && HIWORD(wParam) == EN_CHANGE) {
            state->status = L"Primary edit changed";
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (LOWORD(wParam) == ID_EDIT_SECONDARY && HIWORD(wParam) == EN_CHANGE) {
            state->status = L"Secondary edit changed";
            InvalidateRect(window, nullptr, FALSE);
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

            RECT titleRect{32, 24, client.right - 32, 58};
            RECT descRect{32, 60, client.right - 32, 90};
            RECT noteRect{32, 288, client.right - 32, 340};
            RECT statusRect{32, client.bottom - 96, client.right - 32, client.bottom - 68};
            RECT debugRect{32, client.bottom - 64, client.right - 32, client.bottom - 16};

            DrawLine(dc, state->titleFont, state->theme.text, titleRect, L"Dark Edit Demo", DT_LEFT | DT_TOP | DT_SINGLELINE);
            DrawLine(dc, state->textFont, state->theme.mutedText, descRect, L"Two rounded dark input fields with no visible native border line. Placeholder text color is theme-driven and remains available through the custom fallback overlay.", DT_LEFT | DT_TOP | DT_WORDBREAK);
            DrawTextW(dc,
                      L"Try focus changes, typing, selection, and IME input.\nThe control keeps native text behavior while removing the thin system border and showing a custom placeholder when empty.",
                      -1,
                      &noteRect,
                      DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
            DrawLine(dc, state->textFont, state->theme.text, statusRect, state->status.c_str(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            DrawTextW(dc,
                      state->debugInfo.c_str(),
                      -1,
                      &debugRect,
                      DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

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
                                  820,
                                  420,
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
