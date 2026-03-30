#pragma once

#include "darkui/combobox.h"

#include <commctrl.h>

#include <memory>
#include <string>
#include <vector>

namespace darkui {

// Describes a single table column.
struct TableColumn {
    // Column header text.
    std::wstring text;
    // Relative width weight used during layout.
    int width = 120;
    // Header and cell text alignment, for example LVCFMT_LEFT or LVCFMT_CENTER.
    int format = LVCFMT_LEFT;
};

// Represents one table row. Each string corresponds to one column cell.
using TableRow = std::vector<std::wstring>;

// Custom dark table control for Win32.
// Usage:
// - Create the control with Create().
// - Position it with MoveWindow().
// - Define columns with SetColumns().
// - Fill rows with SetRows() or AddRow().
class Table {
public:
    struct Impl;

    // Constructs an empty table wrapper.
    Table();
    // Destroys the underlying table window if it still exists.
    ~Table();

    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;

    // Creates the table as a child window.
    // Parameters:
    // - parent: Parent window that owns the control.
    // - controlId: Child control ID used for identification.
    // - theme: Visual theme used for drawing.
    // - style: Standard child-window style flags. Report-style defaults are provided.
    // - exStyle: Optional extended window style.
    // Returns:
    // - true on success.
    // - false if the window or drawing resources could not be created.
    // Notes:
    // - Position and size are controlled by your layout code.
    bool Create(HWND parent, int controlId, const Theme& theme = Theme{}, DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL, DWORD exStyle = 0);
    // Destroys the table window and resets wrapper state.
    void Destroy();

    // Returns the underlying HWND, or nullptr before Create().
    HWND hwnd() const { return tableHwnd_; }
    // Returns the parent HWND passed to Create().
    HWND parent() const { return parentHwnd_; }
    // Returns the child control ID passed to Create().
    int control_id() const { return controlId_; }
    // Returns the theme currently stored by the control.
    const Theme& theme() const { return theme_; }
    // Returns whether grid lines are drawn in the empty expanded area below the last row.
    bool draw_empty_grid() const { return drawEmptyGrid_; }

    // Replaces the current theme and repaints the table.
    // Parameter:
    // - theme: New theme data to apply.
    void SetTheme(const Theme& theme);
    // Replaces the entire column definition list.
    // Parameter:
    // - columns: Full set of column descriptors.
    // Notes:
    // - Column widths are treated as relative layout weights.
    void SetColumns(const std::vector<TableColumn>& columns);
    // Replaces the entire row data set.
    // Parameter:
    // - rows: Full set of rows to display.
    void SetRows(const std::vector<TableRow>& rows);
    // Appends a single row to the end of the table.
    // Parameter:
    // - row: Row data to append.
    void AddRow(const TableRow& row);
    // Removes all rows from the table body.
    void ClearRows();
    // Controls whether grid lines continue below the last data row.
    // Parameter:
    // - enabled: true to draw the grid in the empty expanded area, false to leave it plain.
    void SetDrawEmptyGrid(bool enabled);
    // Returns the number of data rows currently stored in the table.
    std::size_t GetRowCount() const;
    // Returns the number of columns currently stored in the table.
    std::size_t GetColumnCount() const;

private:
    // Internal implementation object.
    std::unique_ptr<Impl> impl_;
    // Parent window handle.
    HWND parentHwnd_ = nullptr;
    // Underlying table window handle.
    HWND tableHwnd_ = nullptr;
    // Child control ID.
    int controlId_ = 0;
    // Whether the empty expanded area should continue drawing grid lines.
    bool drawEmptyGrid_ = true;
    // Theme currently used by the control.
    Theme theme_{};
};

}  // namespace darkui
