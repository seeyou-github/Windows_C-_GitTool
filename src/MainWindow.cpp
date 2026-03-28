#include "MainWindow.h"

#include "DarkTheme.h"
#include "ResourceIds.h"

#include <commctrl.h>
#include <richedit.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

constexpr wchar_t kMainWindowClass[] = L"GitVisualToolMainWindow";
constexpr wchar_t kDarkScrollBarClass[] = L"GitVisualToolDarkScrollBar";
constexpr wchar_t kCommitDetailWindowClass[] = L"GitVisualToolCommitDetailWindow";
constexpr wchar_t kDiffContentWindowClass[] = L"GitVisualToolDiffContentWindow";
constexpr int kToolbarHeight = 44;
constexpr int kPadding = 12;
constexpr int kLeftPanelWidth = 380;
constexpr int kLogHeight = 220;
constexpr int kMinWindowWidth = 1231;
constexpr int kMinWindowHeight = 820;
constexpr int kLogScrollBarWidth = 14;
HFONT g_menuFont = nullptr;
constexpr COLORREF kMenuBackground = RGB(242, 242, 242);
constexpr COLORREF kMenuHoverBackground = RGB(228, 228, 228);
constexpr COLORREF kMenuText = RGB(40, 40, 40);
constexpr COLORREF kLogDefaultText = RGB(170, 176, 184);

std::wstring Trim(const std::wstring& value) {
    const wchar_t* whitespace = L" \t\r\n";
    const size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::wstring::npos) {
        return L"";
    }
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

std::wstring BaseNameFromPath(const std::wstring& path) {
    const size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

COLORREF MapAnsiColor(int code, COLORREF fallback) {
    switch (code) {
    case 30: return RGB(80, 80, 80);
    case 31: return RGB(220, 95, 95);
    case 32: return RGB(120, 200, 120);
    case 33: return RGB(220, 190, 110);
    case 34: return RGB(110, 160, 240);
    case 35: return RGB(200, 130, 220);
    case 36: return RGB(100, 200, 210);
    case 37: return RGB(220, 220, 220);
    case 90: return RGB(120, 120, 120);
    case 91: return RGB(255, 120, 120);
    case 92: return RGB(140, 230, 140);
    case 93: return RGB(240, 210, 120);
    case 94: return RGB(130, 180, 255);
    case 95: return RGB(225, 160, 255);
    case 96: return RGB(130, 220, 230);
    case 97: return RGB(245, 245, 245);
    default: return fallback;
    }
}

void SetListViewColumn(HWND listView, int index, int width, const std::wstring& text) {
    LVCOLUMNW column{};
    column.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    column.cx = width;
    column.pszText = const_cast<LPWSTR>(text.c_str());
    column.iSubItem = index;
    ListView_InsertColumn(listView, index, &column);
}

void FreeProjectListItemData(HWND listView) {
    const int count = ListView_GetItemCount(listView);
    for (int i = 0; i < count; ++i) {
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = i;
        if (ListView_GetItem(listView, &item) && item.lParam != 0) {
            delete reinterpret_cast<std::wstring*>(item.lParam);
        }
    }
}

bool IsOwnerDrawButtonId(UINT id) {
    return id == IDC_BTN_ADD_FOLDER ||
           id == IDC_BTN_STATUS ||
           id == IDC_BTN_ADD_ALL ||
           id == IDC_BTN_COMMIT ||
           id == IDC_BTN_PUSH ||
           id == IDC_BTN_PULL ||
           id == IDC_BTN_FETCH ||
           id == IDC_BTN_BRANCH ||
           id == IDC_BTN_REMOTE;
}

void DrawOwnerDrawButton(const DRAWITEMSTRUCT* dis) {
    RECT rect = dis->rcItem;
    COLORREF background = ((dis->itemState & ODS_SELECTED) != 0)
        ? DarkTheme::AccentBackground()
        : DarkTheme::ControlBackground();

    HBRUSH brush = CreateSolidBrush(background);
    FillRect(dis->hDC, &rect, brush);
    DeleteObject(brush);

    HPEN pen = CreatePen(PS_SOLID, 1, DarkTheme::BorderColor());
    HGDIOBJ oldPen = SelectObject(dis->hDC, pen);
    HGDIOBJ oldBrush = SelectObject(dis->hDC, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dis->hDC, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(dis->hDC, oldBrush);
    SelectObject(dis->hDC, oldPen);
    DeleteObject(pen);

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, DarkTheme::TextColor());

    wchar_t text[256] = {};
    GetWindowTextW(dis->hwndItem, text, 256);
    DrawTextW(dis->hDC, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawDangerMenuItem(const DRAWITEMSTRUCT* dis) {
    RECT rect = dis->rcItem;
    COLORREF background = (dis->itemState & ODS_SELECTED) ? kMenuHoverBackground : kMenuBackground;
    HBRUSH brush = CreateSolidBrush(background);
    FillRect(dis->hDC, &rect, brush);
    DeleteObject(brush);

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, RGB(190, 48, 48));
    HGDIOBJ oldFont = nullptr;
    if (g_menuFont != nullptr) {
        oldFont = SelectObject(dis->hDC, g_menuFont);
    }

    MENUITEMINFOW menuInfo{};
    menuInfo.cbSize = sizeof(menuInfo);
    menuInfo.fMask = MIIM_STRING;
    wchar_t text[128] = {};
    menuInfo.dwTypeData = text;
    menuInfo.cch = static_cast<UINT>(std::size(text));
    GetMenuItemInfoW(reinterpret_cast<HMENU>(dis->hwndItem), dis->itemID, FALSE, &menuInfo);

    rect.left += 18;
    rect.top += 6;
    rect.right -= 18;
    rect.bottom -= 6;
    DrawTextW(dis->hDC, text, -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    if (oldFont != nullptr) {
        SelectObject(dis->hDC, oldFont);
    }
}

void DrawNormalMenuItem(const DRAWITEMSTRUCT* dis) {
    RECT rect = dis->rcItem;
    COLORREF background = (dis->itemState & ODS_SELECTED) ? kMenuHoverBackground : kMenuBackground;
    HBRUSH brush = CreateSolidBrush(background);
    FillRect(dis->hDC, &rect, brush);
    DeleteObject(brush);

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, kMenuText);
    HGDIOBJ oldFont = nullptr;
    if (g_menuFont != nullptr) {
        oldFont = SelectObject(dis->hDC, g_menuFont);
    }

    MENUITEMINFOW menuInfo{};
    menuInfo.cbSize = sizeof(menuInfo);
    menuInfo.fMask = MIIM_STRING;
    wchar_t text[128] = {};
    menuInfo.dwTypeData = text;
    menuInfo.cch = static_cast<UINT>(std::size(text));
    GetMenuItemInfoW(reinterpret_cast<HMENU>(dis->hwndItem), dis->itemID, FALSE, &menuInfo);

    rect.left += 18;
    rect.top += 6;
    rect.right -= 18;
    rect.bottom -= 6;
    DrawTextW(dis->hDC, text, -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    if (oldFont != nullptr) {
        SelectObject(dis->hDC, oldFont);
    }
}

void InsertOwnerDrawMenuItem(HMENU menu, UINT id, const wchar_t* text, UINT position) {
    MENUITEMINFOW item{};
    item.cbSize = sizeof(item);
    item.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STRING;
    item.fType = MFT_OWNERDRAW;
    item.wID = id;
    item.dwTypeData = const_cast<LPWSTR>(text);
    InsertMenuItemW(menu, position, TRUE, &item);
}

int GetVisibleLogLines(HWND edit, HFONT font) {
    RECT rect{};
    GetClientRect(edit, &rect);
    HDC dc = GetDC(edit);
    HGDIOBJ oldFont = SelectObject(dc, font);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, oldFont);
    ReleaseDC(edit, dc);
    const int clientHeight = static_cast<int>(rect.bottom - rect.top);
    const int lineHeight = std::max(1, static_cast<int>(tm.tmHeight));
    return std::max(1, clientHeight / lineHeight);
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), size, nullptr, nullptr);
    return utf8;
}

void AddListViewRowsPadding(HWND listView, int count) {
    if (count <= 0) {
        return;
    }

    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iSubItem = 0;
    item.pszText = const_cast<LPWSTR>(L" ");

    const int startIndex = ListView_GetItemCount(listView);
    for (int i = 0; i < count; ++i) {
        item.iItem = startIndex + i;
        ListView_InsertItem(listView, &item);
    }
    ListView_DeleteItem(listView, startIndex);
}

struct PromptDialogState {
    std::wstring title;
    std::wstring prompt;
    std::wstring text;
    bool multiline = false;
    bool accepted = false;
    WNDPROC editProc = nullptr;
};

struct DiffContentWindowState {
    std::wstring title;
    std::wstring beforeTitle;
    std::wstring afterTitle;
    std::wstring beforeContent;
    std::wstring afterContent;
    HWND beforeLabel = nullptr;
    HWND afterLabel = nullptr;
    HWND toggleButton = nullptr;
    HWND beforeEdit = nullptr;
    HWND afterEdit = nullptr;
    HFONT uiFont = nullptr;
    HFONT codeFont = nullptr;
    WNDPROC beforeEditProc = nullptr;
    WNDPROC afterEditProc = nullptr;
    bool hideSame = false;
    bool syncingScroll = false;
};

struct CommitDetailWindowState {
    std::wstring repoPath;
    std::wstring commitHash;
    std::wstring title;
    std::wstring summary;
    std::vector<CommitFileDiff> diffs;
    HWND summaryEdit = nullptr;
    HWND fileList = nullptr;
    HFONT uiFont = nullptr;
    HFONT codeFont = nullptr;
};

COLORREF GetCommitStatusColor(wchar_t status) {
    switch (status) {
    case L'A':
        return RGB(112, 196, 118);
    case L'D':
        return RGB(136, 142, 153);
    case L'R':
        return RGB(234, 176, 76);
    case L'M':
    default:
        return RGB(224, 108, 117);
    }
}

std::wstring BuildCommitFileLabel(const CommitFileDiff& diff) {
    std::wstring label;
    label += diff.status;
    label += L"  ";
    label += diff.path;
    return label;
}

void ConfigureRichEditColors(HWND edit, HFONT font, COLORREF background, COLORREF textColor) {
    if (edit == nullptr) {
        return;
    }

    SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(edit, EM_SETBKGNDCOLOR, 0, background);

    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR;
    format.crTextColor = textColor;
    SendMessageW(edit, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
    SendMessageW(edit, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
}

struct DiffDisplayLine {
    int beforeLineNumber = 0;
    std::wstring beforeText;
    COLORREF beforeTextColor = DarkTheme::TextColor();
    COLORREF beforeBackground = DarkTheme::ControlBackground();
    int afterLineNumber = 0;
    std::wstring afterText;
    COLORREF afterTextColor = DarkTheme::TextColor();
    COLORREF afterBackground = DarkTheme::ControlBackground();
    bool same = false;
    bool placeholder = false;
};

std::vector<std::wstring> SplitTextLines(const std::wstring& text) {
    std::vector<std::wstring> lines;
    std::wstringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    if (lines.empty() && !text.empty()) {
        lines.push_back(text);
    }
    return lines;
}

std::vector<std::pair<int, int>> ComputeLineMatches(
    const std::vector<std::wstring>& beforeLines,
    const std::vector<std::wstring>& afterLines) {
    const size_t beforeCount = beforeLines.size();
    const size_t afterCount = afterLines.size();
    if (beforeCount == 0 || afterCount == 0 || (beforeCount * afterCount) > 400000) {
        return {};
    }

    std::vector<int> table((beforeCount + 1) * (afterCount + 1), 0);
    auto at = [&](size_t i, size_t j) -> int& {
        return table[i * (afterCount + 1) + j];
    };

    for (int i = static_cast<int>(beforeCount) - 1; i >= 0; --i) {
        for (int j = static_cast<int>(afterCount) - 1; j >= 0; --j) {
            if (beforeLines[static_cast<size_t>(i)] == afterLines[static_cast<size_t>(j)]) {
                at(static_cast<size_t>(i), static_cast<size_t>(j)) = 1 + at(static_cast<size_t>(i + 1), static_cast<size_t>(j + 1));
            } else {
                at(static_cast<size_t>(i), static_cast<size_t>(j)) = std::max(
                    at(static_cast<size_t>(i + 1), static_cast<size_t>(j)),
                    at(static_cast<size_t>(i), static_cast<size_t>(j + 1)));
            }
        }
    }

    std::vector<std::pair<int, int>> matches;
    size_t i = 0;
    size_t j = 0;
    while (i < beforeCount && j < afterCount) {
        if (beforeLines[i] == afterLines[j]) {
            matches.emplace_back(static_cast<int>(i), static_cast<int>(j));
            ++i;
            ++j;
        } else if (at(i + 1, j) >= at(i, j + 1)) {
            ++i;
        } else {
            ++j;
        }
    }
    return matches;
}

void AppendDiffChangedRows(
    std::vector<DiffDisplayLine>& rows,
    const std::vector<std::wstring>& beforeLines,
    const std::vector<std::wstring>& afterLines,
    int beforeStart,
    int beforeEnd,
    int afterStart,
    int afterEnd) {
    const int span = std::max(beforeEnd - beforeStart, afterEnd - afterStart);
    for (int i = 0; i < span; ++i) {
        DiffDisplayLine row;
        const bool hasBefore = (beforeStart + i) < beforeEnd;
        const bool hasAfter = (afterStart + i) < afterEnd;

        if (hasBefore) {
            row.beforeLineNumber = beforeStart + i + 1;
            row.beforeText = beforeLines[static_cast<size_t>(beforeStart + i)];
        }
        if (hasAfter) {
            row.afterLineNumber = afterStart + i + 1;
            row.afterText = afterLines[static_cast<size_t>(afterStart + i)];
        }

        if (hasBefore && hasAfter) {
            row.beforeBackground = RGB(73, 44, 46);
            row.afterBackground = RGB(38, 73, 47);
            row.beforeTextColor = RGB(240, 210, 210);
            row.afterTextColor = RGB(214, 239, 214);
        } else if (hasBefore) {
            row.beforeBackground = RGB(73, 44, 46);
            row.beforeTextColor = RGB(240, 210, 210);
            row.afterBackground = DarkTheme::PanelBackground();
            row.afterTextColor = RGB(120, 120, 120);
        } else if (hasAfter) {
            row.beforeBackground = DarkTheme::PanelBackground();
            row.beforeTextColor = RGB(120, 120, 120);
            row.afterBackground = RGB(38, 73, 47);
            row.afterTextColor = RGB(214, 239, 214);
        }
        rows.push_back(row);
    }
}

std::vector<DiffDisplayLine> BuildDiffDisplayLines(
    const std::wstring& beforeContent,
    const std::wstring& afterContent,
    bool hideSame) {
    const std::vector<std::wstring> beforeLines = SplitTextLines(beforeContent);
    const std::vector<std::wstring> afterLines = SplitTextLines(afterContent);
    const std::vector<std::pair<int, int>> matches = ComputeLineMatches(beforeLines, afterLines);

    std::vector<DiffDisplayLine> rows;
    int beforeIndex = 0;
    int afterIndex = 0;
    for (const auto& match : matches) {
        AppendDiffChangedRows(rows, beforeLines, afterLines, beforeIndex, match.first, afterIndex, match.second);

        DiffDisplayLine sameRow;
        sameRow.same = true;
        sameRow.beforeLineNumber = match.first + 1;
        sameRow.afterLineNumber = match.second + 1;
        sameRow.beforeText = beforeLines[static_cast<size_t>(match.first)];
        sameRow.afterText = afterLines[static_cast<size_t>(match.second)];
        sameRow.beforeBackground = DarkTheme::ControlBackground();
        sameRow.afterBackground = DarkTheme::ControlBackground();
        sameRow.beforeTextColor = RGB(190, 196, 204);
        sameRow.afterTextColor = RGB(190, 196, 204);
        rows.push_back(sameRow);

        beforeIndex = match.first + 1;
        afterIndex = match.second + 1;
    }
    AppendDiffChangedRows(
        rows, beforeLines, afterLines,
        beforeIndex, static_cast<int>(beforeLines.size()),
        afterIndex, static_cast<int>(afterLines.size()));

    if (!hideSame) {
        return rows;
    }

    std::vector<DiffDisplayLine> filtered;
    size_t index = 0;
    while (index < rows.size()) {
        if (!rows[index].same) {
            filtered.push_back(rows[index]);
            ++index;
            continue;
        }

        size_t end = index;
        while (end < rows.size() && rows[end].same) {
            ++end;
        }

        DiffDisplayLine placeholder;
        placeholder.placeholder = true;
        placeholder.same = true;
        placeholder.beforeText = L"... same lines hidden ...";
        placeholder.afterText = L"... same lines hidden ...";
        placeholder.beforeTextColor = RGB(138, 145, 156);
        placeholder.afterTextColor = RGB(138, 145, 156);
        placeholder.beforeBackground = DarkTheme::PanelBackground();
        placeholder.afterBackground = DarkTheme::PanelBackground();
        filtered.push_back(placeholder);
        index = end;
    }
    return filtered;
}

std::wstring FormatDiffDisplayLine(int lineNumber, const std::wstring& text, bool placeholder) {
    if (placeholder) {
        return L"      | " + text;
    }

    wchar_t buffer[32] = {};
    if (lineNumber > 0) {
        swprintf_s(buffer, L"%5d | ", lineNumber);
    } else {
        wcscpy_s(buffer, L"      | ");
    }
    return std::wstring(buffer) + text;
}

void AppendRichEditLine(HWND edit, const std::wstring& text, COLORREF textColor, COLORREF backgroundColor) {
    CHARRANGE range{-1, -1};
    SendMessageW(edit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));

    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR | CFM_BACKCOLOR;
    format.crTextColor = textColor;
    format.crBackColor = backgroundColor;
    SendMessageW(edit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    SendMessageW(edit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
}

void RenderDiffEditors(DiffContentWindowState* state) {
    if (state == nullptr || state->beforeEdit == nullptr || state->afterEdit == nullptr) {
        return;
    }

    const std::vector<DiffDisplayLine> rows = BuildDiffDisplayLines(
        state->beforeContent, state->afterContent, state->hideSame);

    SetWindowTextW(state->beforeEdit, L"");
    SetWindowTextW(state->afterEdit, L"");
    for (const auto& row : rows) {
        AppendRichEditLine(
            state->beforeEdit,
            FormatDiffDisplayLine(row.beforeLineNumber, row.beforeText, row.placeholder) + L"\r\n",
            row.beforeTextColor,
            row.beforeBackground);
        AppendRichEditLine(
            state->afterEdit,
            FormatDiffDisplayLine(row.afterLineNumber, row.afterText, row.placeholder) + L"\r\n",
            row.afterTextColor,
            row.afterBackground);
    }

    SendMessageW(state->beforeEdit, WM_VSCROLL, MAKEWPARAM(SB_TOP, 0), 0);
    SendMessageW(state->afterEdit, WM_VSCROLL, MAKEWPARAM(SB_TOP, 0), 0);
}

void SyncDiffEditors(DiffContentWindowState* state, HWND sourceEdit) {
    if (state == nullptr || state->syncingScroll || state->beforeEdit == nullptr || state->afterEdit == nullptr) {
        return;
    }

    HWND targetEdit = sourceEdit == state->beforeEdit ? state->afterEdit : state->beforeEdit;
    const int sourceLine = static_cast<int>(SendMessageW(sourceEdit, EM_GETFIRSTVISIBLELINE, 0, 0));
    const int targetLine = static_cast<int>(SendMessageW(targetEdit, EM_GETFIRSTVISIBLELINE, 0, 0));
    const int delta = sourceLine - targetLine;
    if (delta == 0) {
        return;
    }

    state->syncingScroll = true;
    SendMessageW(targetEdit, EM_LINESCROLL, 0, delta);
    state->syncingScroll = false;
}

LRESULT CALLBACK DiffEditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    HWND parent = GetParent(hwnd);
    auto* state = reinterpret_cast<DiffContentWindowState*>(GetWindowLongPtrW(parent, GWLP_USERDATA));
    WNDPROC originalProc = nullptr;
    if (state != nullptr) {
        originalProc = hwnd == state->beforeEdit ? state->beforeEditProc : state->afterEditProc;
    }
    if (originalProc == nullptr) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    const LRESULT result = CallWindowProcW(originalProc, hwnd, message, wParam, lParam);
    if (state != nullptr) {
        switch (message) {
        case WM_MOUSEWHEEL:
        case WM_VSCROLL:
        case WM_KEYDOWN:
            SyncDiffEditors(state, hwnd);
            break;
        default:
            break;
        }
    }
    return result;
}

LRESULT CALLBACK PromptEditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    HWND dialog = GetParent(hwnd);
    auto* state = reinterpret_cast<PromptDialogState*>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
    if (state == nullptr || state->editProc == nullptr) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    if (message == WM_GETDLGCODE) {
        return CallWindowProcW(state->editProc, hwnd, message, wParam, lParam) | DLGC_WANTALLKEYS;
    }

    return CallWindowProcW(state->editProc, hwnd, message, wParam, lParam);
}

std::wstring FormatCommitSummary(const std::wstring& rawDetails) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    while (start <= rawDetails.size()) {
        const size_t pos = rawDetails.find(L'\x001f', start);
        if (pos == std::wstring::npos) {
            parts.push_back(rawDetails.substr(start));
            break;
        }
        parts.push_back(rawDetails.substr(start, pos - start));
        start = pos + 1;
    }

    const std::wstring hash = parts.size() > 0 ? Trim(parts[0]) : L"";
    const std::wstring commitBy = parts.size() > 1 ? Trim(parts[1]) : L"";
    const std::wstring commitDate = parts.size() > 2 ? Trim(parts[2]) : L"";
    std::wstring message = parts.size() > 3 ? parts[3] : L"";
    message = Trim(message);
    std::wstringstream messageStream(message);
    std::wstring formattedMessage;
    std::wstring line;
    bool firstLine = true;
    while (std::getline(messageStream, line)) {
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (!firstLine) {
            formattedMessage += L"\r\n";
        }
        formattedMessage += L"\t" + line;
        firstLine = false;
    }
    if (formattedMessage.empty()) {
        formattedMessage = L"\t";
    }

    std::wstringstream stream;
    stream << L"commit id: " << hash << L"\r\n"
           << L"Commit:     " << commitBy << L"\r\n"
           << L"CommitDate: " << commitDate << L"\r\n"
           << L"message:\r\n\r\n"
           << formattedMessage;
    return stream.str();
}

LRESULT CALLBACK DiffContentWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DiffContentWindowState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = reinterpret_cast<DiffContentWindowState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        DarkTheme::ApplyTitleBar(hwnd);

        state->beforeLabel = CreateWindowExW(
            0, L"STATIC", state->beforeTitle.c_str(),
            WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, hwnd, nullptr, cs->hInstance, nullptr);
        state->afterLabel = CreateWindowExW(
            0, L"STATIC", state->afterTitle.c_str(),
            WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, hwnd, nullptr, cs->hInstance, nullptr);
        state->toggleButton = CreateWindowExW(
            0, L"BUTTON", L"Hide Same",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(4201), cs->hInstance, nullptr);
        state->beforeEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, state->beforeContent.c_str(),
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY |
                ES_NOHIDESEL | WS_VSCROLL | WS_HSCROLL,
            0, 0, 0, 0, hwnd, nullptr, cs->hInstance, nullptr);
        state->afterEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, state->afterContent.c_str(),
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY |
                ES_NOHIDESEL | WS_VSCROLL | WS_HSCROLL,
            0, 0, 0, 0, hwnd, nullptr, cs->hInstance, nullptr);

        ConfigureRichEditColors(
            state->beforeEdit,
            state->codeFont,
            DarkTheme::ControlBackground(),
            DarkTheme::TextColor());
        ConfigureRichEditColors(
            state->afterEdit,
            state->codeFont,
            DarkTheme::ControlBackground(),
            DarkTheme::TextColor());
        SendMessageW(state->beforeLabel, WM_SETFONT, reinterpret_cast<WPARAM>(state->uiFont), TRUE);
        SendMessageW(state->afterLabel, WM_SETFONT, reinterpret_cast<WPARAM>(state->uiFont), TRUE);
        SendMessageW(state->toggleButton, WM_SETFONT, reinterpret_cast<WPARAM>(state->uiFont), TRUE);
        DarkTheme::ApplyDarkControlTheme(state->beforeEdit);
        DarkTheme::ApplyDarkControlTheme(state->afterEdit);
        DarkTheme::DisableVisualTheme(state->toggleButton);
        state->beforeEditProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(state->beforeEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(DiffEditProc)));
        state->afterEditProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(state->afterEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(DiffEditProc)));
        RenderDiffEditors(state);
        return 0;
    }
    case WM_SIZE:
        if (state != nullptr && state->beforeEdit != nullptr && state->afterEdit != nullptr) {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            const int padding = 12;
            const int labelHeight = 24;
            const int columnGap = 12;
            const int buttonWidth = 120;
            const int contentTop = padding + labelHeight + 8 + 28 + 8;
            const int contentHeight = rc.bottom - contentTop - padding;
            const int columnWidth = (rc.right - padding * 2 - columnGap) / 2;
            MoveWindow(state->beforeLabel, padding, padding, columnWidth, labelHeight, TRUE);
            MoveWindow(state->afterLabel, padding + columnWidth + columnGap, padding, columnWidth, labelHeight, TRUE);
            MoveWindow(state->toggleButton, rc.right - padding - buttonWidth, padding + labelHeight + 4, buttonWidth, 24, TRUE);
            MoveWindow(state->beforeEdit, padding, contentTop, columnWidth, contentHeight, TRUE);
            MoveWindow(state->afterEdit, padding + columnWidth + columnGap, contentTop, columnWidth, contentHeight, TRUE);
        }
        return 0;
    case WM_COMMAND:
        if (state != nullptr && LOWORD(wParam) == 4201) {
            state->hideSame = (SendMessageW(state->toggleButton, BM_GETCHECK, 0, 0) == BST_CHECKED);
            RenderDiffEditors(state);
            return 0;
        }
        break;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        if (state != nullptr &&
            (reinterpret_cast<HWND>(lParam) == state->beforeLabel ||
             reinterpret_cast<HWND>(lParam) == state->afterLabel ||
             reinterpret_cast<HWND>(lParam) == state->toggleButton)) {
            SetTextColor(dc, DarkTheme::TextColor());
            SetBkColor(dc, DarkTheme::WindowBackground());
            return reinterpret_cast<LRESULT>(DarkTheme::WindowBrush());
        }
        SetTextColor(dc, DarkTheme::TextColor());
        SetBkColor(dc, DarkTheme::ControlBackground());
        return reinterpret_cast<LRESULT>(DarkTheme::ControlBrush());
    }
    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, DarkTheme::WindowBrush());
        return 1;
    }
    case WM_DESTROY:
        delete state;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK CommitDetailWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<CommitDetailWindowState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = reinterpret_cast<CommitDetailWindowState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        DarkTheme::ApplyTitleBar(hwnd);

        state->summaryEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, state->summary.c_str(),
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY |
                ES_NOHIDESEL | WS_VSCROLL,
            0, 0, 0, 0, hwnd, nullptr, cs->hInstance, nullptr);
        state->fileList = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_OWNERDRAWFIXED | WS_VSCROLL,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(4101), cs->hInstance, nullptr);

        ConfigureRichEditColors(
            state->summaryEdit,
            state->uiFont,
            DarkTheme::ControlBackground(),
            DarkTheme::TextColor());
        SendMessageW(state->fileList, WM_SETFONT, reinterpret_cast<WPARAM>(state->uiFont), TRUE);
        DarkTheme::ApplyDarkControlTheme(state->summaryEdit);
        DarkTheme::DisableVisualTheme(state->fileList);

        for (size_t i = 0; i < state->diffs.size(); ++i) {
            const int itemIndex = static_cast<int>(SendMessageW(
                state->fileList, LB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(BuildCommitFileLabel(state->diffs[i]).c_str())));
            SendMessageW(state->fileList, LB_SETITEMDATA, itemIndex, static_cast<LPARAM>(i));
        }
        return 0;
    }
    case WM_SIZE:
        if (state != nullptr) {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            const int padding = 12;
            const int topHeight = (rc.bottom - rc.top) * 38 / 100;
            const int bottomTop = padding + topHeight + padding;
            const int bottomHeight = rc.bottom - bottomTop - padding;
            MoveWindow(state->summaryEdit, padding, padding, rc.right - padding * 2, topHeight, TRUE);
            MoveWindow(state->fileList, padding, bottomTop, rc.right - padding * 2, bottomHeight, TRUE);
        }
        return 0;
    case WM_COMMAND:
        if (state != nullptr && reinterpret_cast<HWND>(lParam) == state->fileList) {
            if (HIWORD(wParam) == LBN_DBLCLK) {
                const int index = static_cast<int>(SendMessageW(state->fileList, LB_GETCURSEL, 0, 0));
                if (index >= 0 && index < static_cast<int>(state->diffs.size())) {
                    auto* diffState = new DiffContentWindowState();
                    diffState->title = L"Diff - " + BuildCommitFileLabel(state->diffs[index]);
                    const CommitFileDiff& diff = state->diffs[index];
                    const std::wstring parentRevision = state->commitHash + L"^";
                    std::wstring beforeContent;
                    std::wstring afterContent;

                    if (diff.status != L'A') {
                        const std::wstring beforePath = diff.status == L'R' ? diff.oldPath : diff.patchPath;
                        beforeContent = GitRunner::GetFileContentAtRevision(
                            state->repoPath, parentRevision, beforePath);
                    }
                    if (diff.status != L'D') {
                        const std::wstring afterPath = diff.status == L'R' ? diff.newPath : diff.patchPath;
                        afterContent = GitRunner::GetFileContentAtRevision(
                            state->repoPath, state->commitHash, afterPath);
                    }

                    diffState->beforeTitle = L"Before";
                    diffState->afterTitle = L"After";
                    diffState->beforeContent = beforeContent.empty()
                        ? L"(empty)"
                        : beforeContent;
                    diffState->afterContent = afterContent.empty()
                        ? L"(empty)"
                        : afterContent;
                    diffState->uiFont = state->uiFont;
                    diffState->codeFont = state->codeFont != nullptr ? state->codeFont : state->uiFont;

                    RECT rc{};
                    GetWindowRect(hwnd, &rc);
                    const int width = ((rc.right - rc.left) * 9) / 10;
                    const int height = ((rc.bottom - rc.top) * 9) / 10;
                    const int x = rc.left + ((rc.right - rc.left) - width) / 2;
                    const int y = rc.top + ((rc.bottom - rc.top) - height) / 2;
                    HWND diffWindow = CreateWindowExW(
                        0, kDiffContentWindowClass, diffState->title.c_str(),
                        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_MAXIMIZEBOX | WS_THICKFRAME,
                        x, y, width, height, hwnd, nullptr, GetModuleHandleW(nullptr), diffState);
                    if (diffWindow == nullptr) {
                        delete diffState;
                    }
                }
                return 0;
            }
            return 0;
        }
        break;
    case WM_DRAWITEM: {
        const auto* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (state != nullptr && dis != nullptr && dis->hwndItem == state->fileList) {
            RECT rect = dis->rcItem;
            const bool selected = (dis->itemState & ODS_SELECTED) != 0;
            HBRUSH brush = CreateSolidBrush(selected ? DarkTheme::AccentBackground() : DarkTheme::ControlBackground());
            FillRect(dis->hDC, &rect, brush);
            DeleteObject(brush);

            if (dis->itemID != static_cast<UINT>(-1) && dis->itemID < state->diffs.size()) {
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, GetCommitStatusColor(state->diffs[dis->itemID].status));
                HGDIOBJ oldFont = SelectObject(dis->hDC, state->uiFont);
                rect.left += 16;
                DrawTextW(
                    dis->hDC,
                    BuildCommitFileLabel(state->diffs[dis->itemID]).c_str(),
                    -1,
                    &rect,
                    DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
                SelectObject(dis->hDC, oldFont);
            }
            return TRUE;
        }
        break;
    }
    case WM_MEASUREITEM: {
        auto* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (state != nullptr && mis != nullptr && mis->CtlType == ODT_LISTBOX) {
            mis->itemHeight = 34;
            return TRUE;
        }
        break;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, DarkTheme::TextColor());
        SetBkColor(dc, DarkTheme::ControlBackground());
        return reinterpret_cast<LRESULT>(DarkTheme::ControlBrush());
    }
    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, DarkTheme::WindowBrush());
        return 1;
    }
    case WM_DESTROY:
        delete state;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

INT_PTR CALLBACK PromptDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<PromptDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
    case WM_INITDIALOG: {
        state = reinterpret_cast<PromptDialogState*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        SetWindowTextW(hwnd, state->title.c_str());
        SetDlgItemTextW(hwnd, 1001, state->prompt.c_str());
        SetDlgItemTextW(hwnd, 1002, state->text.c_str());
        SendDlgItemMessageW(hwnd, 1002, EM_SETLIMITTEXT, 8192, 0);
        HWND parent = GetParent(hwnd);
        if (parent != nullptr) {
            RECT parentRect{};
            RECT dialogRect{};
            GetWindowRect(parent, &parentRect);
            GetWindowRect(hwnd, &dialogRect);
            const int dialogWidth = dialogRect.right - dialogRect.left;
            const int dialogHeight = dialogRect.bottom - dialogRect.top;
            const int x = parentRect.left + ((parentRect.right - parentRect.left) - dialogWidth) / 2;
            const int y = parentRect.top + ((parentRect.bottom - parentRect.top) - dialogHeight) / 2;
            SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
        }
        if (state->multiline) {
            HWND edit = GetDlgItem(hwnd, 1002);
            state->editProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(edit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PromptEditProc)));
            SendMessageW(hwnd, DM_SETDEFID, 0, 0);
            SetFocus(edit);
            return FALSE;
        }
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK && state != nullptr) {
            wchar_t buffer[1024] = {};
            GetDlgItemTextW(hwnd, 1002, buffer, 1024);
            state->text = buffer;
            state->accepted = true;
            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

std::vector<BYTE> BuildPromptDialogTemplate(bool multiline) {
    std::vector<BYTE> bytes(2048, 0);
    auto* dlg = reinterpret_cast<DLGTEMPLATE*>(bytes.data());
    dlg->style = WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION | DS_SETFONT;
    dlg->cdit = 4;
    dlg->x = 10;
    dlg->y = 10;
    dlg->cx = multiline ? 260 : 220;
    dlg->cy = multiline ? 130 : 80;

    WORD* cursor = reinterpret_cast<WORD*>(dlg + 1);
    *cursor++ = 0;
    *cursor++ = 0;
    *cursor++ = 0;
    *cursor++ = 9;
    const wchar_t fontName[] = L"Segoe UI";
    for (wchar_t ch : fontName) {
        *cursor++ = ch;
    }

    auto alignToDword = [&cursor]() {
        ULONG_PTR value = reinterpret_cast<ULONG_PTR>(cursor);
        value = (value + 3) & ~static_cast<ULONG_PTR>(3);
        cursor = reinterpret_cast<WORD*>(value);
    };

    auto addControl = [&cursor, &alignToDword](
                          DWORD style, short x, short y, short cx, short cy,
                          WORD id, WORD classAtom, const wchar_t* text) {
        alignToDword();
        auto* item = reinterpret_cast<DLGITEMTEMPLATE*>(cursor);
        item->style = style;
        item->x = x;
        item->y = y;
        item->cx = cx;
        item->cy = cy;
        item->id = id;
        cursor = reinterpret_cast<WORD*>(item + 1);
        *cursor++ = 0xFFFF;
        *cursor++ = classAtom;
        while (*text != L'\0') {
            *cursor++ = *text++;
        }
        *cursor++ = 0;
        *cursor++ = 0;
    };

    if (multiline) {
        addControl(WS_CHILD | WS_VISIBLE, 8, 8, 236, 10, 1001, 0x0082, L"Prompt");
        addControl(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_TABSTOP,
                   8, 24, 236, 62, 1002, 0x0081, L"");
        addControl(WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 108, 96, 60, 14, IDOK, 0x0080, L"OK");
        addControl(WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 184, 96, 60, 14, IDCANCEL, 0x0080, L"Cancel");
    } else {
        addControl(WS_CHILD | WS_VISIBLE, 8, 8, 200, 10, 1001, 0x0082, L"Prompt");
        addControl(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 8, 24, 200, 14, 1002, 0x0081, L"");
        addControl(WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 72, 48, 60, 14, IDOK, 0x0080, L"OK");
        addControl(WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 148, 48, 60, 14, IDCANCEL, 0x0080, L"Cancel");
    }

    bytes.resize(reinterpret_cast<BYTE*>(cursor) - bytes.data());
    return bytes;
}

}  // namespace

MainWindow::MainWindow() = default;

bool MainWindow::Create(HINSTANCE instance, int nCmdShow) {
    instance_ = instance;
    projectStore_.Load();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWindow::WindowProc;
    wc.hInstance = instance_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = DarkTheme::WindowBrush();
    wc.lpszClassName = kMainWindowClass;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    WNDCLASSEXW scrollClass{};
    scrollClass.cbSize = sizeof(scrollClass);
    scrollClass.lpfnWndProc = MainWindow::LogScrollBarProc;
    scrollClass.hInstance = instance_;
    scrollClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    scrollClass.hbrBackground = DarkTheme::PanelBrush();
    scrollClass.lpszClassName = kDarkScrollBarClass;
    RegisterClassExW(&scrollClass);

    WNDCLASSEXW detailClass{};
    detailClass.cbSize = sizeof(detailClass);
    detailClass.lpfnWndProc = CommitDetailWindowProc;
    detailClass.hInstance = instance_;
    detailClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    detailClass.hbrBackground = DarkTheme::WindowBrush();
    detailClass.lpszClassName = kCommitDetailWindowClass;
    RegisterClassExW(&detailClass);

    WNDCLASSEXW diffClass{};
    diffClass.cbSize = sizeof(diffClass);
    diffClass.lpfnWndProc = DiffContentWindowProc;
    diffClass.hInstance = instance_;
    diffClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    diffClass.hbrBackground = DarkTheme::WindowBrush();
    diffClass.lpszClassName = kDiffContentWindowClass;
    RegisterClassExW(&diffClass);

    const int windowWidth = std::max(projectStore_.GetWindowWidth(), kMinWindowWidth);
    const int windowHeight = std::max(projectStore_.GetWindowHeight(), kMinWindowHeight);
    const RECT centeredRect = CalculateCenteredWindowRect(windowWidth, windowHeight);

    hwnd_ = CreateWindowExW(
        0,
        kMainWindowClass,
        LoadStringResource(IDS_APP_TITLE).c_str(),
        WS_OVERLAPPEDWINDOW,
        centeredRect.left,
        centeredRect.top,
        centeredRect.right - centeredRect.left,
        centeredRect.bottom - centeredRect.top,
        nullptr,
        nullptr,
        instance_,
        this);

    if (hwnd_ == nullptr) {
        return false;
    }

    PrepareInitialShow();
    FinalizeInitialShow(nCmdShow);
    return true;
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    return self != nullptr ? self->HandleMessage(message, wParam, lParam)
                           : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK MainWindow::LogEditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(GetParent(hwnd), GWLP_USERDATA));
    if (self != nullptr && self->defaultLogEditProc_ != nullptr) {
        if (message == WM_MOUSEWHEEL) {
            const LRESULT result = CallWindowProcW(self->defaultLogEditProc_, hwnd, message, wParam, lParam);
            self->UpdateLogScrollBar();
            return result;
        }
        const LRESULT result = CallWindowProcW(self->defaultLogEditProc_, hwnd, message, wParam, lParam);
        switch (message) {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_LBUTTONUP:
        case WM_SIZE:
            self->UpdateLogScrollBar();
            break;
        default:
            break;
        }
        return result;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK MainWindow::LogScrollBarProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(GetParent(hwnd), GWLP_USERDATA));
    if (self == nullptr) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    auto calcThumbRect = [self, hwnd]() {
        RECT client{};
        GetClientRect(hwnd, &client);

        RECT thumb = client;
        const int lineCount = std::max(1, static_cast<int>(SendMessageW(self->logEdit_, EM_GETLINECOUNT, 0, 0)));
        const int visibleLines = GetVisibleLogLines(self->logEdit_, self->logFont_ != nullptr ? self->logFont_ : self->uiFont_);
        const int maxPos = std::max(0, lineCount - visibleLines);
        const int trackHeight = std::max(1, static_cast<int>(client.bottom - client.top));
        const int thumbHeight = std::max(28, (visibleLines * trackHeight) / std::max(visibleLines, lineCount));
        const int firstLine = static_cast<int>(SendMessageW(self->logEdit_, EM_GETFIRSTVISIBLELINE, 0, 0));
        const int travel = std::max(0, trackHeight - thumbHeight);
        const int thumbTop = (maxPos > 0) ? ((firstLine * travel) / maxPos) : 0;

        thumb.top = thumbTop;
        thumb.bottom = thumbTop + thumbHeight;
        return thumb;
    };

    switch (message) {
    case WM_LBUTTONDOWN: {
        SetFocus(self->logEdit_);
        SetCapture(hwnd);
        self->logScrollDragging_ = true;

        RECT thumb = calcThumbRect();
        const int mouseY = static_cast<int>(GET_Y_LPARAM(lParam));
        if (mouseY >= thumb.top && mouseY <= thumb.bottom) {
            self->logScrollDragOffsetY_ = mouseY - thumb.top;
        } else {
            self->logScrollDragOffsetY_ = thumb.bottom > thumb.top ? (thumb.bottom - thumb.top) / 2 : 0;
        }
        SendMessageW(hwnd, WM_MOUSEMOVE, 0, lParam);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (self->logScrollDragging_) {
            RECT client{};
            GetClientRect(hwnd, &client);
            RECT thumb = calcThumbRect();
            const int thumbHeight = std::max(1, static_cast<int>(thumb.bottom - thumb.top));
            const int trackHeight = std::max(1, static_cast<int>(client.bottom - client.top));
            const int travel = std::max(1, trackHeight - thumbHeight);
            const int mouseY = static_cast<int>(GET_Y_LPARAM(lParam));
            const int thumbTop = std::clamp(mouseY - self->logScrollDragOffsetY_, 0, travel);

            const int lineCount = std::max(1, static_cast<int>(SendMessageW(self->logEdit_, EM_GETLINECOUNT, 0, 0)));
            const int visibleLines = GetVisibleLogLines(self->logEdit_, self->logFont_ != nullptr ? self->logFont_ : self->uiFont_);
            const int maxPos = std::max(0, lineCount - visibleLines);
            const int target = (travel > 0) ? ((thumbTop * maxPos) / travel) : 0;
            self->ScrollLogToPosition(target);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (self->logScrollDragging_) {
            self->logScrollDragging_ = false;
            ReleaseCapture();
            return 0;
        }
        break;
    case WM_CAPTURECHANGED:
        self->logScrollDragging_ = false;
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);

        HBRUSH trackBrush = CreateSolidBrush(RGB(33, 36, 42));
        FillRect(dc, &client, trackBrush);
        DeleteObject(trackBrush);

        RECT thumb = calcThumbRect();
        HBRUSH thumbBrush = CreateSolidBrush(self->logScrollDragging_ ? RGB(100, 108, 122) : RGB(82, 90, 102));
        FillRect(dc, &thumb, thumbBrush);
        DeleteObject(thumbBrush);
        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        DarkTheme::ApplyTitleBar(hwnd_);
        CreateControls();
        ApplyFonts();
        {
            RECT rect{};
            GetClientRect(hwnd_, &rect);
            LayoutControls(rect.right - rect.left, rect.bottom - rect.top);
        }
        LoadProjectsIntoList();
        AppendLog(LoadStringResource(IDS_LOG_READY));
        RefreshCurrentRepository(false);
        ShowControls();
        return 0;
    case WM_SIZE:
        LayoutControls(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = kMinWindowWidth;
        info->ptMinTrackSize.y = kMinWindowHeight;
        return 0;
    }
    case WM_DRAWITEM: {
        const auto* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (dis != nullptr && IsOwnerDrawButtonId(dis->CtlID)) {
            DrawOwnerDrawButton(dis);
            return TRUE;
        }
        if (dis != nullptr && dis->CtlType == ODT_MENU && dis->itemID == IDM_COMMIT_RESET_HARD) {
            DrawDangerMenuItem(dis);
            return TRUE;
        }
        if (dis != nullptr && dis->CtlType == ODT_MENU && dis->itemID == IDM_COMMIT_RESET_SOFT) {
            DrawNormalMenuItem(dis);
            return TRUE;
        }
        if (dis != nullptr && dis->CtlType == ODT_MENU && dis->itemID == IDM_COMMIT_EDIT_MESSAGE) {
            DrawNormalMenuItem(dis);
            return TRUE;
        }
        break;
    }
    case WM_MEASUREITEM: {
        auto* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (mis != nullptr && mis->CtlType == ODT_MENU &&
            (mis->itemID == IDM_COMMIT_RESET_HARD ||
             mis->itemID == IDM_COMMIT_RESET_SOFT ||
             mis->itemID == IDM_COMMIT_EDIT_MESSAGE)) {
            mis->itemWidth = 320;
            mis->itemHeight = 52;
            return TRUE;
        }
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_ADD_FOLDER:
            AddFolder();
            return 0;
        case IDC_BTN_STATUS:
            RunSimpleCommand({L"status", L"--short", L"--branch"});
            return 0;
        case IDC_BTN_ADD_ALL:
            RunSimpleCommand({L"add", L"-A"});
            RefreshCurrentRepository(false);
            return 0;
        case IDC_BTN_COMMIT:
            RunCommit();
            return 0;
        case IDC_BTN_PUSH:
            RunSimpleCommand({L"push"});
            return 0;
        case IDC_BTN_PULL:
            RunSimpleCommand({L"pull"});
            return 0;
        case IDC_BTN_FETCH:
            RunSimpleCommand({L"fetch", L"--all"});
            return 0;
        case IDC_BTN_BRANCH:
            ShowBranchMenu();
            return 0;
        case IDC_BTN_REMOTE:
            ShowRemoteMenu();
            return 0;
        case IDM_PROJECT_REMOVE:
            RemoveSelectedProject();
            return 0;
        case IDM_PROJECT_OPEN_EXPLORER:
            OpenSelectedInExplorer();
            return 0;
        case IDM_PROJECT_OPEN_TERMINAL:
            OpenSelectedInTerminal();
            return 0;
        case IDM_PROJECT_REFRESH:
            RefreshCurrentRepository(true);
            return 0;
        case IDM_PROJECT_GIT_INIT:
            RunGitInit(GetSelectedProjectPath());
            return 0;
        case IDM_REMOTE_SHOW:
            RunSimpleCommand({L"remote", L"-v"});
            return 0;
        case IDM_REMOTE_SET_ORIGIN:
            HandleRemoteMenuCommand(LOWORD(wParam));
            return 0;
        case IDM_COMMIT_RESET_HARD:
        case IDM_COMMIT_RESET_SOFT:
        case IDM_COMMIT_EDIT_MESSAGE:
            HandleCommitMenuCommand(LOWORD(wParam));
            return 0;
        default:
            if (LOWORD(wParam) == IDM_BRANCH_CREATE ||
                (LOWORD(wParam) >= IDM_BRANCH_BASE && LOWORD(wParam) < IDM_BRANCH_BASE + 200)) {
                HandleBranchMenuCommand(LOWORD(wParam));
                return 0;
            }
            break;
        }
        break;
    case WM_NOTIFY: {
        auto* header = reinterpret_cast<LPNMHDR>(lParam);
        if (header->idFrom == IDC_LIST_PROJECTS && header->code == NM_RCLICK) {
            POINT point{};
            GetCursorPos(&point);
            ShowProjectContextMenu(point);
            return 0;
        }
        if (header->idFrom == IDC_LIST_COMMITS && header->code == NM_RCLICK) {
            POINT point{};
            GetCursorPos(&point);
            ShowCommitContextMenu(point);
            return 0;
        }
        if (header->idFrom == IDC_LIST_COMMITS && header->code == NM_DBLCLK) {
            ShowCommitDetails();
            return 0;
        }
        if (header->idFrom == IDC_LIST_PROJECTS && header->code == LVN_ITEMCHANGED) {
            auto* info = reinterpret_cast<NMLISTVIEW*>(lParam);
            if ((info->uNewState & LVIS_SELECTED) != 0) {
                RefreshCurrentRepository(true);
            }
            return 0;
        }
        if (header->idFrom == IDC_LIST_COMMITS && header->code == NM_CUSTOMDRAW) {
            auto* draw = reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam);
            if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) {
                return CDRF_NOTIFYITEMDRAW;
            }
            if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                draw->clrText = DarkTheme::TextColor();
                draw->clrTextBk = DarkTheme::ControlBackground();
                return CDRF_NEWFONT;
            }
        }
        if (header->code == NM_CUSTOMDRAW &&
            (header->hwndFrom == ListView_GetHeader(projectList_) ||
             header->hwndFrom == ListView_GetHeader(commitList_))) {
            auto* draw = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
            if (draw->dwDrawStage == CDDS_PREPAINT) {
                return CDRF_NOTIFYITEMDRAW;
            }
            if (draw->dwDrawStage == CDDS_ITEMPREPAINT) {
                HBRUSH brush = CreateSolidBrush(DarkTheme::PanelBackground());
                FillRect(draw->hdc, &draw->rc, brush);
                DeleteObject(brush);
                SetTextColor(draw->hdc, DarkTheme::TextColor());
                SetBkMode(draw->hdc, TRANSPARENT);
                return CDRF_DODEFAULT;
            }
        }
        break;
    }
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, DarkTheme::TextColor());
        SetBkColor(dc, DarkTheme::ControlBackground());
        return reinterpret_cast<LRESULT>(DarkTheme::ControlBrush());
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, DarkTheme::TextColor());
        SetBkColor(dc, DarkTheme::ControlBackground());
        return reinterpret_cast<LRESULT>(DarkTheme::ControlBrush());
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd_, &ps);
        RECT rect{};
        GetClientRect(hwnd_, &rect);
        FillRect(dc, &rect, DarkTheme::WindowBrush());

        RECT leftRect{0, 0, kLeftPanelWidth + kPadding, rect.bottom};
        FillRect(dc, &leftRect, DarkTheme::PanelBrush());

        HPEN pen = CreatePen(PS_SOLID, 1, DarkTheme::BorderColor());
        HGDIOBJ oldPen = SelectObject(dc, pen);
        MoveToEx(dc, kLeftPanelWidth + kPadding, 0, nullptr);
        LineTo(dc, kLeftPanelWidth + kPadding, rect.bottom);
        SelectObject(dc, oldPen);
        DeleteObject(pen);

        EndPaint(hwnd_, &ps);
        return 0;
    }
    case WM_DESTROY:
        PersistWindowSize();
        projectStore_.Save();
        if (commitContextMenu_ != nullptr) {
            DestroyMenu(commitContextMenu_);
            commitContextMenu_ = nullptr;
        }
        FreeProjectListItemData(projectList_);
        if (uiFont_ != nullptr) {
            DeleteObject(uiFont_);
            uiFont_ = nullptr;
        }
        if (projectListFont_ != nullptr) {
            DeleteObject(projectListFont_);
            projectListFont_ = nullptr;
        }
        if (logFont_ != nullptr) {
            DeleteObject(logFont_);
            logFont_ = nullptr;
        }
        if (menuFont_ != nullptr) {
            DeleteObject(menuFont_);
            menuFont_ = nullptr;
            g_menuFont = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

void MainWindow::CreateControls() {
    addFolderButton_ = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW,
                                       0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_BTN_ADD_FOLDER), instance_, nullptr);
    projectList_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                   WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER,
                                   0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_LIST_PROJECTS), instance_, nullptr);
    commitList_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                  WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER,
                                  0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_LIST_COMMITS), instance_, nullptr);
    logEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
                               WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                               0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_EDIT_LOG), instance_, nullptr);
    logScrollBar_ = CreateWindowExW(0, kDarkScrollBarClass, L"",
                                    WS_CHILD,
                                    0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);

    buttonStatus_ = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW,
                                    0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_BTN_STATUS), instance_, nullptr);
    buttonAddAll_ = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW,
                                    0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_BTN_ADD_ALL), instance_, nullptr);
    buttonCommit_ = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW,
                                    0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_BTN_COMMIT), instance_, nullptr);
    buttonPush_ = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW,
                                  0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_BTN_PUSH), instance_, nullptr);
    buttonPull_ = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW,
                                  0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_BTN_PULL), instance_, nullptr);
    buttonFetch_ = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW,
                                   0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_BTN_FETCH), instance_, nullptr);
    buttonBranch_ = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW,
                                    0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_BTN_BRANCH), instance_, nullptr);
    buttonRemote_ = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW,
                                    0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(IDC_BTN_REMOTE), instance_, nullptr);

    ListView_SetExtendedListViewStyle(projectList_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    ListView_SetExtendedListViewStyle(commitList_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    DarkTheme::DisableVisualTheme(projectList_);
    DarkTheme::ApplyDarkControlTheme(projectList_);
    DarkTheme::DisableVisualTheme(commitList_);
    DarkTheme::ApplyDarkControlTheme(commitList_);
    DarkTheme::ApplyDarkControlTheme(logEdit_);
    ListView_SetBkColor(projectList_, DarkTheme::ControlBackground());
    ListView_SetTextBkColor(projectList_, DarkTheme::ControlBackground());
    ListView_SetTextColor(projectList_, DarkTheme::TextColor());
    ListView_SetBkColor(commitList_, DarkTheme::ControlBackground());
    ListView_SetTextBkColor(commitList_, DarkTheme::ControlBackground());
    ListView_SetTextColor(commitList_, DarkTheme::TextColor());
    SetListViewColumn(projectList_, 0, 340, L"Repositories");
    SetListViewColumn(commitList_, 0, 90, LoadStringResource(IDS_COL_HASH));
    SetListViewColumn(commitList_, 1, 384, LoadStringResource(IDS_COL_MESSAGE));
    SetListViewColumn(commitList_, 2, 140, LoadStringResource(IDS_COL_AUTHOR));
    SetListViewColumn(commitList_, 3, 120, LoadStringResource(IDS_COL_DATE));

    AddListViewRowsPadding(projectList_, 8);
    AddListViewRowsPadding(commitList_, 10);

    defaultLogEditProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        logEdit_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MainWindow::LogEditProc)));
    SendMessageW(logEdit_, EM_SETBKGNDCOLOR, 0, DarkTheme::ControlBackground());
    ShowScrollBar(logEdit_, SB_VERT, FALSE);

    SetButtonText(IDC_BTN_ADD_FOLDER, IDS_BTN_ADD_FOLDER);
    SetButtonText(IDC_BTN_STATUS, IDS_BTN_STATUS);
    SetButtonText(IDC_BTN_ADD_ALL, IDS_BTN_ADD_ALL);
    SetButtonText(IDC_BTN_COMMIT, IDS_BTN_COMMIT);
    SetButtonText(IDC_BTN_PUSH, IDS_BTN_PUSH);
    SetButtonText(IDC_BTN_PULL, IDS_BTN_PULL);
    SetButtonText(IDC_BTN_FETCH, IDS_BTN_FETCH);
    SetButtonText(IDC_BTN_BRANCH, IDS_BTN_BRANCH);
    SetButtonText(IDC_BTN_REMOTE, IDS_BTN_REMOTE);
    UpdateWindowTitle();
}

void MainWindow::ShowControls() {
    HWND controls[] = {
        addFolderButton_, projectList_, commitList_, logEdit_, logScrollBar_,
        buttonStatus_, buttonAddAll_, buttonCommit_, buttonPush_,
        buttonPull_, buttonFetch_, buttonBranch_, buttonRemote_
    };
    for (HWND control : controls) {
        ShowWindow(control, SW_SHOW);
    }
    ShowScrollBar(logEdit_, SB_VERT, FALSE);
}

void MainWindow::LayoutControls(int width, int height) {
    const int rightX = kLeftPanelWidth + kPadding * 2;
    const int rightWidth = width - rightX - kPadding;
    const int contentTop = kToolbarHeight + kPadding;
    const int contentBottom = height - kPadding;
    const int totalContentHeight = contentBottom - contentTop;
    const int splitGap = kPadding;
    const int paneHeight = (totalContentHeight - splitGap) / 2;

    MoveWindow(addFolderButton_, kPadding, kPadding, kLeftPanelWidth - kPadding, 34, TRUE);
    MoveWindow(projectList_, kPadding, 54, kLeftPanelWidth - kPadding, height - 66, TRUE);
    UpdateProjectListColumnWidth(kLeftPanelWidth - kPadding);

    const int buttonWidth = 92;
    const int buttonGap = 8;
    HWND buttons[] = {buttonStatus_, buttonAddAll_, buttonCommit_, buttonPush_,
                      buttonPull_, buttonFetch_, buttonBranch_, buttonRemote_};
    for (int i = 0; i < 8; ++i) {
        MoveWindow(buttons[i], rightX + i * (buttonWidth + buttonGap), kPadding, buttonWidth, 30, TRUE);
    }

    MoveWindow(commitList_, rightX, contentTop, rightWidth, paneHeight, TRUE);
    const int logTop = contentTop + paneHeight + splitGap;
    MoveWindow(logEdit_, rightX, logTop, rightWidth - kLogScrollBarWidth, paneHeight, TRUE);
    MoveWindow(logScrollBar_, rightX + rightWidth - kLogScrollBarWidth, logTop, kLogScrollBarWidth, paneHeight, TRUE);
    UpdateLogScrollBar();
}

void MainWindow::UpdateProjectListColumnWidth(int listWidth) {
    const int safeWidth = std::max(120, listWidth - 24);
    ListView_SetColumnWidth(projectList_, 0, safeWidth);
}

void MainWindow::UpdateLogScrollBar() {
    if (logEdit_ == nullptr || logScrollBar_ == nullptr) {
        return;
    }
    ShowScrollBar(logEdit_, SB_VERT, FALSE);
    InvalidateRect(logScrollBar_, nullptr, TRUE);
}

void MainWindow::ScrollLogToPosition(int position) {
    const int current = static_cast<int>(SendMessageW(logEdit_, EM_GETFIRSTVISIBLELINE, 0, 0));
    const int delta = position - current;
    if (delta != 0) {
        SendMessageW(logEdit_, EM_LINESCROLL, 0, delta);
        UpdateLogScrollBar();
    }
}

void MainWindow::ApplyFonts() {
    uiFont_ = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    projectListFont_ = CreateFontW(-22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    logFont_ = CreateFontW(-22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
    menuFont_ = CreateFontW(-24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_menuFont = menuFont_;
    HWND controls[] = {addFolderButton_, commitList_, logEdit_,
                       buttonStatus_, buttonAddAll_, buttonCommit_, buttonPush_,
                       buttonPull_, buttonFetch_, buttonBranch_, buttonRemote_};
    for (HWND control : controls) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
    }
    SendMessageW(projectList_, WM_SETFONT, reinterpret_cast<WPARAM>(projectListFont_), TRUE);
    SendMessageW(logEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(logFont_), TRUE);

    SendMessageW(ListView_GetHeader(projectList_), WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
    SendMessageW(ListView_GetHeader(commitList_), WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
}

void MainWindow::LoadProjectsIntoList() {
    FreeProjectListItemData(projectList_);
    ListView_DeleteAllItems(projectList_);
    int index = 0;
    for (const std::wstring& path : projectStore_.GetProjects()) {
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        std::wstring label = BaseNameFromPath(path);
        item.iItem = index++;
        item.pszText = label.data();
        item.lParam = reinterpret_cast<LPARAM>(new std::wstring(path));
        ListView_InsertItem(projectList_, &item);
    }

    if (!projectStore_.GetLastProject().empty()) {
        SelectProjectByPath(projectStore_.GetLastProject());
    } else if (ListView_GetItemCount(projectList_) > 0) {
        ListView_SetItemState(projectList_, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
}

void MainWindow::RefreshCurrentRepository(bool runStatusCommand) {
    const std::wstring path = GetSelectedProjectPath();
    if (path.empty()) {
        ListView_DeleteAllItems(commitList_);
        UpdateWindowTitle();
        ClearLog();
        currentProjectPath_.clear();
        return;
    }

    if (currentProjectPath_ != path) {
        currentProjectPath_ = path;
        ClearLog();
    }

    projectStore_.SetLastProject(path);
    projectStore_.Save();
    UpdateWindowTitle();

    if (!GitRunner::IsGitRepository(path)) {
        ShowCommitPlaceholder(LoadStringResource(IDS_MSG_NOT_GIT_REPO_HINT));
        return;
    }

    if (runStatusCommand) {
        RunSimpleCommand({L"status", L"--short", L"--branch"});
    }
    RefreshCommitList();
}

void MainWindow::RefreshCommitList() {
    ListView_DeleteAllItems(commitList_);
    const std::wstring path = GetSelectedProjectPath();
    if (path.empty()) {
        return;
    }

    if (!GitRunner::IsGitRepository(path)) {
        ShowCommitPlaceholder(LoadStringResource(IDS_MSG_NOT_GIT_REPO_HINT));
        return;
    }

    const auto commits = commitRepository_.LoadRecent(path, 50);
    for (size_t i = 0; i < commits.size(); ++i) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.pszText = const_cast<LPWSTR>(commits[i].hash.c_str());
        ListView_InsertItem(commitList_, &item);
        ListView_SetItemText(commitList_, static_cast<int>(i), 1, const_cast<LPWSTR>(commits[i].message.c_str()));
        ListView_SetItemText(commitList_, static_cast<int>(i), 2, const_cast<LPWSTR>(commits[i].author.c_str()));
        ListView_SetItemText(commitList_, static_cast<int>(i), 3, const_cast<LPWSTR>(commits[i].date.c_str()));
    }
}

void MainWindow::ShowCommitPlaceholder(const std::wstring& message) {
    ListView_DeleteAllItems(commitList_);

    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = 0;
    std::wstring dash = L"-";
    item.pszText = dash.data();
    ListView_InsertItem(commitList_, &item);

    std::wstring text = message;
    ListView_SetItemText(commitList_, 0, 1, text.data());
}

void MainWindow::ClearLog() {
    SetWindowTextW(logEdit_, L"");
    UpdateLogScrollBar();
}

void MainWindow::AppendLog(const std::wstring& text) {
    SYSTEMTIME st{};
    GetLocalTime(&st);

    std::wstringstream stream;
    stream << L"["
           << (st.wHour < 10 ? L"0" : L"") << st.wHour << L":"
           << (st.wMinute < 10 ? L"0" : L"") << st.wMinute << L":"
           << (st.wSecond < 10 ? L"0" : L"") << st.wSecond << L"] "
           << text << L"\r\n";

    AppendLogRichText(stream.str(), kLogDefaultText);
}

void MainWindow::AppendLogRichText(const std::wstring& text, COLORREF color) {
    CHARRANGE range{-1, -1};
    SendMessageW(logEdit_, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));

    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR;
    format.crTextColor = color;
    SendMessageW(logEdit_, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    SendMessageW(logEdit_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    SendMessageW(logEdit_, WM_VSCROLL, MAKEWPARAM(SB_BOTTOM, 0), 0);
    SendMessageW(logEdit_, EM_SCROLLCARET, 0, 0);
    UpdateLogScrollBar();
}

void MainWindow::AppendCommandResult(const GitCommandResult& result) {
    AppendLog(L"$ " + result.commandLine);
    if (!result.output.empty()) {
        const std::wstring output = result.output;
        std::wstring buffer;
        COLORREF currentColor = kLogDefaultText;

        auto flushBuffer = [this, &buffer, &currentColor]() {
            if (!buffer.empty()) {
                AppendLogRichText(buffer, currentColor);
                buffer.clear();
            }
        };

        for (size_t i = 0; i < output.size(); ++i) {
            const wchar_t ch = output[i];
            if (ch == L'\x001b' && i + 1 < output.size() && output[i + 1] == L'[') {
                flushBuffer();
                i += 2;
                std::wstring codeText;
                while (i < output.size() && output[i] != L'm') {
                    codeText += output[i];
                    ++i;
                }

                std::wstringstream codeStream(codeText);
                std::wstring segment;
                while (std::getline(codeStream, segment, L';')) {
                    if (segment.empty()) {
                        continue;
                    }
                    const int code = _wtoi(segment.c_str());
                    if (code == 0 || code == 39) {
                        currentColor = kLogDefaultText;
                    } else {
                        currentColor = MapAnsiColor(code, currentColor);
                    }
                }
                continue;
            }
            if (ch != L'\r') {
                buffer.push_back(ch);
            }
        }
        flushBuffer();
    }
    AppendLog(result.success ? L"Command completed." : L"Command failed.");
}

void MainWindow::SelectProjectByPath(const std::wstring& path) {
    const int count = ListView_GetItemCount(projectList_);
    for (int i = 0; i < count; ++i) {
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = i;
        if (ListView_GetItem(projectList_, &item) && item.lParam != 0) {
            const auto* itemPath = reinterpret_cast<const std::wstring*>(item.lParam);
            if (*itemPath == path) {
                ListView_SetItemState(projectList_, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                ListView_EnsureVisible(projectList_, i, FALSE);
                return;
            }
        }
    }
}

std::wstring MainWindow::GetSelectedProjectPath() const {
    const int index = ListView_GetNextItem(projectList_, -1, LVNI_SELECTED);
    if (index < 0) {
        return L"";
    }

    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = index;
    if (ListView_GetItem(projectList_, &item) && item.lParam != 0) {
        return *reinterpret_cast<const std::wstring*>(item.lParam);
    }
    return L"";
}

std::wstring MainWindow::GetSelectedCommitHash() const {
    const int index = ListView_GetNextItem(commitList_, -1, LVNI_SELECTED);
    if (index < 0) {
        return L"";
    }

    wchar_t buffer[256] = {};
    ListView_GetItemText(commitList_, index, 0, buffer, 256);
    return buffer;
}

std::wstring MainWindow::GetSelectedProjectDisplayName() const {
    const std::wstring path = GetSelectedProjectPath();
    return path.empty() ? LoadStringResource(IDS_APP_TITLE) : BaseNameFromPath(path);
}

void MainWindow::AddFolder() {
    BROWSEINFOW info{};
    info.hwndOwner = hwnd_;
    std::wstring title = LoadStringResource(IDS_MSG_ADD_REPO_TITLE);
    info.lpszTitle = title.c_str();
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE itemId = SHBrowseForFolderW(&info);
    if (itemId == nullptr) {
        return;
    }

    wchar_t path[MAX_PATH] = {};
    SHGetPathFromIDListW(itemId, path);
    CoTaskMemFree(itemId);

    const std::wstring repoPath = path;
    if (repoPath.empty()) {
        return;
    }
    if (projectStore_.Contains(repoPath)) {
        MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_PROJECT_EXISTS).c_str(),
                    LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONINFORMATION);
        return;
    }
    if (!projectStore_.AddProject(repoPath) || !projectStore_.Save()) {
        MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_ADD_FOLDER_FAIL).c_str(),
                    LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONERROR);
        return;
    }

    LoadProjectsIntoList();
    SelectProjectByPath(repoPath);
    RefreshCurrentRepository(true);
}

void MainWindow::RemoveSelectedProject() {
    const std::wstring path = GetSelectedProjectPath();
    if (path.empty()) {
        return;
    }
    if (MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_REMOVE_REPO).c_str(),
                    LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONQUESTION | MB_YESNO) != IDYES) {
        return;
    }
    projectStore_.RemoveProject(path);
    projectStore_.Save();
    LoadProjectsIntoList();
    RefreshCurrentRepository(false);
}

void MainWindow::ShowProjectContextMenu(POINT screenPoint) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_PROJECT_GIT_INIT, L"git init");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_PROJECT_REMOVE, LoadStringResource(IDS_MENU_REMOVE).c_str());
    AppendMenuW(menu, MF_STRING, IDM_PROJECT_OPEN_EXPLORER, LoadStringResource(IDS_MENU_OPEN_EXPLORER).c_str());
    AppendMenuW(menu, MF_STRING, IDM_PROJECT_OPEN_TERMINAL, LoadStringResource(IDS_MENU_OPEN_TERMINAL).c_str());
    AppendMenuW(menu, MF_STRING, IDM_PROJECT_REFRESH, LoadStringResource(IDS_MENU_REFRESH).c_str());
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, screenPoint.x, screenPoint.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

void MainWindow::ShowCommitContextMenu(POINT screenPoint) {
    const std::wstring hash = GetSelectedCommitHash();
    if (hash.empty()) {
        return;
    }

    if (commitContextMenu_ != nullptr) {
        DestroyMenu(commitContextMenu_);
        commitContextMenu_ = nullptr;
    }

    commitContextMenu_ = CreatePopupMenu();
    UINT insertIndex = 0;
    if (CanEditSelectedCommitMessage()) {
        InsertOwnerDrawMenuItem(commitContextMenu_, IDM_COMMIT_EDIT_MESSAGE, L"edit commit message", insertIndex++);
    }
    InsertOwnerDrawMenuItem(commitContextMenu_, IDM_COMMIT_RESET_HARD, L"git reset here (Hard)", insertIndex++);
    InsertOwnerDrawMenuItem(commitContextMenu_, IDM_COMMIT_RESET_SOFT, L"git reset here (Soft)", insertIndex++);
    TrackPopupMenu(commitContextMenu_, TPM_RIGHTBUTTON, screenPoint.x, screenPoint.y, 0, hwnd_, nullptr);
}

void MainWindow::ShowCommitDetails() {
    const std::wstring repoPath = GetSelectedProjectPath();
    const std::wstring hash = GetSelectedCommitHash();
    if (repoPath.empty() || hash.empty() || !GitRunner::IsGitRepository(repoPath)) {
        return;
    }

    auto* state = new CommitDetailWindowState();
    state->repoPath = repoPath;
    state->commitHash = hash;
    state->title = L"Commit Details - " + hash;
    state->summary = FormatCommitSummary(GitRunner::GetCommitDetails(repoPath, hash));
    state->diffs = GitRunner::GetCommitFileDiffs(repoPath, hash);
    state->uiFont = uiFont_;
    state->codeFont = logFont_;

    RECT mainRect{};
    GetWindowRect(hwnd_, &mainRect);
    const int width = ((mainRect.right - mainRect.left) * 9) / 10;
    const int height = ((mainRect.bottom - mainRect.top) * 9) / 10;
    const int x = mainRect.left + ((mainRect.right - mainRect.left) - width) / 2;
    const int y = mainRect.top + ((mainRect.bottom - mainRect.top) - height) / 2;

    HWND detail = CreateWindowExW(
        0,
        kCommitDetailWindowClass,
        state->title.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_MAXIMIZEBOX | WS_THICKFRAME,
        x, y, width, height,
        hwnd_, nullptr, instance_, state);

    if (detail == nullptr) {
        delete state;
    }
}

void MainWindow::OpenSelectedInExplorer() {
    const std::wstring path = GetSelectedProjectPath();
    if (!path.empty()) {
        ShellExecuteW(hwnd_, L"open", L"explorer.exe", path.c_str(), nullptr, SW_SHOWNORMAL);
    }
}

void MainWindow::OpenSelectedInTerminal() {
    const std::wstring path = GetSelectedProjectPath();
    if (!path.empty()) {
        std::wstring parameters = L"/K cd /d \"" + path + L"\"";
        ShellExecuteW(hwnd_, L"open", L"cmd.exe", parameters.c_str(), nullptr, SW_SHOWNORMAL);
    }
}

void MainWindow::RunSimpleCommand(const std::vector<std::wstring>& args) {
    const std::wstring path = GetSelectedProjectPath();
    if (path.empty()) {
        MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_SELECT_REPO).c_str(),
                    LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONINFORMATION);
        return;
    }
    if (commandRunning_) {
        MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_COMMAND_RUNNING).c_str(),
                    LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONINFORMATION);
        return;
    }

    commandRunning_ = true;
    const GitCommandResult result = GitRunner::RunGitCommand(path, args);
    commandRunning_ = false;
    AppendCommandResult(result);
    RefreshCommitList();
}

void MainWindow::RunGitInit(const std::wstring& requestedPath) {
    std::wstring repoPath = requestedPath;
    if (repoPath.empty()) {
        BROWSEINFOW info{};
        info.hwndOwner = hwnd_;
        std::wstring title = LoadStringResource(IDS_MSG_INIT_REPO_TITLE);
        info.lpszTitle = title.c_str();
        info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

        PIDLIST_ABSOLUTE itemId = SHBrowseForFolderW(&info);
        if (itemId == nullptr) {
            return;
        }

        wchar_t pathBuffer[MAX_PATH] = {};
        SHGetPathFromIDListW(itemId, pathBuffer);
        CoTaskMemFree(itemId);
        repoPath = pathBuffer;
    }

    if (repoPath.empty()) {
        return;
    }

    if (GitRunner::IsGitRepository(repoPath)) {
        MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_ALREADY_GIT_REPO).c_str(),
                    LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONINFORMATION);
        return;
    }

    const std::filesystem::path folder(repoPath);
    const std::filesystem::path gitIgnorePath = folder / L".gitignore";
    if (!std::filesystem::exists(gitIgnorePath)) {
        std::wofstream output(gitIgnorePath.c_str(), std::ios::trunc);
        if (!output.is_open()) {
            MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_INIT_GITIGNORE_FAILED).c_str(),
                        LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONERROR);
            return;
        }

        const auto& lines = projectStore_.GetGitIgnoreTemplateLines();
        for (size_t i = 0; i < lines.size(); ++i) {
            output << lines[i];
            if (i + 1 < lines.size()) {
                output << L"\n";
            }
        }
        output.close();
        AppendLog(LoadStringResource(IDS_MSG_INIT_GITIGNORE_CREATED));
    }

    const GitCommandResult result = GitRunner::RunGitCommand(repoPath, {L"init"});
    AppendCommandResult(result);
    if (!result.success) {
        return;
    }

    if (!projectStore_.Contains(repoPath)) {
        projectStore_.AddProject(repoPath);
        projectStore_.Save();
        LoadProjectsIntoList();
    }
    SelectProjectByPath(repoPath);
    RefreshCurrentRepository(true);
    MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_INIT_SUCCESS).c_str(),
                LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONINFORMATION);
}

void MainWindow::RunCommit() {
    bool accepted = false;
    const std::wstring message = Trim(PromptForText(
        IDS_MSG_COMMIT_TITLE, IDS_MSG_COMMIT_PROMPT, L"", true, &accepted));
    if (!accepted) {
        return;
    }
    if (message.empty()) {
        MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_COMMIT_EMPTY).c_str(),
                    LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONWARNING);
        return;
    }

    wchar_t tempPath[MAX_PATH] = {};
    wchar_t tempFile[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempPath);
    GetTempFileNameW(tempPath, L"gcm", 0, tempFile);

    const std::string utf8 = WideToUtf8(message);
    {
        std::ofstream output(tempFile, std::ios::binary | std::ios::trunc);
        output.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    }

    RunSimpleCommand({L"commit", L"-F", tempFile});
    DeleteFileW(tempFile);
}

void MainWindow::ShowBranchMenu() {
    const std::wstring path = GetSelectedProjectPath();
    if (path.empty()) {
        MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_SELECT_REPO).c_str(),
                    LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONINFORMATION);
        return;
    }

    cachedBranches_ = GitRunner::GetLocalBranches(path);
    if (cachedBranches_.empty()) {
        MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_NO_BRANCHES).c_str(),
                    LoadStringResource(IDS_MSG_BRANCH_TITLE).c_str(), MB_ICONINFORMATION);
        return;
    }

    const std::wstring currentBranch = GitRunner::GetCurrentBranch(path);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_BRANCH_CREATE, L"+ Create Branch");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    for (size_t i = 0; i < cachedBranches_.size(); ++i) {
        UINT flags = MF_STRING;
        if (cachedBranches_[i] == currentBranch) {
            flags |= MF_CHECKED;
        }
        AppendMenuW(menu, flags, IDM_BRANCH_BASE + static_cast<UINT>(i), cachedBranches_[i].c_str());
    }

    RECT rect{};
    GetWindowRect(buttonBranch_, &rect);
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN, rect.left, rect.bottom, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

void MainWindow::HandleBranchMenuCommand(UINT commandId) {
    if (commandId == IDM_BRANCH_CREATE) {
        const std::wstring branchName = Trim(PromptForText(IDS_MSG_BRANCH_TITLE, IDS_MSG_BRANCH_PROMPT));
        if (branchName.empty()) {
            MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_BRANCH_EMPTY).c_str(),
                        LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONWARNING);
            return;
        }
        RunSimpleCommand({L"checkout", L"-b", branchName});
        return;
    }

    const size_t index = commandId - IDM_BRANCH_BASE;
    if (index < cachedBranches_.size()) {
        RunSimpleCommand({L"checkout", cachedBranches_[index]});
    }
}

void MainWindow::ShowRemoteMenu() {
    RECT rect{};
    GetWindowRect(buttonRemote_, &rect);

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_REMOTE_SHOW, L"Show Remotes");
    AppendMenuW(menu, MF_STRING, IDM_REMOTE_SET_ORIGIN, L"Set Origin");
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN, rect.left, rect.bottom, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

void MainWindow::HandleRemoteMenuCommand(UINT) {
    const std::wstring remoteUrl = Trim(PromptForText(IDS_MSG_REMOTE_TITLE, IDS_MSG_REMOTE_PROMPT));
    if (remoteUrl.empty()) {
        MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_REMOTE_EMPTY).c_str(),
                    LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONWARNING);
        return;
    }

    const std::wstring path = GetSelectedProjectPath();
    const GitCommandResult getResult = GitRunner::RunGitCommand(path, {L"remote", L"get-url", L"origin"});
    if (getResult.success) {
        AppendCommandResult(GitRunner::RunGitCommand(path, {L"remote", L"set-url", L"origin", remoteUrl}));
    } else {
        AppendCommandResult(GitRunner::RunGitCommand(path, {L"remote", L"add", L"origin", remoteUrl}));
    }
    MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_REMOTE_UPDATED).c_str(),
                LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONINFORMATION);
}

bool MainWindow::CanEditSelectedCommitMessage() const {
    const std::wstring path = GetSelectedProjectPath();
    const std::wstring selectedHash = GetSelectedCommitHash();
    if (path.empty() || selectedHash.empty()) {
        return false;
    }

    const GitCommandResult headResult = GitRunner::RunGitCommand(path, {L"rev-parse", L"HEAD"});
    if (!headResult.success) {
        return false;
    }

    const std::wstring headHash = Trim(headResult.output);
    if (headHash.empty() || selectedHash != headHash.substr(0, std::min(selectedHash.size(), headHash.size()))) {
        return false;
    }

    const GitCommandResult upstreamResult = GitRunner::RunGitCommand(path, {L"rev-parse", L"--abbrev-ref", L"@{upstream}"});
    if (!upstreamResult.success) {
        return true;
    }

    const GitCommandResult aheadResult = GitRunner::RunGitCommand(path, {L"rev-list", L"--count", L"@{upstream}..HEAD"});
    if (!aheadResult.success) {
        return false;
    }

    return _wtoi(Trim(aheadResult.output).c_str()) > 0;
}

void MainWindow::HandleCommitMenuCommand(UINT commandId) {
    const std::wstring hash = GetSelectedCommitHash();
    if (hash.empty()) {
        return;
    }

    if (commandId == IDM_COMMIT_EDIT_MESSAGE) {
        if (!CanEditSelectedCommitMessage()) {
            MessageBoxW(hwnd_, L"Only the latest local unpushed commit can be edited.",
                        LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONINFORMATION);
            return;
        }

        const std::wstring path = GetSelectedProjectPath();
        const GitCommandResult currentMessageResult = GitRunner::RunGitCommand(path, {L"log", L"-1", L"--format=%B", L"HEAD"});
        const std::wstring currentMessage = currentMessageResult.success ? Trim(currentMessageResult.output) : L"";
        bool accepted = false;
        const std::wstring newMessage = PromptForText(
            IDS_MSG_COMMIT_TITLE, IDS_MSG_COMMIT_PROMPT, currentMessage, true, &accepted);
        if (!accepted) {
            return;
        }
        if (Trim(newMessage).empty()) {
            MessageBoxW(hwnd_, LoadStringResource(IDS_MSG_COMMIT_EMPTY).c_str(),
                        LoadStringResource(IDS_APP_TITLE).c_str(), MB_ICONWARNING);
            return;
        }

        wchar_t tempPath[MAX_PATH] = {};
        wchar_t tempFile[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, tempPath);
        GetTempFileNameW(tempPath, L"gcm", 0, tempFile);

        const std::string utf8 = WideToUtf8(newMessage);
        {
            std::ofstream output(tempFile, std::ios::binary | std::ios::trunc);
            output.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
        }

        const GitCommandResult result = GitRunner::RunGitCommand(path, {L"commit", L"--amend", L"-F", tempFile});
        DeleteFileW(tempFile);
        AppendCommandResult(result);
        RefreshCommitList();
        return;
    }

    if (commandId == IDM_COMMIT_RESET_HARD) {
        const int result = MessageBoxW(
            hwnd_,
            L"Dangerous operation.\nThis will discard current branch modifications.\n\nContinue with git reset --hard?",
            L"Warning",
            MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
        if (result != IDYES) {
            return;
        }
        RunSimpleCommand({L"reset", L"--hard", hash});
        return;
    }

    if (commandId == IDM_COMMIT_RESET_SOFT) {
        RunSimpleCommand({L"reset", L"--soft", hash});
    }
}

void MainWindow::SetButtonText(int controlId, int stringId) {
    SetWindowTextW(GetDlgItem(hwnd_, controlId), LoadStringResource(stringId).c_str());
}

void MainWindow::SetWindowTextFromString(int stringId) {
    SetWindowTextW(hwnd_, LoadStringResource(stringId).c_str());
}

void MainWindow::UpdateWindowTitle() {
    std::wstring title = LoadStringResource(IDS_APP_TITLE);
    const std::wstring path = GetSelectedProjectPath();
    if (!path.empty()) {
        title += L" - " + BaseNameFromPath(path);
    }
    SetWindowTextW(hwnd_, title.c_str());
}

std::wstring MainWindow::PromptForText(
    int titleId,
    int promptId,
    const std::wstring& initialValue,
    bool multiline,
    bool* accepted) {
    PromptDialogState state;
    state.title = LoadStringResource(titleId);
    state.prompt = LoadStringResource(promptId);
    state.text = initialValue;
    state.multiline = multiline;

    std::vector<BYTE> dialogTemplate = BuildPromptDialogTemplate(multiline);
    DialogBoxIndirectParamW(instance_, reinterpret_cast<DLGTEMPLATE*>(dialogTemplate.data()),
                            hwnd_, PromptDialogProc, reinterpret_cast<LPARAM>(&state));
    if (accepted != nullptr) {
        *accepted = state.accepted;
    }
    return state.accepted ? state.text : L"";
}

std::wstring MainWindow::LoadStringResource(int resourceId) const {
    wchar_t buffer[512] = {};
    LoadStringW(instance_, resourceId, buffer, 512);
    return buffer;
}

std::wstring MainWindow::GetExecutableDirectory() const {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring fullPath = path;
    const size_t pos = fullPath.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : fullPath.substr(0, pos);
}

RECT MainWindow::CalculateCenteredWindowRect(int width, int height) const {
    const int x = std::max(0, (GetSystemMetrics(SM_CXSCREEN) - width) / 2);
    const int y = std::max(0, (GetSystemMetrics(SM_CYSCREEN) - height) / 2);
    return RECT{x, y, x + width, y + height};
}

void MainWindow::PrepareInitialShow() {
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    if ((exStyle & WS_EX_LAYERED) == 0) {
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    }
    SetLayeredWindowAttributes(hwnd_, 0, 0, LWA_ALPHA);
    initialShowPrepared_ = true;
}

void MainWindow::FinalizeInitialShow(int nCmdShow) {
    if (!initialShowPrepared_) {
        ShowWindow(hwnd_, nCmdShow);
        UpdateWindow(hwnd_);
        return;
    }

    ShowWindow(hwnd_, nCmdShow);
    RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
    UpdateWindow(hwnd_);
    SetLayeredWindowAttributes(hwnd_, 0, 255, LWA_ALPHA);
    initialShowPrepared_ = false;
}

void MainWindow::PersistWindowSize() {
    RECT rect{};
    if (GetWindowRect(hwnd_, &rect)) {
        projectStore_.SetWindowSize(rect.right - rect.left, rect.bottom - rect.top);
    }
}
