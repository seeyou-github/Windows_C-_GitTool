#include <windows.h>
#include <commctrl.h>

#include <string>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiTabDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui tab demo";

enum ControlId {
    ID_TAB = 7001,
    ID_PAGE_ONE = 7101,
    ID_PAGE_TWO,
    ID_PAGE_THREE
};

struct DemoState {
    darkui::Theme theme;
    darkui::Tab tab;
    HWND pageOne = nullptr;
    HWND pageTwo = nullptr;
    HWND pageThree = nullptr;
    HBRUSH brushBackground = nullptr;
    HFONT titleFont = nullptr;
    HFONT textFont = nullptr;
};

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.text = RGB(232, 236, 241);
    theme.mutedText = RGB(144, 151, 161);
    theme.tabBackground = RGB(24, 27, 31);
    theme.tabItem = RGB(48, 53, 60);
    theme.tabItemActive = RGB(78, 120, 184);
    theme.tabText = RGB(206, 211, 218);
    theme.tabTextActive = RGB(245, 247, 250);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    theme.tabHeight = 38;
    return theme;
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

void RecreateBackgroundBrush(DemoState* state) {
    if (state->brushBackground) {
        DeleteObject(state->brushBackground);
    }
    state->brushBackground = CreateSolidBrush(state->theme.background);
}

HWND CreatePage(HWND parent, int controlId, const wchar_t* text, HFONT font) {
    HWND page = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
                                0, 0, 0, 0, parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)),
                                nullptr);
    if (page && font) {
        SendMessageW(page, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    return page;
}

void Layout(HWND window, DemoState* state) {
    RECT client{};
    GetClientRect(window, &client);
    MoveWindow(state->tab.hwnd(), 32, 104, client.right - 64, client.bottom - 136, TRUE);
}

RECT GetDescriptionRect(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    return RECT{32, 64, client.right - 32, 118};
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

        if (!created->tab.Create(window, ID_TAB, created->theme)) {
            CleanupState(created);
            return -1;
        }

        created->tab.SetItems({
            {L"Overview", 1},
            {L"Metrics", 2},
            {L"Notes", 3},
        });

        created->pageOne = CreatePage(created->tab.hwnd(), ID_PAGE_ONE, L"Overview page content", created->textFont);
        created->pageTwo = CreatePage(created->tab.hwnd(), ID_PAGE_TWO, L"Metrics page content", created->textFont);
        created->pageThree = CreatePage(created->tab.hwnd(), ID_PAGE_THREE, L"Notes page content", created->textFont);
        if (!created->pageOne || !created->pageTwo || !created->pageThree) {
            CleanupState(created);
            return -1;
        }

        created->tab.AttachPage(0, created->pageOne);
        created->tab.AttachPage(1, created->pageTwo);
        created->tab.AttachPage(2, created->pageThree);
        created->tab.SetSelection(0, false);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(window, created);
        return 0;
    }
    case WM_SIZE:
        if (state) {
            Layout(window, state);
        }
        return 0;
    case WM_NOTIFY:
        if (state) {
            auto* hdr = reinterpret_cast<NMHDR*>(lParam);
            if (hdr && hdr->hwndFrom == state->tab.hwnd() && hdr->code == TCN_SELCHANGE) {
                std::wstring title = L"Tab: " + state->tab.GetItem(state->tab.GetSelection()).text;
                SetWindowTextW(window, title.c_str());
                return 0;
            }
        }
        break;
    case WM_CTLCOLORSTATIC:
        if (state) {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, state->theme.text);
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
    case WM_PAINT:
        if (state) {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);
            RECT client{};
            GetClientRect(window, &client);
            FillRect(dc, &client, state->brushBackground);

            RECT titleRect{32, 24, client.right - 32, 56};
            RECT descRect = GetDescriptionRect(window);

            HFONT oldTitle = reinterpret_cast<HFONT>(SelectObject(dc, state->titleFont));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, state->theme.text);
            DrawTextW(dc, L"Dark Tab Demo", -1, &titleRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, oldTitle);

            HFONT oldText = reinterpret_cast<HFONT>(SelectObject(dc, state->textFont));
            SetTextColor(dc, state->theme.mutedText);
            DrawTextW(dc, L"Custom tab strip with external page windows and standard TCN_SELCHANGE notification.", -1, &descRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
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
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES | ICC_TAB_CLASSES};
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
                                  780,
                                  520,
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
