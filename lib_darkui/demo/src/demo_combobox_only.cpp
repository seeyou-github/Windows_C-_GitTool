#include <windows.h>
#include <commctrl.h>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiComboBoxOnlyDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui combobox theme demo";
constexpr int kComboCount = 4;
constexpr int kComboIds[kComboCount] = {1001, 1002, 1003, 1004};

struct DemoCombo {
    const wchar_t* label = L"";
    darkui::Theme theme;
    darkui::ComboBox combo;
};

struct DemoState {
    HBRUSH brushBackground = nullptr;
    DemoCombo combos[kComboCount];
};

darkui::Theme MakeSlateTheme() {
    darkui::Theme theme;
    theme.background = RGB(20, 23, 28);
    theme.panel = RGB(34, 39, 46);
    theme.button = RGB(52, 61, 72);
    theme.buttonHot = RGB(70, 82, 96);
    theme.border = RGB(74, 84, 98);
    theme.text = RGB(231, 235, 240);
    theme.mutedText = RGB(157, 166, 176);
    theme.arrow = RGB(157, 166, 176);
    theme.popupAccentItem = RGB(28, 54, 95);
    theme.popupAccentItemHot = RGB(42, 75, 126);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    return theme;
}

darkui::Theme MakeAmberTheme() {
    darkui::Theme theme;
    theme.background = RGB(20, 23, 28);
    theme.panel = RGB(54, 43, 31);
    theme.button = RGB(87, 62, 36);
    theme.buttonHot = RGB(109, 79, 47);
    theme.border = RGB(142, 107, 61);
    theme.text = RGB(255, 233, 197);
    theme.mutedText = RGB(219, 181, 120);
    theme.arrow = RGB(235, 191, 123);
    theme.popupItem = RGB(50, 39, 28);
    theme.popupItemHot = RGB(83, 60, 35);
    theme.popupAccentItem = RGB(120, 74, 25);
    theme.popupAccentItemHot = RGB(146, 92, 31);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    return theme;
}

darkui::Theme MakeTealTheme() {
    darkui::Theme theme;
    theme.background = RGB(20, 23, 28);
    theme.panel = RGB(25, 49, 53);
    theme.button = RGB(30, 74, 80);
    theme.buttonHot = RGB(40, 97, 105);
    theme.border = RGB(71, 145, 150);
    theme.text = RGB(213, 246, 242);
    theme.mutedText = RGB(137, 202, 197);
    theme.arrow = RGB(116, 220, 210);
    theme.popupItem = RGB(20, 45, 49);
    theme.popupItemHot = RGB(27, 69, 74);
    theme.popupAccentItem = RGB(21, 88, 90);
    theme.popupAccentItemHot = RGB(29, 111, 114);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    return theme;
}

darkui::Theme MakeRoseTheme() {
    darkui::Theme theme;
    theme.background = RGB(20, 23, 28);
    theme.panel = RGB(57, 33, 42);
    theme.button = RGB(90, 44, 61);
    theme.buttonHot = RGB(113, 57, 77);
    theme.border = RGB(154, 94, 117);
    theme.text = RGB(255, 225, 233);
    theme.mutedText = RGB(222, 161, 181);
    theme.arrow = RGB(241, 170, 194);
    theme.popupItem = RGB(51, 29, 38);
    theme.popupItemHot = RGB(78, 38, 53);
    theme.popupAccentItem = RGB(111, 35, 66);
    theme.popupAccentItemHot = RGB(140, 46, 83);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    return theme;
}

void Layout(HWND window, DemoState* state) {
    RECT rc{};
    GetClientRect(window, &rc);
    const int width = 420;
    const int height = 40;
    const int spacing = 22;
    const int totalHeight = kComboCount * height + (kComboCount - 1) * spacing;
    const int x = (rc.right - width) / 2;
    int y = (rc.bottom - totalHeight) / 2;
    for (int i = 0; i < kComboCount; ++i) {
        MoveWindow(state->combos[i].combo.hwnd(), x, y, width, height, TRUE);
        y += height + spacing;
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DemoState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* created = new DemoState();
        created->brushBackground = CreateSolidBrush(RGB(20, 23, 28));
        created->combos[0].label = L"Slate";
        created->combos[0].theme = MakeSlateTheme();
        created->combos[1].label = L"Amber";
        created->combos[1].theme = MakeAmberTheme();
        created->combos[2].label = L"Teal";
        created->combos[2].theme = MakeTealTheme();
        created->combos[3].label = L"Rose";
        created->combos[3].theme = MakeRoseTheme();
        for (int i = 0; i < kComboCount; ++i) {
            created->combos[i].combo.Create(window, kComboIds[i], created->combos[i].theme);
            created->combos[i].combo.SetItems({
                {std::wstring(created->combos[i].label) + L" / bestaudio", 1, false},
                {std::wstring(created->combos[i].label) + L" / bestvideo", 2, true},
                {std::wstring(created->combos[i].label) + L" / 137 - mp4 1080p", 3, true},
                {std::wstring(created->combos[i].label) + L" / 140 - m4a 128k", 4, false},
            });
            created->combos[i].combo.SetSelection(0);
        }
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        return 0;
    }
    case WM_SIZE:
        if (state) {
            Layout(window, state);
        }
        return 0;
    case WM_ERASEBKGND:
        if (state) {
            RECT rect{};
            GetClientRect(window, &rect);
            FillRect(reinterpret_cast<HDC>(wParam), &rect, state->brushBackground);
            return 1;
        }
        break;
    case WM_COMMAND:
        if (state && HIWORD(wParam) == CBN_SELCHANGE) {
            for (int i = 0; i < kComboCount; ++i) {
                if (LOWORD(wParam) == kComboIds[i]) {
                    const auto item = state->combos[i].combo.GetItem(state->combos[i].combo.GetSelection());
                    std::wstring title = std::wstring(state->combos[i].label) + L": " + item.text;
                    SetWindowTextW(window, title.c_str());
                    return 0;
                }
            }
        }
        break;
    case WM_DESTROY:
        if (state) {
            if (state->brushBackground) {
                DeleteObject(state->brushBackground);
            }
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
