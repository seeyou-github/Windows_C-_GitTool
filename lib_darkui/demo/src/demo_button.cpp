#include <windows.h>
#include <commctrl.h>

#include <string>

#include "darkui/button.h"
#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiButtonDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui button demo";

enum ControlId {
    ID_BUTTON_PRIMARY = 3001,
    ID_BUTTON_ALT = 3002
};

struct DemoState {
    darkui::Theme theme;
    darkui::Button primary;
    darkui::Button secondary;
    HBRUSH brushBackground = nullptr;
    HFONT titleFont = nullptr;
    HFONT textFont = nullptr;
    int clickCount = 0;
};

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.button = RGB(52, 61, 72);
    theme.buttonHot = RGB(72, 86, 104);
    theme.border = RGB(84, 96, 112);
    theme.text = RGB(232, 236, 241);
    theme.mutedText = RGB(144, 151, 161);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    return theme;
}

void Layout(HWND window, DemoState* state) {
    RECT rc{};
    GetClientRect(window, &rc);
    const int left = 32;
    MoveWindow(state->primary.hwnd(), left, 120, 220, 46, TRUE);
    MoveWindow(state->secondary.hwnd(), left, 182, 220, 46, TRUE);
}

void DrawTextLine(HDC dc, HFONT font, COLORREF color, RECT rect, const wchar_t* text) {
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_SINGLELINE);
    if (oldFont) {
        SelectObject(dc, oldFont);
    }
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

        created->primary.Create(window, ID_BUTTON_PRIMARY, L"Primary Action", created->theme);
        created->primary.SetCornerRadius(14);
        darkui::Theme altTheme = created->theme;
        altTheme.button = RGB(70, 44, 58);
        altTheme.buttonHover = RGB(88, 54, 72);
        altTheme.buttonHot = RGB(102, 59, 79);
        altTheme.buttonDisabled = RGB(58, 50, 54);
        altTheme.buttonDisabledText = RGB(138, 128, 132);
        altTheme.border = RGB(132, 79, 102);
        created->secondary.Create(window, ID_BUTTON_ALT, L"Secondary Action", altTheme);
        created->secondary.SetCornerRadius(22);
        EnableWindow(created->secondary.hwnd(), FALSE);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        return 0;
    }
    case WM_SIZE:
        if (state) {
            Layout(window, state);
        }
        return 0;
    case WM_COMMAND:
        if (!state || HIWORD(wParam) != BN_CLICKED) {
            break;
        }
        if (LOWORD(wParam) == ID_BUTTON_PRIMARY) {
            ++state->clickCount;
            std::wstring title = L"Primary clicked " + std::to_wstring(state->clickCount) + L" times";
            SetWindowTextW(window, title.c_str());
            return 0;
        }
        if (LOWORD(wParam) == ID_BUTTON_ALT) {
            state->primary.SetText(L"Text Updated");
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
            RECT descRect{32, 64, client.right - 32, 92};
            RECT noteRect{32, 248, client.right - 32, 320};

            DrawTextLine(dc, state->titleFont, state->theme.text, titleRect, L"Dark Button Demo");
            DrawTextLine(dc, state->textFont, state->theme.mutedText, descRect, L"Owner-draw dark buttons with hover, press animation, corner radius, and disabled state.");
            DrawTextW(dc,
                      L"Primary Action: hover + press animation + 14px radius.\nSecondary Action: 22px radius and disabled-state colors.\nMove the mouse over the primary button to see hover and click motion.",
                      -1,
                      &noteRect,
                      DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

            EndPaint(window, &ps);
            return 0;
        }
        break;
    case WM_DESTROY:
        if (state) {
            if (state->brushBackground) DeleteObject(state->brushBackground);
            if (state->titleFont) DeleteObject(state->titleFont);
            if (state->textFont) DeleteObject(state->textFont);
        }
        delete state;
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
