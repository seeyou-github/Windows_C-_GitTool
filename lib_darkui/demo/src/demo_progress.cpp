#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <string>

#include "darkui/button.h"
#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiProgressDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui progress demo";

enum ControlId {
    ID_PROGRESS = 5001,
    ID_BUTTON_DEC = 5101,
    ID_BUTTON_INC,
    ID_BUTTON_TOGGLE_TEXT
};

struct DemoState {
    darkui::Theme theme;
    darkui::ProgressBar progress;
    darkui::Button buttonDec;
    darkui::Button buttonInc;
    darkui::Button buttonToggleText;
    HBRUSH brushBackground = nullptr;
    HFONT titleFont = nullptr;
    HFONT textFont = nullptr;
};

void CleanupState(DemoState* state) {
    if (!state) {
        return;
    }
    if (state->brushBackground) DeleteObject(state->brushBackground);
    if (state->titleFont) DeleteObject(state->titleFont);
    if (state->textFont) DeleteObject(state->textFont);
    delete state;
}

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.text = RGB(232, 236, 241);
    theme.mutedText = RGB(144, 151, 161);
    theme.border = RGB(70, 78, 90);
    theme.button = RGB(52, 57, 66);
    theme.buttonHover = RGB(62, 68, 79);
    theme.buttonHot = RGB(74, 82, 95);
    theme.progressBackground = RGB(24, 27, 31);
    theme.progressTrack = RGB(49, 54, 61);
    theme.progressFill = RGB(78, 120, 184);
    theme.progressText = RGB(240, 244, 248);
    theme.progressHeight = 18;
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

void Layout(HWND window, DemoState* state) {
    RECT client{};
    GetClientRect(window, &client);
    MoveWindow(state->progress.hwnd(), 32, 118, client.right - 64, 44, TRUE);
    MoveWindow(state->buttonDec.hwnd(), 32, 188, 92, 34, TRUE);
    MoveWindow(state->buttonInc.hwnd(), 132, 188, 92, 34, TRUE);
    MoveWindow(state->buttonToggleText.hwnd(), 236, 188, 160, 34, TRUE);
}

RECT GetValueRect(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    return RECT{32, 244, client.right - 32, 320};
}

void ApplyTheme(HWND window, DemoState* state) {
    RecreateBackgroundBrush(state);
    state->progress.SetTheme(state->theme);
    state->buttonDec.SetTheme(state->theme);
    state->buttonInc.SetTheme(state->theme);
    state->buttonToggleText.SetTheme(state->theme);
    InvalidateRect(window, nullptr, TRUE);
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

        if (!created->progress.Create(window, ID_PROGRESS, created->theme)) {
            CleanupState(created);
            return -1;
        }
        created->progress.SetRange(0, 100);
        created->progress.SetValue(64);

        if (!created->buttonDec.Create(window, ID_BUTTON_DEC, L"-10", created->theme) ||
            !created->buttonInc.Create(window, ID_BUTTON_INC, L"+10", created->theme) ||
            !created->buttonToggleText.Create(window, ID_BUTTON_TOGGLE_TEXT, L"Toggle Percent", created->theme)) {
            CleanupState(created);
            return -1;
        }
        created->buttonDec.SetCornerRadius(10);
        created->buttonInc.SetCornerRadius(10);
        created->buttonToggleText.SetCornerRadius(10);

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
        switch (LOWORD(wParam)) {
        case ID_BUTTON_DEC:
            state->progress.SetValue(std::max(state->progress.GetMinimum(), state->progress.GetValue() - 10));
            {
                const RECT valueRect = GetValueRect(window);
                InvalidateRect(window, &valueRect, FALSE);
            }
            return 0;
        case ID_BUTTON_INC:
            state->progress.SetValue(std::min(state->progress.GetMaximum(), state->progress.GetValue() + 10));
            {
                const RECT valueRect = GetValueRect(window);
                InvalidateRect(window, &valueRect, FALSE);
            }
            return 0;
        case ID_BUTTON_TOGGLE_TEXT:
            state->progress.SetShowPercentage(!state->progress.show_percentage());
            return 0;
        default:
            break;
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
            RECT noteRect = GetValueRect(window);

            HFONT oldTitle = reinterpret_cast<HFONT>(SelectObject(dc, state->titleFont));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, state->theme.text);
            DrawTextW(dc, L"Dark ProgressBar Demo", -1, &titleRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, oldTitle);

            HFONT oldText = reinterpret_cast<HFONT>(SelectObject(dc, state->textFont));
            SetTextColor(dc, state->theme.mutedText);
            DrawTextW(dc, L"Custom progress bar with independent outer background, track, fill, and text colors.", -1, &descRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

            std::wstring note = L"Current value: " + std::to_wstring(state->progress.GetValue()) + L" / 100";
            SetTextColor(dc, state->theme.text);
            DrawTextW(dc, note.c_str(), -1, &noteRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
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
                                  380,
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
