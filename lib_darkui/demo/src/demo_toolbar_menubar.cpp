#include <windows.h>
#include <commctrl.h>

#include <string>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiToolbarMenuBarDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui toolbar menubar demo";

enum ControlId {
    ID_TOOLBAR = 9101,
    ID_CMD_FILE = 9201,
    ID_CMD_EDIT,
    ID_CMD_VIEW,
    ID_CMD_WINDOW,
    ID_CMD_HELP,
    ID_CMD_THEME,
    ID_CMD_SHARE
};

struct DemoState {
    darkui::Theme theme;
    darkui::Toolbar toolbar;
    HBRUSH brushBackground = nullptr;
    HBRUSH brushPanel = nullptr;
    HFONT titleFont = nullptr;
    HFONT textFont = nullptr;
    std::wstring status = L"Ready";
    HMENU fileMenu = nullptr;
    HMENU editMenu = nullptr;
    HMENU viewMenu = nullptr;
    HMENU windowMenu = nullptr;
    HMENU helpMenu = nullptr;
};

void CleanupState(DemoState* state) {
    if (!state) return;
    if (state->brushBackground) DeleteObject(state->brushBackground);
    if (state->brushPanel) DeleteObject(state->brushPanel);
    if (state->titleFont) DeleteObject(state->titleFont);
    if (state->textFont) DeleteObject(state->textFont);
    if (state->fileMenu) DestroyMenu(state->fileMenu);
    if (state->editMenu) DestroyMenu(state->editMenu);
    if (state->viewMenu) DestroyMenu(state->viewMenu);
    if (state->windowMenu) DestroyMenu(state->windowMenu);
    if (state->helpMenu) DestroyMenu(state->helpMenu);
    delete state;
}

darkui::Theme MakeTheme() {
    darkui::Theme theme;
    theme.background = RGB(15, 17, 21);
    theme.panel = RGB(28, 31, 36);
    theme.border = RGB(58, 64, 74);
    theme.text = RGB(238, 241, 246);
    theme.mutedText = RGB(146, 154, 166);
    theme.toolbarBackground = RGB(24, 27, 31);
    theme.toolbarItem = RGB(24, 27, 31);
    theme.toolbarItemHot = RGB(52, 58, 67);
    theme.toolbarItemActive = RGB(71, 100, 156);
    theme.toolbarText = RGB(228, 232, 238);
    theme.toolbarTextActive = RGB(246, 248, 252);
    theme.toolbarSeparator = RGB(66, 72, 82);
    theme.popupItem = RGB(34, 38, 44);
    theme.popupItemHot = RGB(55, 63, 74);
    theme.popupAccentItem = RGB(52, 84, 138);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -19;
    theme.toolbarHeight = 34;
    theme.textPadding = 11;
    return theme;
}

HMENU CreateMenuFromItems(const std::initializer_list<std::pair<UINT, const wchar_t*>>& items) {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return nullptr;
    }
    for (const auto& item : items) {
        if (item.first == 0) {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        } else {
            AppendMenuW(menu, MF_STRING, item.first, item.second);
        }
    }
    return menu;
}

void Layout(HWND window, DemoState* state) {
    RECT client{};
    GetClientRect(window, &client);
    MoveWindow(state->toolbar.hwnd(), 0, 0, client.right, state->theme.toolbarHeight + 10, TRUE);
}

void DrawLine(HDC dc, HFONT font, COLORREF color, RECT rect, const wchar_t* text, UINT format) {
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, format | DT_NOPREFIX);
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
        created->brushPanel = CreateSolidBrush(created->theme.panel);

        darkui::FontSpec titleSpec = created->theme.uiFont;
        titleSpec.height = -30;
        titleSpec.weight = FW_SEMIBOLD;
        created->titleFont = darkui::CreateFont(titleSpec);
        created->textFont = darkui::CreateFont(created->theme.uiFont);
        if (!created->brushBackground || !created->brushPanel || !created->titleFont || !created->textFont) {
            CleanupState(created);
            return -1;
        }

        created->fileMenu = CreateMenuFromItems({
            {ID_CMD_FILE + 100, L"New Window"},
            {ID_CMD_FILE + 101, L"Open Project..."},
            {ID_CMD_FILE + 102, L"Open Recent"},
            {0, nullptr},
            {ID_CMD_FILE + 103, L"Save"},
            {ID_CMD_FILE + 104, L"Save As..."},
        });
        created->editMenu = CreateMenuFromItems({
            {ID_CMD_EDIT + 100, L"Undo"},
            {ID_CMD_EDIT + 101, L"Redo"},
            {0, nullptr},
            {ID_CMD_EDIT + 102, L"Cut"},
            {ID_CMD_EDIT + 103, L"Copy"},
            {ID_CMD_EDIT + 104, L"Paste"},
        });
        created->viewMenu = CreateMenuFromItems({
            {ID_CMD_VIEW + 100, L"Command Palette"},
            {ID_CMD_VIEW + 101, L"Sidebar"},
            {ID_CMD_VIEW + 102, L"Inspector"},
            {0, nullptr},
            {ID_CMD_VIEW + 103, L"Zen Mode"},
        });
        created->windowMenu = CreateMenuFromItems({
            {ID_CMD_WINDOW + 100, L"Minimize"},
            {ID_CMD_WINDOW + 101, L"Zoom"},
            {ID_CMD_WINDOW + 102, L"Bring All To Front"},
        });
        created->helpMenu = CreateMenuFromItems({
            {ID_CMD_HELP + 100, L"Quick Start"},
            {ID_CMD_HELP + 101, L"Keyboard Shortcuts"},
            {0, nullptr},
            {ID_CMD_HELP + 102, L"About lib_darkui"},
        });

        if (!created->fileMenu || !created->editMenu || !created->viewMenu || !created->windowMenu || !created->helpMenu) {
            CleanupState(created);
            return -1;
        }

        if (!created->toolbar.Create(window, ID_TOOLBAR, created->theme)) {
            CleanupState(created);
            return -1;
        }

        created->toolbar.SetItems({
            {L"File", ID_CMD_FILE, nullptr, created->fileMenu, 0, false, false, false, false, false, true},
            {L"Edit", ID_CMD_EDIT, nullptr, created->editMenu, 0, false, false, false, false, false, true},
            {L"View", ID_CMD_VIEW, nullptr, created->viewMenu, 0, false, false, false, false, false, true},
            {L"Window", ID_CMD_WINDOW, nullptr, created->windowMenu, 0, false, false, false, false, false, true},
            {L"Help", ID_CMD_HELP, nullptr, created->helpMenu, 0, false, false, false, false, false, true},
            {L"", 0, nullptr, nullptr, 0, true, false, false, true, false, false},
            {L"Theme", ID_CMD_THEME, nullptr, nullptr, 0, false, false, false, true, false, false},
            {L"Share", ID_CMD_SHARE, nullptr, nullptr, 0, false, false, false, true, false, false}
        });

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
        if (state) {
            switch (LOWORD(wParam)) {
            case ID_CMD_THEME:
                state->status = L"Theme panel opened";
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            case ID_CMD_SHARE:
                state->status = L"Share action triggered";
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            default:
                if (LOWORD(wParam) >= ID_CMD_FILE + 100 && LOWORD(wParam) <= ID_CMD_HELP + 199) {
                    state->status = L"Selected menu command ID " + std::to_wstring(LOWORD(wParam));
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
                break;
            }
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

            RECT heroRect{34, 74, client.right - 34, 254};
            FillRect(dc, &heroRect, state->brushPanel);

            RECT titleRect{34, 96, client.right - 34, 132};
            RECT descRect{34, 138, client.right - 34, 186};
            RECT noteRect{34, 196, client.right - 34, 244};
            RECT statusRect{34, client.bottom - 56, client.right - 34, client.bottom - 28};

            DrawLine(dc, state->titleFont, state->theme.text, titleRect, L"Toolbar As Menu Bar", DT_LEFT | DT_TOP | DT_SINGLELINE);
            DrawLine(dc, state->textFont, state->theme.mutedText, descRect, L"This demo uses the existing darkui::Toolbar as a menu-bar style control. Each top-level item is a drop-down toolbar button backed by a dark custom popup.", DT_LEFT | DT_TOP | DT_WORDBREAK);
            DrawTextW(dc,
                      L"Try File / Edit / View / Window / Help at the top. The right-aligned Theme and Share actions stay pinned to the edge like application chrome actions. This is the main path I would recommend instead of fighting the native light Win32 menu bar.",
                      -1,
                      &noteRect,
                      DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
            DrawLine(dc, state->textFont, state->theme.text, statusRect, state->status.c_str(), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

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
                                  980,
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
