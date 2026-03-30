#include "darkui/table.h"

#include <algorithm>
#include <array>

namespace darkui {
namespace {

constexpr wchar_t kTableClassName[] = L"DarkUiTableControl";

ATOM EnsureTableClassRegistered(HINSTANCE instance);
LRESULT CALLBACK TableWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

}  // namespace

struct Table::Impl {
    Table* owner = nullptr;
    HINSTANCE instance = nullptr;
    HFONT font = nullptr;
    HBRUSH brushBackground = nullptr;
    HBRUSH brushHeaderBackground = nullptr;
    HBRUSH brushSelected = nullptr;
    HPEN penGrid = nullptr;
    std::vector<TableColumn> columns;
    std::vector<TableRow> rows;
    int selectedRow = -1;

    explicit Impl(Table* table) : owner(table) {}

    ~Impl() {
        if (font) DeleteObject(font);
        if (brushBackground) DeleteObject(brushBackground);
        if (brushHeaderBackground) DeleteObject(brushHeaderBackground);
        if (brushSelected) DeleteObject(brushSelected);
        if (penGrid) DeleteObject(penGrid);
    }

    void UpdateThemeResources() {
        if (font) DeleteObject(font);
        if (brushBackground) DeleteObject(brushBackground);
        if (brushHeaderBackground) DeleteObject(brushHeaderBackground);
        if (brushSelected) DeleteObject(brushSelected);
        if (penGrid) DeleteObject(penGrid);

        font = CreateFont(owner->theme_.uiFont);
        brushBackground = CreateSolidBrush(owner->theme_.tableBackground);
        brushHeaderBackground = CreateSolidBrush(owner->theme_.tableHeaderBackground);
        brushSelected = CreateSolidBrush(owner->theme_.buttonHot);
        penGrid = CreatePen(PS_SOLID, 1, owner->theme_.tableGrid);

        if (owner->tableHwnd_) {
            SendMessageW(owner->tableHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            InvalidateRect(owner->tableHwnd_, nullptr, TRUE);
        }
    }

    std::wstring GetCellText(int rowIndex, int columnIndex) const {
        if (rowIndex < 0 || rowIndex >= static_cast<int>(rows.size())) {
            return L"";
        }
        const auto& row = rows[rowIndex];
        if (columnIndex < 0 || columnIndex >= static_cast<int>(row.size())) {
            return L"";
        }
        return row[columnIndex];
    }

    HFONT SelectControlFont(HDC dc, HWND control) {
        HFONT current = reinterpret_cast<HFONT>(SendMessageW(control, WM_GETFONT, 0, 0));
        return current ? reinterpret_cast<HFONT>(SelectObject(dc, current)) : nullptr;
    }

    int ColumnCount() const {
        return std::max(1, static_cast<int>(columns.size()));
    }

    int TotalColumnWeight() const {
        int total = 0;
        for (const auto& column : columns) {
            total += std::max(1, column.width);
        }
        return std::max(1, total);
    }

    std::vector<int> BuildColumnEdges(const RECT& client) const {
        const int count = ColumnCount();
        std::vector<int> edges(static_cast<std::size_t>(count) + 1, client.left);
        edges[0] = client.left;
        if (count <= 0) {
            return edges;
        }

        const int width = static_cast<int>(client.right - client.left);
        const int total = TotalColumnWeight();
        int offset = client.left;
        for (int i = 0; i < count; ++i) {
            if (i == count - 1) {
                edges[static_cast<std::size_t>(i + 1)] = client.right;
            } else {
                const int weight = (i < static_cast<int>(columns.size())) ? std::max(1, columns[i].width) : 1;
                offset += (width * weight) / total;
                edges[static_cast<std::size_t>(i + 1)] = offset;
            }
        }
        return edges;
    }

    int RowTop(int rowIndex) const {
        return owner->theme_.tableHeaderHeight + rowIndex * owner->theme_.tableRowHeight;
    }

    int HitTestRow(int y) const {
        if (y < owner->theme_.tableHeaderHeight) {
            return -1;
        }
        const int index = (y - owner->theme_.tableHeaderHeight) / std::max(1, owner->theme_.tableRowHeight);
        return (index >= 0 && index < static_cast<int>(rows.size())) ? index : -1;
    }

    void DrawTextCell(HDC dc, const std::wstring& text, RECT rect, COLORREF color, UINT format) {
        rect.left += owner->theme_.textPadding;
        rect.right -= owner->theme_.textPadding;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        DrawTextW(dc, text.c_str(), -1, &rect, format | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    void Paint(HDC targetDc) {
        RECT client{};
        GetClientRect(owner->tableHwnd_, &client);

        HDC memDc = CreateCompatibleDC(targetDc);
        if (!memDc) {
            FillRect(targetDc, &client, brushBackground);
            return;
        }
        HBITMAP bitmap = CreateCompatibleBitmap(targetDc,
                                               std::max(1, static_cast<int>(client.right)),
                                               std::max(1, static_cast<int>(client.bottom)));
        if (!bitmap) {
            FillRect(targetDc, &client, brushBackground);
            DeleteDC(memDc);
            return;
        }
        HGDIOBJ oldBitmap = SelectObject(memDc, bitmap);
        HFONT oldFont = SelectControlFont(memDc, owner->tableHwnd_);
        HGDIOBJ oldPen = SelectObject(memDc, penGrid);
        HGDIOBJ oldBrush = SelectObject(memDc, GetStockObject(NULL_BRUSH));

        FillRect(memDc, &client, brushBackground);

        RECT headerRect{client.left, client.top, client.right, std::min(static_cast<int>(client.bottom), owner->theme_.tableHeaderHeight)};
        FillRect(memDc, &headerRect, brushHeaderBackground);

        const int columnCount = ColumnCount();
        const std::vector<int> columnEdges = BuildColumnEdges(client);

        for (int col = 0; col < columnCount; ++col) {
            RECT cell{
                columnEdges[static_cast<std::size_t>(col)],
                client.top,
                columnEdges[static_cast<std::size_t>(col + 1)],
                headerRect.bottom
            };
            Rectangle(memDc, cell.left, cell.top, cell.right, cell.bottom);
            if (col < static_cast<int>(columns.size())) {
                UINT format = DT_LEFT;
                if (columns[col].format == LVCFMT_CENTER) {
                    format = DT_CENTER;
                } else if (columns[col].format == LVCFMT_RIGHT) {
                    format = DT_RIGHT;
                }
                DrawTextCell(memDc, columns[col].text, cell, owner->theme_.tableHeaderText, format);
            }
        }

        const int visibleRows = std::max(0, static_cast<int>((client.bottom - owner->theme_.tableHeaderHeight + owner->theme_.tableRowHeight - 1) /
                                                std::max(1, owner->theme_.tableRowHeight)));
        const int rowsToPaint = std::min(static_cast<int>(rows.size()), visibleRows);

        for (int row = 0; row < rowsToPaint; ++row) {
            RECT rowRect{
                client.left,
                RowTop(row),
                client.right,
                std::min(static_cast<int>(client.bottom), RowTop(row) + owner->theme_.tableRowHeight)
            };
            FillRect(memDc, &rowRect, row == selectedRow ? brushSelected : brushBackground);

            for (int col = 0; col < columnCount; ++col) {
                RECT cell{
                    columnEdges[static_cast<std::size_t>(col)],
                    rowRect.top,
                    columnEdges[static_cast<std::size_t>(col + 1)],
                    rowRect.bottom
                };
                Rectangle(memDc, cell.left, cell.top, cell.right, cell.bottom);
                DrawTextCell(memDc,
                             GetCellText(row, col),
                             cell,
                             row == selectedRow ? owner->theme_.tableHeaderText : owner->theme_.tableText,
                             DT_LEFT);
            }
        }

        const int bodyStart = owner->theme_.tableHeaderHeight + rowsToPaint * owner->theme_.tableRowHeight;
        if (bodyStart < client.bottom && owner->drawEmptyGrid_) {
            for (int col = 0; col < columnCount; ++col) {
                const int x = columnEdges[static_cast<std::size_t>(col + 1)];
                MoveToEx(memDc, x - 1, owner->theme_.tableHeaderHeight, nullptr);
                LineTo(memDc, x - 1, client.bottom);
            }
            for (int y = bodyStart; y < client.bottom; y += owner->theme_.tableRowHeight) {
                MoveToEx(memDc, client.left, y, nullptr);
                LineTo(memDc, client.right, y);
            }
        }

        Rectangle(memDc, client.left, client.top, client.right, client.bottom);
        BitBlt(targetDc, 0, 0, client.right, client.bottom, memDc, 0, 0, SRCCOPY);

        SelectObject(memDc, oldBrush);
        SelectObject(memDc, oldPen);
        if (oldFont) {
            SelectObject(memDc, oldFont);
        }
        SelectObject(memDc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(memDc);
    }

    static LRESULT CALLBACK TableWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));

        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = reinterpret_cast<Impl*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            return TRUE;
        }

        if (!self) {
            return DefWindowProcW(window, message, wParam, lParam);
        }

        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(window, &ps);
            self->Paint(dc);
            EndPaint(window, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            const int row = self->HitTestRow(static_cast<int>(HIWORD(lParam)));
            if (row != self->selectedRow) {
                self->selectedRow = row;
                InvalidateRect(window, nullptr, FALSE);
            }
            SetFocus(window);
            return 0;
        }
        case WM_SIZE:
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
};

namespace {

ATOM EnsureTableClassRegistered(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) {
        return atom;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = TableWindowProcThunk;
    wc.hInstance = instance;
    wc.lpszClassName = kTableClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    atom = RegisterClassExW(&wc);
    return atom;
}

}  // namespace

namespace {

LRESULT CALLBACK TableWindowProcThunk(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return Table::Impl::TableWindowProc(window, message, wParam, lParam);
}

}  // namespace

Table::Table() : impl_(std::make_unique<Impl>(this)) {}

Table::~Table() {
    Destroy();
}

bool Table::Create(HWND parent, int controlId, const Theme& theme, DWORD style, DWORD exStyle) {
    Destroy();
    parentHwnd_ = parent;
    controlId_ = controlId;
    theme_ = theme;
    impl_->instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!impl_->instance) {
        impl_->instance = GetModuleHandleW(nullptr);
    }

    EnsureTableClassRegistered(impl_->instance);
    tableHwnd_ = CreateWindowExW(exStyle,
                                 kTableClassName,
                                 L"",
                                 style | WS_CLIPSIBLINGS,
                                 0,
                                 0,
                                 0,
                                 0,
                                 parent,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                 impl_->instance,
                                 impl_.get());
    if (!tableHwnd_) {
        Destroy();
        return false;
    }

    impl_->UpdateThemeResources();
    return true;
}

void Table::Destroy() {
    if (tableHwnd_) {
        DestroyWindow(tableHwnd_);
        tableHwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
    controlId_ = 0;
    impl_->selectedRow = -1;
}

void Table::SetTheme(const Theme& theme) {
    theme_ = theme;
    impl_->UpdateThemeResources();
}

void Table::SetColumns(const std::vector<TableColumn>& columns) {
    impl_->columns = columns;
    if (tableHwnd_) {
        InvalidateRect(tableHwnd_, nullptr, TRUE);
    }
}

void Table::SetRows(const std::vector<TableRow>& rows) {
    impl_->rows = rows;
    if (impl_->selectedRow >= static_cast<int>(rows.size())) {
        impl_->selectedRow = -1;
    }
    if (tableHwnd_) {
        InvalidateRect(tableHwnd_, nullptr, TRUE);
    }
}

void Table::AddRow(const TableRow& row) {
    impl_->rows.push_back(row);
    if (tableHwnd_) {
        InvalidateRect(tableHwnd_, nullptr, TRUE);
    }
}

void Table::ClearRows() {
    impl_->rows.clear();
    impl_->selectedRow = -1;
    if (tableHwnd_) {
        InvalidateRect(tableHwnd_, nullptr, TRUE);
    }
}

void Table::SetDrawEmptyGrid(bool enabled) {
    drawEmptyGrid_ = enabled;
    if (tableHwnd_) {
        InvalidateRect(tableHwnd_, nullptr, TRUE);
    }
}

std::size_t Table::GetRowCount() const {
    return impl_->rows.size();
}

std::size_t Table::GetColumnCount() const {
    return impl_->columns.size();
}

}  // namespace darkui
