// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_TABLE_VIEW_H_
#define CHROME_VIEWS_TABLE_VIEW_H_

#include <windows.h>

#include <map>
#include <unicode/coll.h>
#include <unicode/uchar.h>
#include <vector>

#include "base/logging.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/native_control.h"
#include "SkColor.h"

class SkBitmap;

// A TableView is a view that displays multiple rows with any number of columns.
// TableView is driven by a TableModel. The model returns the contents
// to display. TableModel also has an Observer which is used to notify
// TableView of changes to the model so that the display may be updated
// appropriately.
//
// TableView itself has an observer that is notified when the selection
// changes.
//
// Tables may be sorted either by directly invoking SetSortDescriptors or by
// marking the column as sortable and the user doing a gesture to sort the
// contents. TableView itself maintains the sort so that the underlying model
// isn't effected.
//
// When a table is sorted the model coordinates do not necessarily match the
// view coordinates. All table methods are in terms of the model. If you need to
// convert to view coordinates use model_to_view.
//
// Sorting is done by a locale sensitive string sort. You can customize the
// sort by way of overriding CompareValues.
//
// TableView is a wrapper around the window type ListView in report mode.
namespace views {

class HWNDView;
class ListView;
class ListViewParent;
class TableView;
struct TableColumn;

// The cells in the first column of a table can contain:
// - only text
// - a small icon (16x16) and some text
// - a check box and some text
typedef enum TableTypes {
  TEXT_ONLY = 0,
  ICON_AND_TEXT,
  CHECK_BOX_AND_TEXT
};

// Any time the TableModel changes, it must notify its observer.
class TableModelObserver {
 public:
  // Invoked when the model has been completely changed.
  virtual void OnModelChanged() = 0;

  // Invoked when a range of items has changed.
  virtual void OnItemsChanged(int start, int length) = 0;

  // Invoked when new items are added.
  virtual void OnItemsAdded(int start, int length) = 0;

  // Invoked when a range of items has been removed.
  virtual void OnItemsRemoved(int start, int length) = 0;
};

// The model driving the TableView.
class TableModel {
 public:
  // See HasGroups, get GetGroupID for details as to how this is used.
  struct Group {
    // The title text for the group.
    std::wstring title;

    // Unique id for the group.
    int id;
  };
  typedef std::vector<Group> Groups;

  // Number of rows in the model.
  virtual int RowCount() = 0;

  // Returns the value at a particular location in text.
  virtual std::wstring GetText(int row, int column_id) = 0;

  // Returns the small icon (16x16) that should be displayed in the first
  // column before the text. This is only used when the TableView was created
  // with the ICON_AND_TEXT table type. Returns an isNull() bitmap if there is
  // no bitmap.
  virtual SkBitmap GetIcon(int row);

  // Sets whether a particular row is checked. This is only invoked
  // if the TableView was created with show_check_in_first_column true.
  virtual void SetChecked(int row, bool is_checked) {
    NOTREACHED();
  }

  // Returns whether a particular row is checked. This is only invoked
  // if the TableView was created with show_check_in_first_column true.
  virtual bool IsChecked(int row) {
    return false;
  }

  // Returns true if the TableView has groups. Groups provide a way to visually
  // delineate the rows in a table view. When groups are enabled table view
  // shows a visual separator for each group, followed by all the rows in
  // the group.
  //
  // On win2k a visual separator is not rendered for the group headers.
  virtual bool HasGroups() { return false; }

  // Returns the groups.
  // This is only used if HasGroups returns true.
  virtual Groups GetGroups() {
    // If you override HasGroups to return true, you must override this as
    // well.
    NOTREACHED();
    return std::vector<Group>();
  }

  // Returns the group id of the specified row.
  // This is only used if HasGroups returns true.
  virtual int GetGroupID(int row) {
    // If you override HasGroups to return true, you must override this as
    // well.
    NOTREACHED();
    return 0;
  }

  // Sets the observer for the model. The TableView should NOT take ownership
  // of the observer.
  virtual void SetObserver(TableModelObserver* observer) = 0;

  // Compares the values in the column with id |column_id| for the two rows.
  // Returns a value < 0, == 0 or > 0 as to whether the first value is
  // <, == or > the second value.
  //
  // This implementation does a case insensitive locale specific string
  // comparison.
  virtual int CompareValues(int row1, int row2, int column_id);

 protected:
  // Returns the collator used by CompareValues.
  Collator* GetCollator();
};

// TableColumn specifies the title, alignment and size of a particular column.
struct TableColumn {
  typedef enum Alignment {
    LEFT, RIGHT, CENTER
  };

  TableColumn()
      : id(0),
        title(),
        alignment(LEFT),
        width(-1),
        percent(),
        min_visible_width(0),
        sortable(false) {
  }
  TableColumn(int id, const std::wstring title, Alignment alignment, int width)
      : id(id),
        title(title),
        alignment(alignment),
        width(width),
        percent(0),
        min_visible_width(0),
        sortable(false) {
  }
  TableColumn(int id, const std::wstring title, Alignment alignment, int width,
              float percent)
      : id(id),
        title(title),
        alignment(alignment),
        width(width),
        percent(percent),
        min_visible_width(0),
        sortable(false) {
  }
  // It's common (but not required) to use the title's IDS_* tag as the column
  // id. In this case, the provided conveniences look up the title string on
  // bahalf of the caller.
  TableColumn(int id, Alignment alignment, int width)
      : id(id),
        alignment(alignment),
        width(width),
        percent(0),
        min_visible_width(0),
        sortable(false) {
    title = l10n_util::GetString(id);
  }
  TableColumn(int id, Alignment alignment, int width, float percent)
      : id(id),
        alignment(alignment),
        width(width),
        percent(percent),
        min_visible_width(0),
        sortable(false) {
    title = l10n_util::GetString(id);
  }

  // A unique identifier for the column.
  int id;

  // The title for the column.
  std::wstring title;

  // Alignment for the content.
  Alignment alignment;

  // The size of a column may be specified in two ways:
  // 1. A fixed width. Set the width field to a positive number and the
  //    column will be given that width, in pixels.
  // 2. As a percentage of the available width. If width is -1, and percent is
  //    > 0, the column is given a width of
  //    available_width * percent / total_percent.
  // 3. If the width == -1 and percent == 0, the column is autosized based on
  //    the width of the column header text.
  //
  // Sizing is done in four passes. Fixed width columns are given
  // their width, percentages are applied, autosized columns are autosized,
  // and finally percentages are applied again taking into account the widths
  // of autosized columns.
  int width;
  float percent;

  // The minimum width required for all items in this column
  // (including the header)
  // to be visible.
  int min_visible_width;

  // Is this column sortable? Default is false
  bool sortable;
};

// Returned from SelectionBegin/SelectionEnd
class TableSelectionIterator {
 public:
  TableSelectionIterator(TableView* view, int view_index);
  TableSelectionIterator& operator=(const TableSelectionIterator& other);
  bool operator==(const TableSelectionIterator& other);
  bool operator!=(const TableSelectionIterator& other);
  TableSelectionIterator& operator++();
  int operator*();

 private:
  void UpdateModelIndexFromViewIndex();

  TableView* table_view_;
  int view_index_;

  // The index in terms of the model. This is returned from the * operator. This
  // is cached to avoid dependencies on the view_to_model mapping.
  int model_index_;
};

// TableViewObserver is notified about the TableView selection.
class TableViewObserver {
 public:
  virtual ~TableViewObserver() {}

  // Invoked when the selection changes.
  virtual void OnSelectionChanged() = 0;

  // Optional method invoked when the user double clicks on the table.
  virtual void OnDoubleClick() {}

  // Optional method invoked when the user hits a key with the table in focus.
  virtual void OnKeyDown(unsigned short virtual_keycode) {}

  // Invoked when the user presses the delete key.
  virtual void OnTableViewDelete(TableView* table_view) {}
};

class TableView : public NativeControl,
                  public TableModelObserver {
 public:
  typedef TableSelectionIterator iterator;

  // A helper struct for GetCellColors. Set |color_is_set| to true if color is
  // set.  See OnCustomDraw for more details on why we need this.
  struct ItemColor {
    bool color_is_set;
    SkColor color;
  };

  // Describes a sorted column.
  struct SortDescriptor {
    SortDescriptor() : column_id(-1), ascending(true) {}
    SortDescriptor(int column_id, bool ascending)
        : column_id(column_id),
          ascending(ascending) { }

    // ID of the sorted column.
    int column_id;

    // Is the sort ascending?
    bool ascending;
  };

  typedef std::vector<SortDescriptor> SortDescriptors;

  // Creates a new table using the model and columns specified.
  // The table type applies to the content of the first column (text, icon and
  // text, checkbox and text).
  // When autosize_columns is true, columns always fill the available width. If
  // false, columns are not resized when the table is resized. An extra empty
  // column at the right fills the remaining space.
  // When resizable_columns is true, users can resize columns by dragging the
  // separator on the column header.  NOTE: Right now this is always true.  The
  // code to set it false is still in place to be a base for future, better
  // resizing behavior (see http://b/issue?id=874646 ), but no one uses or
  // tests the case where this flag is false.
  // Note that setting both resizable_columns and autosize_columns to false is
  // probably not a good idea, as there is no way for the user to increase a
  // column's size in that case.
  TableView(TableModel* model, const std::vector<TableColumn>& columns,
            TableTypes table_type, bool single_selection,
            bool resizable_columns, bool autosize_columns);
  virtual ~TableView();

  // Assigns a new model to the table view, detaching the old one if present.
  // If |model| is NULL, the table view cannot be used after this call. This
  // should be called in the containing view's destructor to avoid destruction
  // issues when the model needs to be deleted before the table.
  void SetModel(TableModel* model);
  TableModel* model() const { return model_; }

  // Resorts the contents.
  void SetSortDescriptors(const SortDescriptors& sort_descriptors);

  // Current sort.
  const SortDescriptors& sort_descriptors() const { return sort_descriptors_; }

  void DidChangeBounds(const gfx::Rect& previous,
                       const gfx::Rect& current);

  // Returns the number of rows in the TableView.
  int RowCount();

  // Returns the number of selected rows.
  int SelectedRowCount();

  // Selects the specified item, making sure it's visible.
  void Select(int model_row);

  // Sets the selected state of an item (without sending any selection
  // notifications). Note that this routine does NOT set the focus to the
  // item at the given index.
  void SetSelectedState(int model_row, bool state);

  // Sets the focus to the item at the given index.
  void SetFocusOnItem(int model_row);

  // Returns the first selected row in terms of the model.
  int FirstSelectedRow();

  // Returns true if the item at the specified index is selected.
  bool IsItemSelected(int model_row);

  // Returns true if the item at the specified index has the focus.
  bool ItemHasTheFocus(int model_row);

  // Returns an iterator over the selection. The iterator proceeds from the
  // last index to the first.
  //
  // NOTE: the iterator iterates over the visual order (but returns coordinates
  // in terms of the model).
  iterator SelectionBegin();
  iterator SelectionEnd();

  // TableModelObserver methods.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

  void SetObserver(TableViewObserver* observer) {
    table_view_observer_ = observer;
  }
  TableViewObserver* observer() const { return table_view_observer_; }

  // Replaces the set of known columns without changing the current visible
  // columns.
  void SetColumns(const std::vector<TableColumn>& columns);
  void AddColumn(const TableColumn& col);
  bool HasColumn(int id);

  // Sets which columns (by id) are displayed.  All transient size and position
  // information is lost.
  void SetVisibleColumns(const std::vector<int>& columns);
  void SetColumnVisibility(int id, bool is_visible);
  bool IsColumnVisible(int id) const;

  // Resets the size of the columns based on the sizes passed to the
  // constructor. Your normally needn't invoked this, it's done for you the
  // first time the TableView is given a valid size.
  void ResetColumnSizes();

  // Sometimes we may want to size the TableView to a specific width and
  // height.
  virtual gfx::Size GetPreferredSize();
  void set_preferred_size(const gfx::Size& size) { preferred_size_ = size; }

  // Is the table sorted?
  bool is_sorted() const { return !sort_descriptors_.empty(); }

  // Maps from the index in terms of the model to that of the view.
  int model_to_view(int model_index) const {
    return model_to_view_.get() ? model_to_view_[model_index] : model_index;
  }

  // Maps from the index in terms of the view to that of the model.
  int view_to_model(int view_index) const {
    return view_to_model_.get() ? view_to_model_[view_index] : view_index;
  }

 protected:
  // Overriden to return the position of the first selected row.
  virtual gfx::Point GetKeyboardContextMenuLocation();

  // Subclasses that want to customize the colors of a particular row/column,
  // must invoke this passing in true. The default value is false, such that
  // GetCellColors is never invoked.
  void SetCustomColorsEnabled(bool custom_colors_enabled);

  // Notification from the ListView that the selected state of an item has
  // changed.
  virtual void OnSelectedStateChanged();

  // Notification from the ListView that the used double clicked the table.
  virtual void OnDoubleClick();

  // Subclasses can implement this method if they need to be notified of a key
  // press event.  Other wise, it appeals to table_view_observer_
  virtual void OnKeyDown(unsigned short virtual_keycode);

  // Invoked to customize the colors or font at a particular cell. If you
  // change the colors or font, return true. This is only invoked if
  // SetCustomColorsEnabled(true) has been invoked.
  virtual bool GetCellColors(int model_row,
                             int column,
                             ItemColor* foreground,
                             ItemColor* background,
                             LOGFONT* logfont);

  // Subclasses that want to perform some custom painting (on top of the regular
  // list view painting) should return true here and implement the PostPaint
  // method.
  virtual bool ImplementPostPaint() { return false; }
  // Subclasses can implement in this method extra-painting for cells.
  virtual void PostPaint(int model_row, int column, bool selected,
                         const CRect& bounds, HDC device_context) { }
  virtual void PostPaint() {}

  virtual HWND CreateNativeControl(HWND parent_container);

  virtual LRESULT OnNotify(int w_param, LPNMHDR l_param);

  // Overriden to destroy the image list.
  virtual void OnDestroy();

  // Used to sort the two rows. Returns a value < 0, == 0 or > 0 indicating
  // whether the row2 comes before row1, row2 is the same as row1 or row1 comes
  // after row2. This invokes CompareValues on the model with the sorted column.
  virtual int CompareRows(int model_row1, int model_row2);

  // Called before sorting. This does nothing and is intended for subclasses
  // that need to cache state used during sorting.
  virtual void PrepareForSort() {}

  // Returns the width of the specified column by id, or -1 if the column isn't
  // visible.
  int GetColumnWidth(int column_id);

  // Returns the offset from the top of the client area to the start of the
  // content.
  int content_offset() const { return content_offset_; }

  // Size (width and height) of images.
  static const int kImageSize;

 private:
  // Direction of a sort.
  enum SortDirection {
    ASCENDING_SORT,
    DESCENDING_SORT,
    NO_SORT
  };

  // We need this wrapper to pass the table view to the windows proc handler
  // when subclassing the list view and list view header, as the reinterpret
  // cast from GetWindowLongPtr would break the pointer if it is pointing to a
  // subclass (in the OO sense of TableView).
  struct TableViewWrapper {
    explicit TableViewWrapper(TableView* view) : table_view(view) { }
    TableView* table_view;
  };

  friend class ListViewParent;
  friend class TableSelectionIterator;

  LRESULT OnCustomDraw(NMLVCUSTOMDRAW* draw_info);

  // Invoked when the user clicks on a column to toggle the sort order. If
  // column_id is the primary sorted column the direction of the sort is
  // toggled, otherwise column_id is made the primary sorted column.
  void ToggleSortOrder(int column_id);

  // Updates the lparam of each of the list view items to be the model index.
  // If length is > 0, all items with an index >= start get offset by length.
  // This is used during sorting to determine how the items were sorted.
  void UpdateItemsLParams(int start, int length);

  // Does the actual sort and updates the mappings (view_to_model and
  // model_to_view) appropriately.
  void SortItemsAndUpdateMapping();

  // Method invoked by ListView to compare the two values. Invokes CompareRows.
  static int CALLBACK SortFunc(LPARAM model_index_1_p,
                               LPARAM model_index_2_p,
                               LPARAM table_view_param);

  // Method invoked by ListView when sorting back to natural state. Returns
  // model_index_1_p - model_index_2_p.
  static int CALLBACK NaturalSortFunc(LPARAM model_index_1_p,
                                      LPARAM model_index_2_p,
                                      LPARAM table_view_param);

  // Resets the sort image displayed for the specified column.
  void ResetColumnSortImage(int column_id, SortDirection direction);

  // Adds a new column.
  void InsertColumn(const TableColumn& tc, int index);

  // Update headers and internal state after columns have changed
  void OnColumnsChanged();

  // Updates the ListView with values from the model. See UpdateListViewCache0
  // for a complete description.
  // This turns off redrawing, and invokes UpdateListViewCache0 to do the
  // actual updating.
  void UpdateListViewCache(int start, int length, bool add);

  // Updates ListView with values from the model.
  // If add is true, this adds length items starting at index start.
  // If add is not true, the items are not added, the but the values in the
  // range start - [start + length] are updated from the model.
  void UpdateListViewCache0(int start, int length, bool add);

  // Notification from the ListView that the checked state of the item has
  // changed.
  void OnCheckedStateChanged(int model_row, bool is_checked);

  // Returns the index of the selected item before |view_index|, or -1 if
  // |view_index| is the first selected item.
  //
  // WARNING: this returns coordinates in terms of the view, NOT the model.
  int PreviousSelectedViewIndex(int view_index);

  // Returns the last selected view index in the table view, or -1 if the table
  // is empty, or nothing is selected.
  //
  // WARNING: this returns coordinates in terms of the view, NOT the model.
  int LastSelectedViewIndex();

  // The TableColumn visible at position pos.
  const TableColumn& GetColumnAtPosition(int pos);

  // Window procedure of the list view class. We subclass the list view to
  // ignore WM_ERASEBKGND, which gives smoother painting during resizing.
  static LRESULT CALLBACK TableWndProc(HWND window,
                                       UINT message,
                                       WPARAM w_param,
                                       LPARAM l_param);

  // Window procedure of the header class. We subclass the header of the table
  // to disable resizing of columns.
  static LRESULT CALLBACK TableHeaderWndProc(HWND window, UINT message,
                                             WPARAM w_param, LPARAM l_param);

  // Updates content_offset_ from the position of the header.
  void UpdateContentOffset();

  TableModel* model_;
  TableTypes table_type_;
  TableViewObserver* table_view_observer_;

  // An ordered list of id's into all_columns_ representing current visible
  // columns.
  std::vector<int> visible_columns_;

  // Mapping of an int id to a TableColumn representing all possible columns.
  std::map<int, TableColumn> all_columns_;

  // Cached value of columns_.size()
  int column_count_;

  // Selection mode.
  bool single_selection_;

  // If true, any events that would normally be propagated to the observer
  // are ignored. For example, if this is true and the selection changes in
  // the listview, the observer is not notified.
  bool ignore_listview_change_;

  // Reflects the value passed to SetCustomColorsEnabled.
  bool custom_colors_enabled_;

  // Whether or not the columns have been sized in the ListView. This is
  // set to true the first time Layout() is invoked and we have a valid size.
  bool sized_columns_;

  // Whether or not columns should automatically be resized to fill the
  // the available width when the list view is resized.
  bool autosize_columns_;

  // Whether or not the user can resize columns.
  bool resizable_columns_;

  // NOTE: While this has the name View in it, it's not a view. Rather it's
  // a wrapper around the List-View window.
  HWND list_view_;

  // The list view's header original proc handler. It is required when
  // subclassing.
  WNDPROC header_original_handler_;

  // Window procedure of the listview before we subclassed it.
  WNDPROC original_handler_;

  // A wrapper around 'this' used when "subclassing" the list view and header.
  TableViewWrapper table_view_wrapper_;

  // A custom font we use when overriding the font type for a specific cell.
  HFONT custom_cell_font_;

  // The preferred size of the table view.
  gfx::Size preferred_size_;

  int content_offset_;

  // Current sort.
  SortDescriptors sort_descriptors_;

  // Mappings used when sorted.
  scoped_array<int> view_to_model_;
  scoped_array<int> model_to_view_;

  DISALLOW_COPY_AND_ASSIGN(TableView);
};

}  // namespace views

#endif  // CHROME_VIEWS_TABLE_VIEW_H_
