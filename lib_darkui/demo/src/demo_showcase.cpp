#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "darkui/button.h"
#include "darkui/darkui.h"

namespace {

constexpr wchar_t kDemoClassName[] = L"DarkUiShowcaseWindow";
constexpr wchar_t kPageClassName[] = L"DarkUiShowcasePage";
constexpr wchar_t kDemoTitle[] = L"lib_darkui showcase";

enum ControlId {
    ID_THEME_GRAPHITE = 9001,
    ID_THEME_OBSIDIAN,
    ID_THEME_NOCTURNE,
    ID_MAIN_TAB,

    ID_OVERVIEW_PROFILE = 9101,
    ID_OVERVIEW_SURFACE,
    ID_OVERVIEW_PRIMARY,
    ID_OVERVIEW_SECONDARY,
    ID_OVERVIEW_DISABLED,
    ID_OVERVIEW_STORAGE,
    ID_OVERVIEW_INDEX,

    ID_CONTROLS_EXPOSURE = 9201,
    ID_CONTROLS_BALANCE,
    ID_CONTROLS_SCROLL_H,
    ID_CONTROLS_SCROLL_V,
    ID_CONTROLS_PREVIEW,

    ID_DATA_FILTER = 9301,
    ID_DATA_REFRESH,
    ID_DATA_ARCHIVE,
    ID_DATA_TABLE,

    ID_PAGE_OVERVIEW = 9401,
    ID_PAGE_CONTROLS,
    ID_PAGE_DATA
};

enum class PageKind {
    Overview,
    Controls,
    Data
};

struct OverviewControls {
    darkui::ComboBox profile;
    darkui::ComboBox surface;
    darkui::Button primary;
    darkui::Button secondary;
    darkui::Button disabled;
    darkui::ProgressBar storage;
    darkui::ProgressBar index;
};

struct ControlsPanel {
    darkui::Slider exposure;
    darkui::Slider balance;
    darkui::ScrollBar timeline;
    darkui::ScrollBar navigator;
    darkui::ProgressBar preview;
};

struct DataPanel {
    darkui::ComboBox filter;
    darkui::Button refresh;
    darkui::Button archive;
    darkui::Table table;
};

struct AppState {
    darkui::Theme theme;
    int themeIndex = 0;

    darkui::Tab tab;
    darkui::Button themeGraphite;
    darkui::Button themeObsidian;
    darkui::Button themeNocturne;

    HWND overviewPage = nullptr;
    HWND controlsPage = nullptr;
    HWND dataPage = nullptr;

    OverviewControls overview;
    ControlsPanel controls;
    DataPanel data;

    HBRUSH windowBrush = nullptr;
    HFONT titleFont = nullptr;
    HFONT subtitleFont = nullptr;
    HFONT sectionFont = nullptr;
    HFONT bodyFont = nullptr;
};

struct PageState {
    AppState* app = nullptr;
    PageKind kind = PageKind::Overview;
};

darkui::Theme MakeGraphiteTheme();
darkui::Theme MakeObsidianTheme();
darkui::Theme MakeNocturneTheme();
darkui::Theme MakeThemeByIndex(int index);

void CleanupAppState(AppState* state);
void RecreateWindowBrush(AppState* state);
void ApplyTheme(AppState* state, int themeIndex);
void LayoutMainWindow(HWND window, AppState* state);
void LayoutOverviewPage(HWND page, AppState* state);
void LayoutControlsPage(HWND page, AppState* state);
void LayoutDataPage(HWND page, AppState* state);
void LayoutPage(HWND page, AppState* state, PageKind kind);
void PaintMainWindow(HWND window, HDC dc, AppState* state);
void PaintPage(HWND page, HDC dc, PageState* state);
void UpdateControlPreview(AppState* state);
void RefreshDataRows(AppState* state);

RECT InsetRectCopy(const RECT& rect, int dx, int dy);
RECT SliceTop(const RECT& rect, int height);
RECT SliceLeft(const RECT& rect, int width);
RECT SliceRight(const RECT& rect, int width);
void FillRoundedRect(HDC dc, const RECT& rect, int radius, COLORREF fill, COLORREF border);
void DrawLabel(HDC dc, HFONT font, COLORREF color, const RECT& rect, const wchar_t* text, UINT format);

LRESULT CALLBACK ShowcaseWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ShowcasePageProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

darkui::Theme MakeGraphiteTheme() {
    darkui::Theme theme;
    theme.background = RGB(18, 20, 24);
    theme.panel = RGB(32, 35, 40);
    theme.button = RGB(56, 62, 71);
    theme.buttonHover = RGB(70, 77, 88);
    theme.buttonHot = RGB(82, 90, 102);
    theme.buttonDisabled = RGB(43, 46, 52);
    theme.buttonDisabledText = RGB(118, 126, 136);
    theme.border = RGB(60, 66, 76);
    theme.text = RGB(236, 239, 244);
    theme.mutedText = RGB(142, 149, 160);
    theme.arrow = RGB(176, 181, 190);
    theme.popupItem = RGB(36, 39, 44);
    theme.popupItemHot = RGB(57, 64, 74);
    theme.popupAccentItem = RGB(42, 68, 114);
    theme.popupAccentItemHot = RGB(55, 86, 144);
    theme.tableBackground = RGB(25, 28, 32);
    theme.tableText = RGB(232, 236, 241);
    theme.tableHeaderBackground = RGB(37, 41, 47);
    theme.tableHeaderText = RGB(244, 247, 250);
    theme.tableGrid = RGB(58, 63, 72);
    theme.sliderBackground = RGB(29, 32, 37);
    theme.sliderTrack = RGB(58, 64, 72);
    theme.sliderFill = RGB(104, 142, 214);
    theme.sliderThumb = RGB(244, 247, 250);
    theme.sliderThumbHot = RGB(255, 255, 255);
    theme.sliderTick = RGB(98, 108, 122);
    theme.progressBackground = RGB(25, 28, 32);
    theme.progressTrack = RGB(50, 56, 64);
    theme.progressFill = RGB(104, 142, 214);
    theme.progressText = RGB(248, 250, 252);
    theme.scrollBarBackground = RGB(25, 28, 32);
    theme.scrollBarTrack = RGB(54, 59, 67);
    theme.scrollBarThumb = RGB(139, 149, 163);
    theme.scrollBarThumbHot = RGB(174, 183, 196);
    theme.tabBackground = RGB(22, 25, 29);
    theme.tabItem = RGB(36, 40, 46);
    theme.tabItemActive = RGB(67, 92, 140);
    theme.tabText = RGB(205, 211, 219);
    theme.tabTextActive = RGB(248, 250, 252);
    theme.uiFont.family = L"Segoe UI";
    theme.uiFont.height = -19;
    theme.textPadding = 12;
    theme.itemHeight = 30;
    theme.tableRowHeight = 32;
    theme.tableHeaderHeight = 34;
    theme.sliderTrackHeight = 8;
    theme.sliderThumbRadius = 10;
    theme.progressHeight = 14;
    theme.scrollBarThickness = 16;
    theme.scrollBarMinThumbSize = 36;
    theme.tabHeight = 48;
    theme.tabWidth = 196;
    return theme;
}

darkui::Theme MakeObsidianTheme() {
    darkui::Theme theme = MakeGraphiteTheme();
    theme.background = RGB(14, 15, 19);
    theme.panel = RGB(26, 28, 34);
    theme.button = RGB(60, 52, 74);
    theme.buttonHover = RGB(76, 65, 94);
    theme.buttonHot = RGB(91, 78, 112);
    theme.border = RGB(74, 70, 86);
    theme.popupAccentItem = RGB(76, 53, 126);
    theme.popupAccentItemHot = RGB(96, 67, 153);
    theme.sliderFill = RGB(147, 108, 218);
    theme.progressFill = RGB(147, 108, 218);
    theme.tabItemActive = RGB(94, 72, 144);
    theme.scrollBarThumb = RGB(148, 137, 165);
    theme.scrollBarThumbHot = RGB(183, 170, 202);
    return theme;
}

darkui::Theme MakeNocturneTheme() {
    darkui::Theme theme = MakeGraphiteTheme();
    theme.background = RGB(16, 22, 26);
    theme.panel = RGB(24, 32, 38);
    theme.button = RGB(38, 82, 86);
    theme.buttonHover = RGB(46, 99, 103);
    theme.buttonHot = RGB(54, 119, 123);
    theme.border = RGB(46, 78, 82);
    theme.popupAccentItem = RGB(25, 92, 96);
    theme.popupAccentItemHot = RGB(31, 116, 121);
    theme.sliderFill = RGB(58, 185, 172);
    theme.progressFill = RGB(58, 185, 172);
    theme.tabItemActive = RGB(32, 103, 109);
    theme.scrollBarThumb = RGB(91, 153, 153);
    theme.scrollBarThumbHot = RGB(116, 185, 184);
    return theme;
}

darkui::Theme MakeThemeByIndex(int index) {
    switch (index) {
    case 1:
        return MakeObsidianTheme();
    case 2:
        return MakeNocturneTheme();
    default:
        return MakeGraphiteTheme();
    }
}

RECT InsetRectCopy(const RECT& rect, int dx, int dy) {
    RECT rc = rect;
    rc.left += dx;
    rc.top += dy;
    rc.right -= dx;
    rc.bottom -= dy;
    return rc;
}

RECT SliceTop(const RECT& rect, int height) {
    RECT rc = rect;
    rc.bottom = std::min(rect.bottom, rect.top + height);
    return rc;
}

RECT SliceLeft(const RECT& rect, int width) {
    RECT rc = rect;
    rc.right = std::min(rect.right, rect.left + width);
    return rc;
}

RECT SliceRight(const RECT& rect, int width) {
    RECT rc = rect;
    rc.left = std::max(rect.left, rect.right - width);
    return rc;
}

void FillRoundedRect(HDC dc, const RECT& rect, int radius, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void DrawLabel(HDC dc, HFONT font, COLORREF color, const RECT& rect, const wchar_t* text, UINT format) {
    HFONT oldFont = font ? reinterpret_cast<HFONT>(SelectObject(dc, font)) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, const_cast<RECT*>(&rect), format | DT_NOPREFIX);
    if (oldFont) {
        SelectObject(dc, oldFont);
    }
}

void CleanupAppState(AppState* state) {
    if (!state) {
        return;
    }
    if (state->windowBrush) DeleteObject(state->windowBrush);
    if (state->titleFont) DeleteObject(state->titleFont);
    if (state->subtitleFont) DeleteObject(state->subtitleFont);
    if (state->sectionFont) DeleteObject(state->sectionFont);
    if (state->bodyFont) DeleteObject(state->bodyFont);
    delete state;
}

void RecreateWindowBrush(AppState* state) {
    if (state->windowBrush) {
        DeleteObject(state->windowBrush);
    }
    state->windowBrush = CreateSolidBrush(state->theme.background);
}

void RefreshDataRows(AppState* state) {
    const int filter = state->data.filter.GetSelection();
    std::vector<darkui::TableRow> rows{
        {L"Studio render", L"Local", L"Queued", L"4.8 GB", L"02:14"},
        {L"Client review", L"Cloud", L"Syncing", L"1.2 GB", L"Live"},
        {L"Product clips", L"SSD", L"Ready", L"864 MB", L"11:07"},
        {L"Marketing pack", L"Archive", L"Indexed", L"12.4 GB", L"Yesterday"},
        {L"Audio stems", L"NAS", L"Ready", L"2.1 GB", L"08:42"},
        {L"Shot selects", L"Cloud", L"Rendering", L"6.3 GB", L"Now"},
        {L"Motion boards", L"Local", L"Ready", L"742 MB", L"13:55"},
        {L"Launch deck", L"Archive", L"Locked", L"318 MB", L"Mon"}
    };

    if (filter == 1) {
        rows.erase(std::remove_if(rows.begin(), rows.end(), [](const darkui::TableRow& row) { return row[2] != L"Ready"; }), rows.end());
    } else if (filter == 2) {
        rows.erase(std::remove_if(rows.begin(), rows.end(), [](const darkui::TableRow& row) { return row[1] != L"Cloud"; }), rows.end());
    } else if (filter == 3) {
        rows.erase(std::remove_if(rows.begin(), rows.end(), [](const darkui::TableRow& row) { return row[3].find(L"GB") == std::wstring::npos; }), rows.end());
    }

    state->data.table.SetRows(rows);
}

void UpdateControlPreview(AppState* state) {
    const int exposure = state->controls.exposure.GetValue();
    const int balance = state->controls.balance.GetValue();
    const int timeline = state->controls.timeline.GetValue();
    const int navigator = state->controls.navigator.GetValue();
    const int composed = std::clamp((exposure + balance + (timeline / 2) + navigator) / 3, 0, 100);
    state->controls.preview.SetValue(composed);
}

void ApplyTheme(AppState* state, int themeIndex) {
    state->themeIndex = themeIndex;
    state->theme = MakeThemeByIndex(themeIndex);
    RecreateWindowBrush(state);

    state->tab.SetTheme(state->theme);
    state->themeGraphite.SetTheme(state->theme);
    state->themeObsidian.SetTheme(state->theme);
    state->themeNocturne.SetTheme(state->theme);
    state->themeGraphite.SetSurfaceColor(state->theme.background);
    state->themeObsidian.SetSurfaceColor(state->theme.background);
    state->themeNocturne.SetSurfaceColor(state->theme.background);

    state->overview.profile.SetTheme(state->theme);
    state->overview.surface.SetTheme(state->theme);
    state->overview.primary.SetTheme(state->theme);
    state->overview.secondary.SetTheme(state->theme);
    state->overview.disabled.SetTheme(state->theme);
    state->overview.primary.SetSurfaceColor(state->theme.panel);
    state->overview.secondary.SetSurfaceColor(state->theme.panel);
    state->overview.disabled.SetSurfaceColor(state->theme.panel);
    state->overview.storage.SetTheme(state->theme);
    state->overview.index.SetTheme(state->theme);
    state->overview.storage.SetSurfaceColor(state->theme.panel);
    state->overview.index.SetSurfaceColor(state->theme.panel);

    state->controls.exposure.SetTheme(state->theme);
    state->controls.balance.SetTheme(state->theme);
    state->controls.timeline.SetTheme(state->theme);
    state->controls.navigator.SetTheme(state->theme);
    state->controls.preview.SetTheme(state->theme);
    state->controls.preview.SetSurfaceColor(state->theme.panel);

    state->data.filter.SetTheme(state->theme);
    state->data.refresh.SetTheme(state->theme);
    state->data.archive.SetTheme(state->theme);
    state->data.refresh.SetSurfaceColor(state->theme.panel);
    state->data.archive.SetSurfaceColor(state->theme.panel);
    state->data.table.SetTheme(state->theme);

    InvalidateRect(state->overviewPage, nullptr, TRUE);
    InvalidateRect(state->controlsPage, nullptr, TRUE);
    InvalidateRect(state->dataPage, nullptr, TRUE);
}

void LayoutMainWindow(HWND window, AppState* state) {
    RECT client{};
    GetClientRect(window, &client);

    const int outer = 28;
    const int headerHeight = 96;
    const int buttonWidth = 118;
    const int buttonHeight = 36;
    const int buttonGap = 12;

    const int rightEdge = client.right - outer;
    const int topY = outer + 10;

    MoveWindow(state->themeNocturne.hwnd(), rightEdge - buttonWidth, topY, buttonWidth, buttonHeight, TRUE);
    MoveWindow(state->themeObsidian.hwnd(), rightEdge - (buttonWidth * 2) - buttonGap, topY, buttonWidth, buttonHeight, TRUE);
    MoveWindow(state->themeGraphite.hwnd(), rightEdge - (buttonWidth * 3) - (buttonGap * 2), topY, buttonWidth, buttonHeight, TRUE);

    MoveWindow(state->tab.hwnd(),
               outer,
               outer + headerHeight,
               std::max(420, static_cast<int>(client.right - outer * 2)),
               std::max(420, static_cast<int>(client.bottom - outer * 2 - headerHeight)),
               TRUE);
}

void LayoutOverviewPage(HWND page, AppState* state) {
    RECT client{};
    GetClientRect(page, &client);
    RECT content = InsetRectCopy(client, 28, 28);

    const bool compact = (content.right - content.left) < 920;
    const int gap = 20;

    RECT topArea = SliceTop(content, compact ? 336 : 250);
    RECT bottomArea{content.left, topArea.bottom + gap, content.right, content.bottom};

    RECT leftCard = compact ? SliceTop(topArea, 162) : SliceLeft(topArea, ((topArea.right - topArea.left) - gap) / 2);
    RECT rightCard = compact ? RECT{topArea.left, leftCard.bottom + gap, topArea.right, topArea.bottom}
                             : RECT{leftCard.right + gap, topArea.top, topArea.right, topArea.bottom};

    RECT leftInner = InsetRectCopy(leftCard, 24, 24);
    RECT rightInner = InsetRectCopy(rightCard, 24, 24);
    RECT bottomInner = InsetRectCopy(bottomArea, 24, 24);

    MoveWindow(state->overview.profile.hwnd(), leftInner.left, leftInner.top + 54, leftInner.right - leftInner.left, 34, TRUE);
    MoveWindow(state->overview.surface.hwnd(), leftInner.left, leftInner.top + 116, leftInner.right - leftInner.left, 34, TRUE);

    const int actionWidth = compact ? (rightInner.right - rightInner.left - gap) / 2 : 168;
    MoveWindow(state->overview.primary.hwnd(), rightInner.left, rightInner.top + 56, actionWidth, 38, TRUE);
    MoveWindow(state->overview.secondary.hwnd(), rightInner.left + actionWidth + gap, rightInner.top + 56, actionWidth, 38, TRUE);
    MoveWindow(state->overview.disabled.hwnd(), rightInner.left, rightInner.top + 110, rightInner.right - rightInner.left, 38, TRUE);

    MoveWindow(state->overview.storage.hwnd(), bottomInner.left, bottomInner.top + 58, bottomInner.right - bottomInner.left, 34, TRUE);
    MoveWindow(state->overview.index.hwnd(), bottomInner.left, bottomInner.top + 134, bottomInner.right - bottomInner.left, 30, TRUE);
}

void LayoutControlsPage(HWND page, AppState* state) {
    RECT client{};
    GetClientRect(page, &client);
    RECT content = InsetRectCopy(client, 28, 28);
    const bool compact = (content.right - content.left) < 960;
    const int gap = 20;

    RECT topCard = SliceTop(content, 264);
    RECT bottomArea{content.left, topCard.bottom + gap, content.right, content.bottom};
    RECT leftBottom = compact ? SliceTop(bottomArea, 170) : SliceLeft(bottomArea, ((bottomArea.right - bottomArea.left) - gap) / 2);
    RECT rightBottom = compact ? RECT{bottomArea.left, leftBottom.bottom + gap, bottomArea.right, bottomArea.bottom}
                               : RECT{leftBottom.right + gap, bottomArea.top, bottomArea.right, bottomArea.bottom};

    RECT topInner = InsetRectCopy(topCard, 24, 24);
    RECT leftInner = InsetRectCopy(leftBottom, 24, 24);
    RECT rightInner = InsetRectCopy(rightBottom, 24, 24);

    MoveWindow(state->controls.exposure.hwnd(), topInner.left, topInner.top + 64, topInner.right - topInner.left, 42, TRUE);
    MoveWindow(state->controls.balance.hwnd(), topInner.left, topInner.top + 156, topInner.right - topInner.left, 42, TRUE);

    MoveWindow(state->controls.timeline.hwnd(), leftInner.left, leftInner.top + 74, leftInner.right - leftInner.left, state->theme.scrollBarThickness + 14, TRUE);
    MoveWindow(state->controls.navigator.hwnd(), leftInner.right - 24, leftInner.top + 124, 20, std::max(96, static_cast<int>(leftInner.bottom - leftInner.top - 136)), TRUE);

    MoveWindow(state->controls.preview.hwnd(), rightInner.left, rightInner.top + 78, rightInner.right - rightInner.left, 40, TRUE);
}

void LayoutDataPage(HWND page, AppState* state) {
    RECT client{};
    GetClientRect(page, &client);
    RECT content = InsetRectCopy(client, 28, 28);
    const int gap = 12;

    RECT toolbar = SliceTop(content, 42);
    RECT tableArea{content.left, toolbar.bottom + 20, content.right, content.bottom};

    const int rightButtons = 150;
    RECT filterRect = SliceLeft(toolbar, std::max(220, static_cast<int>((toolbar.right - toolbar.left) / 2)));
    RECT archiveRect = SliceRight(toolbar, rightButtons);
    RECT refreshRect{archiveRect.left - gap - rightButtons, toolbar.top, archiveRect.left - gap, toolbar.bottom};

    MoveWindow(state->data.filter.hwnd(), filterRect.left, filterRect.top, filterRect.right - filterRect.left, filterRect.bottom - filterRect.top, TRUE);
    MoveWindow(state->data.refresh.hwnd(), refreshRect.left, refreshRect.top, refreshRect.right - refreshRect.left, refreshRect.bottom - refreshRect.top, TRUE);
    MoveWindow(state->data.archive.hwnd(), archiveRect.left, archiveRect.top, archiveRect.right - archiveRect.left, archiveRect.bottom - archiveRect.top, TRUE);
    MoveWindow(state->data.table.hwnd(), tableArea.left, tableArea.top, tableArea.right - tableArea.left, tableArea.bottom - tableArea.top, TRUE);
}

void LayoutPage(HWND page, AppState* state, PageKind kind) {
    switch (kind) {
    case PageKind::Overview:
        LayoutOverviewPage(page, state);
        break;
    case PageKind::Controls:
        LayoutControlsPage(page, state);
        break;
    case PageKind::Data:
        LayoutDataPage(page, state);
        break;
    }
}

void PaintMainWindow(HWND window, HDC dc, AppState* state) {
    RECT client{};
    GetClientRect(window, &client);
    FillRect(dc, &client, state->windowBrush);

    RECT hero{28, 26, client.right - 28, 90};
    RECT titleRect = hero;
    titleRect.bottom = titleRect.top + 34;
    RECT subtitleRect{hero.left, hero.top + 40, client.right - 380, hero.bottom + 18};

    DrawLabel(dc, state->titleFont, state->theme.text, titleRect, L"DarkUI Studio Showcase", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc,
              state->subtitleFont,
              state->theme.mutedText,
              subtitleRect,
              L"A unified dark workspace that exercises every custom control with live themes, sample content, and adaptive layout.",
              DT_LEFT | DT_TOP | DT_WORDBREAK);
}

void PaintOverviewPage(HWND page, HDC dc, PageState* state) {
    RECT client{};
    GetClientRect(page, &client);
    FillRect(dc, &client, state->app->windowBrush);

    RECT content = InsetRectCopy(client, 28, 28);
    const bool compact = (content.right - content.left) < 920;
    const int gap = 20;
    RECT topArea = SliceTop(content, compact ? 336 : 250);
    RECT bottomArea{content.left, topArea.bottom + gap, content.right, content.bottom};
    RECT leftCard = compact ? SliceTop(topArea, 162) : SliceLeft(topArea, ((topArea.right - topArea.left) - gap) / 2);
    RECT rightCard = compact ? RECT{topArea.left, leftCard.bottom + gap, topArea.right, topArea.bottom}
                             : RECT{leftCard.right + gap, topArea.top, topArea.right, topArea.bottom};

    FillRoundedRect(dc, leftCard, 24, state->app->theme.panel, state->app->theme.border);
    FillRoundedRect(dc, rightCard, 24, state->app->theme.panel, state->app->theme.border);
    FillRoundedRect(dc, bottomArea, 24, state->app->theme.panel, state->app->theme.border);

    RECT leftInner = InsetRectCopy(leftCard, 24, 24);
    RECT rightInner = InsetRectCopy(rightCard, 24, 24);
    RECT bottomInner = InsetRectCopy(bottomArea, 24, 24);

    DrawLabel(dc, state->app->sectionFont, state->app->theme.text, SliceTop(leftInner, 26), L"Workspace Profile", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->bodyFont, state->app->theme.mutedText, RECT{leftInner.left, leftInner.top + 30, leftInner.right, leftInner.top + 48}, L"Choose a polished starting point for cards, density, and data presentation.", DT_LEFT | DT_TOP | DT_WORDBREAK);
    DrawLabel(dc, state->app->bodyFont, state->app->theme.mutedText, RECT{leftInner.left, leftInner.top + 58, leftInner.right, leftInner.top + 76}, L"Profile", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->bodyFont, state->app->theme.mutedText, RECT{leftInner.left, leftInner.top + 120, leftInner.right, leftInner.top + 138}, L"Surface style", DT_LEFT | DT_TOP | DT_SINGLELINE);

    DrawLabel(dc, state->app->sectionFont, state->app->theme.text, SliceTop(rightInner, 26), L"Quick Actions", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->bodyFont, state->app->theme.mutedText, RECT{rightInner.left, rightInner.top + 30, rightInner.right, rightInner.top + 48}, L"Buttons showcase corner radius, hover, disabled styling, and compact toolbar placement.", DT_LEFT | DT_TOP | DT_WORDBREAK);

    DrawLabel(dc, state->app->sectionFont, state->app->theme.text, SliceTop(bottomInner, 26), L"Operational Health", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->bodyFont, state->app->theme.mutedText, RECT{bottomInner.left, bottomInner.top + 32, bottomInner.right, bottomInner.top + 50}, L"Storage pool", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->bodyFont, state->app->theme.mutedText, RECT{bottomInner.left, bottomInner.top + 108, bottomInner.right, bottomInner.top + 126}, L"Search indexing", DT_LEFT | DT_TOP | DT_SINGLELINE);
}

void PaintControlsPage(HWND page, HDC dc, PageState* state) {
    RECT client{};
    GetClientRect(page, &client);
    FillRect(dc, &client, state->app->windowBrush);

    RECT content = InsetRectCopy(client, 28, 28);
    const bool compact = (content.right - content.left) < 960;
    const int gap = 20;
    RECT topCard = SliceTop(content, 264);
    RECT bottomArea{content.left, topCard.bottom + gap, content.right, content.bottom};
    RECT leftBottom = compact ? SliceTop(bottomArea, 170) : SliceLeft(bottomArea, ((bottomArea.right - bottomArea.left) - gap) / 2);
    RECT rightBottom = compact ? RECT{bottomArea.left, leftBottom.bottom + gap, bottomArea.right, bottomArea.bottom}
                               : RECT{leftBottom.right + gap, bottomArea.top, bottomArea.right, bottomArea.bottom};

    FillRoundedRect(dc, topCard, 24, state->app->theme.panel, state->app->theme.border);
    FillRoundedRect(dc, leftBottom, 24, state->app->theme.panel, state->app->theme.border);
    FillRoundedRect(dc, rightBottom, 24, state->app->theme.panel, state->app->theme.border);

    RECT topInner = InsetRectCopy(topCard, 24, 24);
    RECT leftInner = InsetRectCopy(leftBottom, 24, 24);
    RECT rightInner = InsetRectCopy(rightBottom, 24, 24);

    DrawLabel(dc, state->app->sectionFont, state->app->theme.text, SliceTop(topInner, 26), L"Tuning Sliders", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->bodyFont, state->app->theme.mutedText, RECT{topInner.left, topInner.top + 32, topInner.right, topInner.top + 50}, L"Exposure", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->bodyFont, state->app->theme.mutedText, RECT{topInner.left, topInner.top + 124, topInner.right, topInner.top + 142}, L"Balance", DT_LEFT | DT_TOP | DT_SINGLELINE);

    DrawLabel(dc, state->app->sectionFont, state->app->theme.text, SliceTop(leftInner, 26), L"Navigation Scrollbars", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->bodyFont, state->app->theme.mutedText, RECT{leftInner.left, leftInner.top + 34, leftInner.right, leftInner.top + 56}, L"Horizontal storyboard scrub and a compact vertical inspector rail.", DT_LEFT | DT_TOP | DT_WORDBREAK);

    DrawLabel(dc, state->app->sectionFont, state->app->theme.text, SliceTop(rightInner, 26), L"Composite Preview", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->bodyFont, state->app->theme.mutedText, RECT{rightInner.left, rightInner.top + 34, rightInner.right, rightInner.top + 56}, L"Preview level updates from the combined sliders and scrollbar position.", DT_LEFT | DT_TOP | DT_WORDBREAK);
}

void PaintDataPage(HWND page, HDC dc, PageState* state) {
    RECT client{};
    GetClientRect(page, &client);
    FillRect(dc, &client, state->app->windowBrush);

    RECT content = InsetRectCopy(client, 28, 28);
    RECT toolbar = SliceTop(content, 42);
    RECT tableArea{content.left, toolbar.bottom + 20, content.right, content.bottom};

    DrawLabel(dc, state->app->sectionFont, state->app->theme.text, RECT{content.left, content.top - 4, content.right, content.top + 22}, L"Content Library", DT_LEFT | DT_TOP | DT_SINGLELINE);
    DrawLabel(dc, state->app->bodyFont, state->app->theme.mutedText, RECT{content.left, content.top + 24, content.right, content.top + 46}, L"Filter presets, action buttons, and a custom dark table with balanced sample data.", DT_LEFT | DT_TOP | DT_WORDBREAK);

    RECT tableCard = tableArea;
    FillRoundedRect(dc, tableCard, 24, state->app->theme.panel, state->app->theme.border);
}

void PaintPage(HWND page, HDC dc, PageState* state) {
    switch (state->kind) {
    case PageKind::Overview:
        PaintOverviewPage(page, dc, state);
        break;
    case PageKind::Controls:
        PaintControlsPage(page, dc, state);
        break;
    case PageKind::Data:
        PaintDataPage(page, dc, state);
        break;
    }
}

bool CreatePageWindow(HWND parent, int controlId, PageKind kind, AppState* app, HWND* outPage) {
    auto* pageState = new PageState();
    pageState->app = app;
    pageState->kind = kind;

    HWND page = CreateWindowExW(0,
                                kPageClassName,
                                L"",
                                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                                0,
                                0,
                                0,
                                0,
                                parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)),
                                pageState);
    if (!page) {
        delete pageState;
        return false;
    }
    *outPage = page;
    return true;
}

bool CreateOverviewControls(AppState* app) {
    if (!app->overview.profile.Create(app->overviewPage, ID_OVERVIEW_PROFILE, app->theme)) return false;
    if (!app->overview.surface.Create(app->overviewPage, ID_OVERVIEW_SURFACE, app->theme)) return false;
    if (!app->overview.primary.Create(app->overviewPage, ID_OVERVIEW_PRIMARY, L"Sync Assets", app->theme)) return false;
    if (!app->overview.secondary.Create(app->overviewPage, ID_OVERVIEW_SECONDARY, L"Share Review", app->theme)) return false;
    if (!app->overview.disabled.Create(app->overviewPage, ID_OVERVIEW_DISABLED, L"Approve Build", app->theme)) return false;
    if (!app->overview.storage.Create(app->overviewPage, ID_OVERVIEW_STORAGE, app->theme)) return false;
    if (!app->overview.index.Create(app->overviewPage, ID_OVERVIEW_INDEX, app->theme)) return false;

    app->overview.primary.SetCornerRadius(14);
    app->overview.secondary.SetCornerRadius(14);
    app->overview.disabled.SetCornerRadius(14);
    app->overview.primary.SetSurfaceColor(app->theme.panel);
    app->overview.secondary.SetSurfaceColor(app->theme.panel);
    app->overview.disabled.SetSurfaceColor(app->theme.panel);
    EnableWindow(app->overview.disabled.hwnd(), FALSE);

    app->overview.profile.SetItems({
        {L"Studio Executive", 1, true},
        {L"Production Grid", 2, false},
        {L"Minimal Review", 3, false}
    });
    app->overview.profile.SetSelection(0);
    app->overview.surface.SetItems({
        {L"Frosted Graphite", 1, true},
        {L"Soft Obsidian", 2, false},
        {L"Teal Nocturne", 3, false}
    });
    app->overview.surface.SetSelection(0);

    app->overview.storage.SetRange(0, 100);
    app->overview.storage.SetSurfaceColor(app->theme.panel);
    app->overview.storage.SetValue(74);
    app->overview.index.SetRange(0, 100);
    app->overview.index.SetSurfaceColor(app->theme.panel);
    app->overview.index.SetValue(61);
    app->overview.index.SetShowPercentage(false);
    return true;
}

bool CreateControlsPanel(AppState* app) {
    if (!app->controls.exposure.Create(app->controlsPage, ID_CONTROLS_EXPOSURE, app->theme)) return false;
    if (!app->controls.balance.Create(app->controlsPage, ID_CONTROLS_BALANCE, app->theme)) return false;
    if (!app->controls.timeline.Create(app->controlsPage, ID_CONTROLS_SCROLL_H, false, app->theme)) return false;
    if (!app->controls.navigator.Create(app->controlsPage, ID_CONTROLS_SCROLL_V, true, app->theme)) return false;
    if (!app->controls.preview.Create(app->controlsPage, ID_CONTROLS_PREVIEW, app->theme)) return false;

    app->controls.exposure.SetRange(0, 100);
    app->controls.exposure.SetValue(68);
    app->controls.exposure.SetShowTicks(true);
    app->controls.exposure.SetTickCount(9);

    app->controls.balance.SetRange(0, 100);
    app->controls.balance.SetValue(42);
    app->controls.balance.SetShowTicks(true);
    app->controls.balance.SetTickCount(7);

    app->controls.timeline.SetRange(0, 100);
    app->controls.timeline.SetPageSize(20);
    app->controls.timeline.SetValue(38);

    app->controls.navigator.SetRange(0, 100);
    app->controls.navigator.SetPageSize(18);
    app->controls.navigator.SetValue(26);

    app->controls.preview.SetRange(0, 100);
    app->controls.preview.SetSurfaceColor(app->theme.panel);
    UpdateControlPreview(app);
    return true;
}

bool CreateDataPanel(AppState* app) {
    if (!app->data.filter.Create(app->dataPage, ID_DATA_FILTER, app->theme)) return false;
    if (!app->data.refresh.Create(app->dataPage, ID_DATA_REFRESH, L"Refresh", app->theme)) return false;
    if (!app->data.archive.Create(app->dataPage, ID_DATA_ARCHIVE, L"Archive", app->theme)) return false;
    if (!app->data.table.Create(app->dataPage, ID_DATA_TABLE, app->theme)) return false;

    app->data.refresh.SetCornerRadius(14);
    app->data.archive.SetCornerRadius(14);
    app->data.refresh.SetSurfaceColor(app->theme.panel);
    app->data.archive.SetSurfaceColor(app->theme.panel);

    app->data.filter.SetItems({
        {L"All assets", 0, true},
        {L"Ready only", 1, false},
        {L"Cloud synced", 2, false},
        {L"Large transfers", 3, false}
    });
    app->data.filter.SetSelection(0);

    app->data.table.SetColumns({
        {L"Collection", 240, LVCFMT_LEFT},
        {L"Source", 110, LVCFMT_LEFT},
        {L"State", 120, LVCFMT_LEFT},
        {L"Size", 120, LVCFMT_RIGHT},
        {L"Updated", 120, LVCFMT_LEFT}
    });
    app->data.table.SetDrawEmptyGrid(true);
    RefreshDataRows(app);
    return true;
}

LRESULT CALLBACK ShowcasePageProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<PageState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* created = reinterpret_cast<PageState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        return TRUE;
    }

    switch (message) {
    case WM_SIZE:
        if (state) {
            LayoutPage(window, state->app, state->kind);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        if (state) {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);
            PaintPage(window, dc, state);
            EndPaint(window, &ps);
            return 0;
        }
        break;
    case WM_HSCROLL:
    case WM_VSCROLL:
        if (state && state->kind == PageKind::Controls) {
            UpdateControlPreview(state->app);
            return 0;
        }
        break;
    case WM_COMMAND:
        if (state) {
            const int id = LOWORD(wParam);
            const int code = HIWORD(wParam);
            if (state->kind == PageKind::Overview && code == BN_CLICKED) {
                if (id == ID_OVERVIEW_PRIMARY) {
                    state->app->overview.storage.SetValue(std::min(100, state->app->overview.storage.GetValue() + 6));
                    return 0;
                }
                if (id == ID_OVERVIEW_SECONDARY) {
                    state->app->overview.index.SetValue(std::min(100, state->app->overview.index.GetValue() + 9));
                    return 0;
                }
            }
            if (state->kind == PageKind::Data) {
                if (id == ID_DATA_FILTER && code == CBN_SELCHANGE) {
                    RefreshDataRows(state->app);
                    return 0;
                }
                if (code == BN_CLICKED && (id == ID_DATA_REFRESH || id == ID_DATA_ARCHIVE)) {
                    RefreshDataRows(state->app);
                    return 0;
                }
            }
        }
        break;
    case WM_NCDESTROY:
        delete state;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK ShowcaseWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* created = new AppState();
        created->theme = MakeThemeByIndex(0);
        RecreateWindowBrush(created);

        darkui::FontSpec titleSpec = created->theme.uiFont;
        titleSpec.height = -34;
        titleSpec.weight = FW_SEMIBOLD;
        created->titleFont = darkui::CreateFont(titleSpec);

        darkui::FontSpec subtitleSpec = created->theme.uiFont;
        subtitleSpec.height = -18;
        created->subtitleFont = darkui::CreateFont(subtitleSpec);

        darkui::FontSpec sectionSpec = created->theme.uiFont;
        sectionSpec.height = -22;
        sectionSpec.weight = FW_SEMIBOLD;
        created->sectionFont = darkui::CreateFont(sectionSpec);
        created->bodyFont = darkui::CreateFont(created->theme.uiFont);
        if (!created->windowBrush || !created->titleFont || !created->subtitleFont || !created->sectionFont || !created->bodyFont) {
            CleanupAppState(created);
            return -1;
        }

        if (!created->themeGraphite.Create(window, ID_THEME_GRAPHITE, L"Graphite", created->theme)) {
            CleanupAppState(created);
            return -1;
        }
        if (!created->themeObsidian.Create(window, ID_THEME_OBSIDIAN, L"Obsidian", created->theme)) {
            CleanupAppState(created);
            return -1;
        }
        if (!created->themeNocturne.Create(window, ID_THEME_NOCTURNE, L"Nocturne", created->theme)) {
            CleanupAppState(created);
            return -1;
        }
        created->themeGraphite.SetCornerRadius(18);
        created->themeObsidian.SetCornerRadius(18);
        created->themeNocturne.SetCornerRadius(18);

        if (!created->tab.Create(window, ID_MAIN_TAB, created->theme)) {
            CleanupAppState(created);
            return -1;
        }
        created->tab.SetVertical(true);
        created->tab.SetItems({
            {L"Overview", 1},
            {L"Controls", 2},
            {L"Data Library", 3}
        });

        if (!CreatePageWindow(created->tab.hwnd(), ID_PAGE_OVERVIEW, PageKind::Overview, created, &created->overviewPage) ||
            !CreatePageWindow(created->tab.hwnd(), ID_PAGE_CONTROLS, PageKind::Controls, created, &created->controlsPage) ||
            !CreatePageWindow(created->tab.hwnd(), ID_PAGE_DATA, PageKind::Data, created, &created->dataPage)) {
            CleanupAppState(created);
            return -1;
        }

        if (!CreateOverviewControls(created) || !CreateControlsPanel(created) || !CreateDataPanel(created)) {
            CleanupAppState(created);
            return -1;
        }

        created->tab.AttachPage(0, created->overviewPage);
        created->tab.AttachPage(1, created->controlsPage);
        created->tab.AttachPage(2, created->dataPage);
        created->tab.SetSelection(0, false);

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
        LayoutMainWindow(window, created);
        return 0;
    }
    case WM_SIZE:
        if (state) {
            LayoutMainWindow(window, state);
        }
        return 0;
    case WM_COMMAND:
        if (state && HIWORD(wParam) == BN_CLICKED) {
            switch (LOWORD(wParam)) {
            case ID_THEME_GRAPHITE:
                ApplyTheme(state, 0);
                return 0;
            case ID_THEME_OBSIDIAN:
                ApplyTheme(state, 1);
                return 0;
            case ID_THEME_NOCTURNE:
                ApplyTheme(state, 2);
                return 0;
            default:
                break;
            }
        }
        break;
    case WM_NOTIFY:
        if (state) {
            auto* hdr = reinterpret_cast<NMHDR*>(lParam);
            if (hdr && hdr->hwndFrom == state->tab.hwnd() && hdr->code == TCN_SELCHANGE) {
                InvalidateRect(window, nullptr, FALSE);
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
            return reinterpret_cast<LRESULT>(state->windowBrush);
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        if (state) {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);
            PaintMainWindow(window, dc, state);
            EndPaint(window, &ps);
            return 0;
        }
        break;
    case WM_DESTROY:
        CleanupAppState(state);
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
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSEXW pageClass{};
    pageClass.cbSize = sizeof(pageClass);
    pageClass.lpfnWndProc = ShowcasePageProc;
    pageClass.hInstance = instance;
    pageClass.lpszClassName = kPageClassName;
    pageClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    pageClass.hbrBackground = nullptr;
    RegisterClassExW(&pageClass);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = ShowcaseWindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kDemoClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    if (!RegisterClassExW(&windowClass)) {
        return 0;
    }

    HWND window = CreateWindowExW(0,
                                  kDemoClassName,
                                  kDemoTitle,
                                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  1360,
                                  900,
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
