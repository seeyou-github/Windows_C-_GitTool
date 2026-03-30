#include <windows.h>
#include <commctrl.h>

#include <memory>
#include <string>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiComboBoxDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui combobox demo";

enum ControlId {
    ID_LABEL_URL = 1001,
    ID_URL,
    ID_LABEL_FORMAT,
    ID_FORMAT,
    ID_SWITCH_THEME,
    ID_LOG
};

struct DemoState {
    darkui::Theme theme;
    HWND labelUrl = nullptr;
    HWND editUrl = nullptr;
    HWND labelFormat = nullptr;
    HWND buttonTheme = nullptr;
    HWND listLog = nullptr;
    darkui::ComboBox comboFormat;
    HBRUSH brushBackground = nullptr;
    HBRUSH brushPanel = nullptr;
    bool warmTheme = false;
};

darkui::Theme MakeCoolTheme() {
    darkui::Theme theme;
    theme.background = RGB(23, 26, 31);
    theme.panel = RGB(37, 42, 49);
    theme.button = RGB(57, 66, 78);
    theme.buttonHot = RGB(74, 85, 99);
    theme.border = RGB(71, 79, 92);
    theme.text = RGB(231, 235, 240);
    theme.mutedText = RGB(160, 168, 178);
    theme.popupAccentItem = RGB(28, 54, 95);
    theme.popupAccentItemHot = RGB(42, 75, 126);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -22;
    return theme;
}

darkui::Theme MakeWarmTheme() {
    darkui::Theme theme;
    theme.background = RGB(36, 29, 26);
    theme.panel = RGB(52, 41, 37);
    theme.button = RGB(88, 66, 55);
    theme.buttonHot = RGB(112, 83, 69);
    theme.border = RGB(128, 95, 79);
    theme.text = RGB(244, 232, 220);
    theme.mutedText = RGB(188, 168, 152);
    theme.popupAccentItem = RGB(101, 60, 24);
    theme.popupAccentItemHot = RGB(135, 82, 34);
    theme.uiFont.family = L"Microsoft YaHei UI";
    theme.uiFont.height = -22;
    return theme;
}

void AppendLog(HWND listBox, const std::wstring& text) {
    SendMessageW(listBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    const int count = static_cast<int>(SendMessageW(listBox, LB_GETCOUNT, 0, 0));
    if (count > 0) {
        SendMessageW(listBox, LB_SETTOPINDEX, count - 1, 0);
    }
}

void ApplyTheme(DemoState* state) {
    if (!state) {
        return;
    }
    state->theme = state->warmTheme ? MakeWarmTheme() : MakeCoolTheme();
    if (state->brushBackground) DeleteObject(state->brushBackground);
    if (state->brushPanel) DeleteObject(state->brushPanel);
    state->brushBackground = CreateSolidBrush(state->theme.background);
    state->brushPanel = CreateSolidBrush(state->theme.panel);
    state->comboFormat.SetTheme(state->theme);
    SetWindowTextW(state->buttonTheme, state->warmTheme ? L"Switch To Cool Theme" : L"Switch To Warm Theme");
    InvalidateRect(GetParent(state->labelUrl), nullptr, TRUE);
}

void Layout(HWND window, DemoState* state) {
    RECT rc{};
    GetClientRect(window, &rc);
    const int padding = 16;
    const int labelWidth = 120;
    const int inputHeight = 42;
    const int buttonHeight = 42;
    const int gap = 12;
    const int fieldX = padding + labelWidth;
    const int fieldWidth = rc.right - fieldX - padding;
    int y = padding;

    MoveWindow(state->labelUrl, padding, y + 6, labelWidth - 8, 30, TRUE);
    MoveWindow(state->editUrl, fieldX, y, fieldWidth, inputHeight, TRUE);
    y += inputHeight + gap;

    MoveWindow(state->labelFormat, padding, y + 6, labelWidth - 8, 30, TRUE);
    MoveWindow(state->comboFormat.hwnd(), fieldX, y, fieldWidth, inputHeight, TRUE);
    y += inputHeight + gap;

    MoveWindow(state->buttonTheme, padding, y, 200, buttonHeight, TRUE);
    y += buttonHeight + gap;

    MoveWindow(state->listLog, padding, y, rc.right - padding * 2, rc.bottom - y - padding, TRUE);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DemoState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* created = new DemoState();
        HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(window, GWLP_HINSTANCE));
        if (!instance) {
            instance = GetModuleHandleW(nullptr);
        }
        created->theme = MakeCoolTheme();
        created->brushBackground = CreateSolidBrush(created->theme.background);
        created->brushPanel = CreateSolidBrush(created->theme.panel);
        HFONT font = darkui::CreateFont(created->theme.uiFont);
        SetPropW(window, L"DemoFont", font);
        created->labelUrl = CreateWindowExW(0, L"STATIC", L"URL", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LABEL_URL)), instance, nullptr);
        created->editUrl = CreateWindowExW(0, L"EDIT", L"https://www.youtube.com/watch?v=demo", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_URL)), instance, nullptr);
        created->labelFormat = CreateWindowExW(0, L"STATIC", L"Format", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LABEL_FORMAT)), instance, nullptr);
        created->buttonTheme = CreateWindowExW(0, L"BUTTON", L"Switch To Warm Theme", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SWITCH_THEME)), instance, nullptr);
        created->listLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY, 0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LOG)), instance, nullptr);
        SendMessageW(created->labelUrl, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(created->editUrl, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(created->labelFormat, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(created->buttonTheme, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(created->listLog, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        created->comboFormat.Create(window, ID_FORMAT, created->theme);
        created->comboFormat.SetItems({
            {L"bestaudio", 1, false},
            {L"bestvideo", 2, true},
            {L"137 - mp4 1080p", 3, true},
            {L"140 - m4a 128k", 4, false},
        });
        created->comboFormat.SetSelection(0);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        AppendLog(created->listLog, L"demo ready");
        return 0;
    }
    case WM_SIZE:
        if (state) {
            Layout(window, state);
        }
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        if (state) {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, state->theme.text);
            if (message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX) {
                SetBkColor(dc, state->theme.panel);
                return reinterpret_cast<LRESULT>(state->brushPanel);
            }
            SetBkColor(dc, state->theme.background);
            return reinterpret_cast<LRESULT>(state->brushBackground);
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
    case WM_COMMAND:
        if (!state) {
            break;
        }
        if (LOWORD(wParam) == ID_SWITCH_THEME && HIWORD(wParam) == BN_CLICKED) {
            state->warmTheme = !state->warmTheme;
            ApplyTheme(state);
            AppendLog(state->listLog, state->warmTheme ? L"theme changed: warm" : L"theme changed: cool");
            return 0;
        }
        if (LOWORD(wParam) == ID_FORMAT && HIWORD(wParam) == CBN_SELCHANGE) {
            const int index = state->comboFormat.GetSelection();
            const auto item = state->comboFormat.GetItem(index);
            AppendLog(state->listLog, L"format selected: " + item.text);
            return 0;
        }
        break;
    case WM_DESTROY:
        if (state) {
            HFONT font = reinterpret_cast<HFONT>(GetPropW(window, L"DemoFont"));
            if (font) {
                DeleteObject(font);
                RemovePropW(window, L"DemoFont");
            }
            if (state->brushBackground) DeleteObject(state->brushBackground);
            if (state->brushPanel) DeleteObject(state->brushPanel);
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
                                  840,
                                  560,
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
