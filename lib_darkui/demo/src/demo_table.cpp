#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <array>
#include <string>

#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiTableDemoWindow";
constexpr wchar_t kDemoTitle[] = L"lib_darkui table style demo";

constexpr int kTableX = 24;
constexpr int kTableY = 108;
constexpr int kTableWidth = 620;
constexpr int kTableHeight = 360;
constexpr int kPanelX = 680;
constexpr int kPanelY = 108;
constexpr int kPanelWidth = 250;
constexpr int kPanelButtonWidth = 150;
constexpr int kPanelButtonHeight = 30;
constexpr int kPanelRowGap = 16;
constexpr int kSwatchSize = 20;

enum ControlId {
    ID_TABLE = 2001,
    ID_PICK_TABLE_BG = 2101,
    ID_PICK_TABLE_TEXT,
    ID_PICK_HEADER_BG,
    ID_PICK_HEADER_TEXT,
    ID_PICK_GRID,
    ID_TOGGLE_EMPTY_GRID,
    ID_RESET_THEME
};

struct DemoState {
    darkui::Theme theme;
    darkui::Theme defaultTheme;
    darkui::Table table;
    HBRUSH brushBackground = nullptr;
    HFONT titleFont = nullptr;
    HFONT textFont = nullptr;
    std::array<COLORREF, 16> customColors{};
    HWND buttonTableBg = nullptr;
    HWND buttonTableText = nullptr;
    HWND buttonHeaderBg = nullptr;
    HWND buttonHeaderText = nullptr;
    HWND buttonGrid = nullptr;
    HWND checkEmptyGrid = nullptr;
    HWND buttonReset = nullptr;
};

darkui::Theme MakeTableTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.text = RGB(232, 236, 241);
    theme.mutedText = RGB(144, 151, 161);
    theme.tableBackground = RGB(25, 28, 33);
    theme.tableText = RGB(228, 232, 238);
    theme.tableHeaderBackground = RGB(38, 42, 48);
    theme.tableHeaderText = RGB(245, 247, 250);
    theme.tableGrid = RGB(69, 77, 89);
    theme.buttonHot = RGB(48, 73, 108);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -20;
    theme.tableRowHeight = 30;
    theme.tableHeaderHeight = 34;
    return theme;
}

void PopulateTable(DemoState* state) {
    state->table.SetColumns({
        {L"Control", 120},
        {L"Area", 120},
        {L"State", 120},
        {L"Notes", 120},
    });

    state->table.SetRows({
        {L"Table", L"Body", L"Dark", L"Custom painted body background"},
        {L"Table", L"Header", L"Dark", L"Header background and text are configurable"},
        {L"Theme", L"Grid", L"Live", L"Grid color updates immediately"},
        {L"Theme", L"Text", L"Live", L"Body text and header text are separate"},
        {L"Demo", L"Layout", L"Fixed", L"Table size stays fixed while window resizes"},
        {L"Demo", L"Style", L"Editable", L"Use the right panel to test customization"},
    });
}

void RecreateBackgroundBrush(DemoState* state) {
    if (state->brushBackground) {
        DeleteObject(state->brushBackground);
    }
    state->brushBackground = CreateSolidBrush(state->theme.background);
}

void ApplyTheme(HWND window, DemoState* state) {
    RecreateBackgroundBrush(state);
    state->table.SetTheme(state->theme);
    InvalidateRect(window, nullptr, TRUE);
}

void Layout(DemoState* state) {
    MoveWindow(state->table.hwnd(), kTableX, kTableY, kTableWidth, kTableHeight, TRUE);

    int y = kPanelY + 38;
    MoveWindow(state->buttonTableBg, kPanelX + 78, y, kPanelButtonWidth, kPanelButtonHeight, TRUE);
    y += kPanelButtonHeight + kPanelRowGap;
    MoveWindow(state->buttonTableText, kPanelX + 78, y, kPanelButtonWidth, kPanelButtonHeight, TRUE);
    y += kPanelButtonHeight + kPanelRowGap;
    MoveWindow(state->buttonHeaderBg, kPanelX + 78, y, kPanelButtonWidth, kPanelButtonHeight, TRUE);
    y += kPanelButtonHeight + kPanelRowGap;
    MoveWindow(state->buttonHeaderText, kPanelX + 78, y, kPanelButtonWidth, kPanelButtonHeight, TRUE);
    y += kPanelButtonHeight + kPanelRowGap;
    MoveWindow(state->buttonGrid, kPanelX + 78, y, kPanelButtonWidth, kPanelButtonHeight, TRUE);
    y += kPanelButtonHeight + 18;
    MoveWindow(state->checkEmptyGrid, kPanelX, y, kPanelWidth, 24, TRUE);
    y += 34;
    MoveWindow(state->buttonReset, kPanelX, y, kPanelWidth - 8, 36, TRUE);
}

bool PickColor(HWND window, DemoState* state, COLORREF* color) {
    CHOOSECOLORW cc{};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = window;
    cc.rgbResult = *color;
    cc.lpCustColors = state->customColors.data();
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&cc)) {
        return false;
    }
    *color = cc.rgbResult;
    return true;
}

void DrawTextLine(HDC dc, HFONT font, COLORREF color, RECT rect, const wchar_t* text, UINT format = DT_LEFT) {
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, format | DT_TOP | DT_NOPREFIX | DT_SINGLELINE);
    if (oldFont) {
        SelectObject(dc, oldFont);
    }
}

void DrawColorSwatch(HDC dc, int x, int y, COLORREF color, COLORREF border) {
    RECT swatch{x, y, x + kSwatchSize, y + kSwatchSize};
    HBRUSH fill = CreateSolidBrush(color);
    HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(dc, fill));
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(dc, pen));
    Rectangle(dc, swatch.left, swatch.top, swatch.right, swatch.bottom);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(fill);
}

void DrawStylePanel(HDC dc, DemoState* state) {
    RECT panelTitle{kPanelX, kPanelY, kPanelX + kPanelWidth, kPanelY + 24};
    DrawTextLine(dc, state->textFont, state->theme.text, panelTitle, L"Live Theme Controls");

    struct RowInfo {
        const wchar_t* label;
        COLORREF color;
    };
    const RowInfo rows[] = {
        {L"Body", state->theme.tableBackground},
        {L"Body Text", state->theme.tableText},
        {L"Header", state->theme.tableHeaderBackground},
        {L"Header Text", state->theme.tableHeaderText},
        {L"Grid", state->theme.tableGrid},
    };

    int y = kPanelY + 44;
    for (const auto& row : rows) {
        RECT labelRect{kPanelX, y + 6, kPanelX + 70, y + 24};
        DrawTextLine(dc, state->textFont, state->theme.mutedText, labelRect, row.label);
        DrawColorSwatch(dc, kPanelX + 238, y + 5, row.color, state->theme.tableGrid);
        y += kPanelButtonHeight + kPanelRowGap;
    }

    RECT infoRect{kPanelX, y + 58, kPanelX + kPanelWidth, y + 140};
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, state->theme.mutedText);
    DrawTextW(dc,
              L"Customizable areas: body background, body text, header background, header text, grid lines, and whether empty expanded space keeps drawing grid lines.",
              -1,
              &infoRect,
              DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DemoState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* created = new DemoState();
        created->defaultTheme = MakeTableTheme();
        created->theme = created->defaultTheme;
        RecreateBackgroundBrush(created);

        darkui::FontSpec titleSpec = created->theme.uiFont;
        titleSpec.height = -30;
        titleSpec.weight = FW_SEMIBOLD;
        created->titleFont = darkui::CreateFont(titleSpec);
        created->textFont = darkui::CreateFont(created->theme.uiFont);

        HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(window, GWLP_HINSTANCE));
        created->table.Create(window, ID_TABLE, created->theme, WS_CHILD | WS_VISIBLE | WS_TABSTOP);
        PopulateTable(created);

        created->buttonTableBg = CreateWindowExW(0, L"BUTTON", L"Pick Color", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                                 0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PICK_TABLE_BG)), instance, nullptr);
        created->buttonTableText = CreateWindowExW(0, L"BUTTON", L"Pick Color", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                                   0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PICK_TABLE_TEXT)), instance, nullptr);
        created->buttonHeaderBg = CreateWindowExW(0, L"BUTTON", L"Pick Color", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                                  0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PICK_HEADER_BG)), instance, nullptr);
        created->buttonHeaderText = CreateWindowExW(0, L"BUTTON", L"Pick Color", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                                    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PICK_HEADER_TEXT)), instance, nullptr);
        created->buttonGrid = CreateWindowExW(0, L"BUTTON", L"Pick Color", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                              0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PICK_GRID)), instance, nullptr);
        created->checkEmptyGrid = CreateWindowExW(0, L"BUTTON", L"Draw grid in empty expanded area", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                                  0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TOGGLE_EMPTY_GRID)), instance, nullptr);
        created->buttonReset = CreateWindowExW(0, L"BUTTON", L"Reset Theme", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                               0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_RESET_THEME)), instance, nullptr);

        SendMessageW(created->buttonTableBg, WM_SETFONT, reinterpret_cast<WPARAM>(created->textFont), TRUE);
        SendMessageW(created->buttonTableText, WM_SETFONT, reinterpret_cast<WPARAM>(created->textFont), TRUE);
        SendMessageW(created->buttonHeaderBg, WM_SETFONT, reinterpret_cast<WPARAM>(created->textFont), TRUE);
        SendMessageW(created->buttonHeaderText, WM_SETFONT, reinterpret_cast<WPARAM>(created->textFont), TRUE);
        SendMessageW(created->buttonGrid, WM_SETFONT, reinterpret_cast<WPARAM>(created->textFont), TRUE);
        SendMessageW(created->checkEmptyGrid, WM_SETFONT, reinterpret_cast<WPARAM>(created->textFont), TRUE);
        SendMessageW(created->buttonReset, WM_SETFONT, reinterpret_cast<WPARAM>(created->textFont), TRUE);
        SendMessageW(created->checkEmptyGrid, BM_SETCHECK, BST_CHECKED, 0);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        Layout(created);
        return 0;
    }
    case WM_SIZE:
        if (state) {
            Layout(state);
        }
        return 0;
    case WM_COMMAND:
        if (!state || HIWORD(wParam) != BN_CLICKED) {
            break;
        }
        switch (LOWORD(wParam)) {
        case ID_PICK_TABLE_BG:
            if (PickColor(window, state, &state->theme.tableBackground)) {
                ApplyTheme(window, state);
            }
            return 0;
        case ID_PICK_TABLE_TEXT:
            if (PickColor(window, state, &state->theme.tableText)) {
                ApplyTheme(window, state);
            }
            return 0;
        case ID_PICK_HEADER_BG:
            if (PickColor(window, state, &state->theme.tableHeaderBackground)) {
                ApplyTheme(window, state);
            }
            return 0;
        case ID_PICK_HEADER_TEXT:
            if (PickColor(window, state, &state->theme.tableHeaderText)) {
                ApplyTheme(window, state);
            }
            return 0;
        case ID_PICK_GRID:
            if (PickColor(window, state, &state->theme.tableGrid)) {
                ApplyTheme(window, state);
            }
            return 0;
        case ID_TOGGLE_EMPTY_GRID:
            state->table.SetDrawEmptyGrid(SendMessageW(state->checkEmptyGrid, BM_GETCHECK, 0, 0) == BST_CHECKED);
            InvalidateRect(window, nullptr, TRUE);
            return 0;
        case ID_RESET_THEME:
            state->theme = state->defaultTheme;
            ApplyTheme(window, state);
            state->table.SetDrawEmptyGrid(true);
            SendMessageW(state->checkEmptyGrid, BM_SETCHECK, BST_CHECKED, 0);
            return 0;
        default:
            break;
        }
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
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

            RECT titleRect{24, 20, client.right - 24, 54};
            RECT descRect{24, 58, client.right - 24, 84};
            DrawTextLine(dc, state->titleFont, state->theme.text, titleRect, L"Dark Table Style Demo");
            DrawTextLine(dc, state->textFont, state->theme.mutedText, descRect,
                         L"The table size is fixed. Use the right panel to modify each customizable table color in real time.");
            DrawStylePanel(dc, state);

            EndPaint(window, &ps);
            return 0;
        }
        break;
    case WM_DESTROY:
        if (state) {
            if (state->brushBackground) {
                DeleteObject(state->brushBackground);
            }
            if (state->titleFont) {
                DeleteObject(state->titleFont);
            }
            if (state->textFont) {
                DeleteObject(state->textFont);
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
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES};
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
                                  620,
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
