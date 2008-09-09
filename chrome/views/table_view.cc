// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windowsx.h>

#include "chrome/views/table_view.h"

#include "base/string_util.h"
#include "base/win_util.h"
#include "base/gfx/skia_utils.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/favicon_size.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/hwnd_view.h"
#include "chrome/views/view_container.h"
#include "SkBitmap.h"
#include "SkColorFilter.h"

namespace ChromeViews {

// Added to column width to prevent truncation.
const int kListViewTextPadding = 15;
// Additional column width necessary if column has icons.
const int kListViewIconWidthAndPadding = 18;

SkBitmap TableModel::GetIcon(int row) {
  return SkBitmap();
}

TableView::TableView(TableModel* model,
                     const std::vector<TableColumn>& columns,
                     TableTypes table_type,
                     bool single_selection,
                     bool resizable_columns,
                     bool autosize_columns)
    : model_(model),
      table_view_observer_(NULL),
      visible_columns_(),
      all_columns_(),
      column_count_(static_cast<int>(columns.size())),
      cache_data_(true),
      table_type_(table_type),
      single_selection_(single_selection),
      ignore_listview_change_(false),
      custom_colors_enabled_(false),
      sized_columns_(false),
      autosize_columns_(autosize_columns),
      resizable_columns_(resizable_columns),
      list_view_(NULL),
      header_original_handler_(NULL),
      original_handler_(NULL),
      table_view_wrapper_(this),
      custom_cell_font_(NULL),
      content_offset_(0) {
  DCHECK(model);
  for (std::vector<TableColumn>::const_iterator i = columns.begin();
      i != columns.end(); ++i) {
    AddColumn(*i);
    visible_columns_.push_back(i->id);
  }
}

TableView::~TableView() {
  if (list_view_) {
    if (model_)
      model_->SetObserver(NULL);
  }
  if (custom_cell_font_)
    DeleteObject(custom_cell_font_);
}

void TableView::SetModel(TableModel* model) {
  model_ = model;
  if (model_)
    OnModelChanged();
}

void TableView::DidChangeBounds(const CRect& previous,
                                const CRect& current) {
  if (!list_view_)
    return;
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);
  Layout();
  if ((!sized_columns_ || autosize_columns_) && GetWidth() > 0) {
    sized_columns_ = true;
    ResetColumnSizes();
  }
  UpdateContentOffset();
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);
}

int TableView::RowCount() {
  if (!list_view_)
    return 0;
  return ListView_GetItemCount(list_view_);
}

int TableView::SelectedRowCount() {
  if (!list_view_)
    return 0;
  return ListView_GetSelectedCount(list_view_);
}

void TableView::Select(int item) {
  if (!list_view_)
    return;

  DCHECK(item >= 0 && item < RowCount());
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);
  ignore_listview_change_ = true;

  // Unselect everything.
  ListView_SetItemState(list_view_, -1, 0, LVIS_SELECTED);

  // Select the specified item.
  ListView_SetItemState(list_view_, item, LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED);

  // Make it visible.
  ListView_EnsureVisible(list_view_, item, FALSE);
  ignore_listview_change_ = false;
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);
  if (table_view_observer_)
    table_view_observer_->OnSelectionChanged();
}

void TableView::SetSelectedState(int item, bool state) {
  if (!list_view_)
    return;

  DCHECK(item >= 0 && item < RowCount());

  ignore_listview_change_ = true;

  // Select the specified item.
  ListView_SetItemState(list_view_, item, LVIS_SELECTED, LVIS_SELECTED);

  ignore_listview_change_ = false;
}

void TableView::SetFocusOnItem(int item) {
  if (!list_view_)
    return;

  DCHECK(item >= 0 && item < RowCount());

  ignore_listview_change_ = true;

  // Set the focus to the given item.
  ListView_SetItemState(list_view_, item, LVIS_FOCUSED, LVIS_FOCUSED);

  ignore_listview_change_ = false;
}

int TableView::FirstSelectedRow() {
  if (!list_view_)
    return -1;

  return ListView_GetNextItem(list_view_, -1, LVNI_ALL | LVIS_SELECTED);
}


bool TableView::IsItemSelected(int item) {
  if (!list_view_)
    return false;

  DCHECK(item >= 0 && item < RowCount());
  return (ListView_GetItemState(list_view_, item, LVIS_SELECTED) ==
              LVIS_SELECTED);
}

bool TableView::ItemHasTheFocus(int item) {
  if (!list_view_)
    return false;

  DCHECK(item >= 0 && item < RowCount());
  return (ListView_GetItemState(list_view_, item, LVIS_FOCUSED) ==
              LVIS_FOCUSED);
}

TableView::iterator TableView::SelectionBegin() {
  return TableView::iterator(this, LastSelectedIndex());
}

TableView::iterator TableView::SelectionEnd() {
  return TableView::iterator(this, -1);
}

void TableView::OnItemsChanged(int start, int length) {
  if (!list_view_)
    return;

  if (length == -1) {
    DCHECK(start >= 0);
    if (cache_data_) {
      length = model_->RowCount() - start;
    } else {
      length = RowCount() - start;
    }
  }
  int row_count = RowCount();
  DCHECK(start >= 0 && length > 0 && start + length <= row_count);
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);
  if (table_type_ == ICON_AND_TEXT) {
    // The redraw event does not include the icon in the clip rect, preventing
    // our icon from being repainted. So far the only way I could find around
    // this is to change the image for the item. Even if the image does not
    // exist, it causes the clip rect to include the icon's bounds so we can
    // paint it in the post paint event.
    LVITEM lv_item;
    memset(&lv_item, 0, sizeof(LVITEM));
    lv_item.mask = LVIF_IMAGE;
    for (int i = start; i < start + length; ++i) {
      // Retrieve the current icon index.
      lv_item.iItem = i;
      BOOL r = ListView_GetItem(list_view_, &lv_item);
      DCHECK(r);
      // Set the current icon index to the other image.
      lv_item.iImage = (lv_item.iImage + 1) % 2;
      DCHECK((lv_item.iImage == 0) || (lv_item.iImage == 1));
      r = ListView_SetItem(list_view_, &lv_item);
      DCHECK(r);
    }
  }
  if (!cache_data_) {
    ListView_RedrawItems(list_view_, start, start + length);
  } else {
    UpdateListViewCache(start, length, false);
  }
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);
}


void TableView::OnModelChanged() {
  if (!list_view_)
    return;

  int current_row_count = ListView_GetItemCount(list_view_);
  if (current_row_count > 0)
    OnItemsRemoved(0, current_row_count);
  if (model_->RowCount())
    OnItemsAdded(0, model_->RowCount());
}

void TableView::OnItemsAdded(int start, int length) {
  if (!list_view_)
    return;

  DCHECK(start >= 0 && length > 0 && start <= RowCount());
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);
  if (!cache_data_) {
    ListView_SetItemCount(list_view_, model_->RowCount());
    ListView_RedrawItems(list_view_, start, start + length);
  } else {
    UpdateListViewCache(start, length, true);
  }
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);
}

void TableView::OnItemsRemoved(int start, int length) {
  if (!list_view_)
    return;

  DCHECK(start >= 0 && length > 0 && start + length <= RowCount());
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(FALSE), 0);
  bool had_selection = (SelectedRowCount() > 0);
  if (!cache_data_) {
    // TODO(sky): Make sure this triggers a repaint.
    ListView_SetItemCount(list_view_, model_->RowCount());
  } else {
    // Update the cache.
    if (start == 0 && length == RowCount()) {
      ListView_DeleteAllItems(list_view_);
    } else {
      for (int i = 0; i < length; ++i) {
        ListView_DeleteItem(list_view_, start);
      }
    }
  }
  SendMessage(list_view_, WM_SETREDRAW, static_cast<WPARAM>(TRUE), 0);
  // We don't seem to get notification in this case.
  if (table_view_observer_ && had_selection && RowCount() == 0)
    table_view_observer_->OnSelectionChanged();
}

void TableView::AddColumn(const TableColumn& col) {
  DCHECK(all_columns_.count(col.id) == 0);
  all_columns_[col.id] = col;
}

void TableView::SetColumns(const std::vector<TableColumn>& columns) {
  all_columns_.empty();
  for (std::vector<TableColumn>::const_iterator i = columns.begin();
       i != columns.end(); ++i) {
    AddColumn(*i);
  }
}

void TableView::OnColumnsChanged() {
  column_count_ = static_cast<int>(visible_columns_.size());
  ResetColumnSizes();
}

void TableView::SetColumnVisibility(int id, bool is_visible) {
  bool changed = false;
  for (std::vector<int>::iterator i = visible_columns_.begin();
       i != visible_columns_.end(); ++i) {
    if (*i == id) {
      if (is_visible) {
        // It's already visible, bail out early.
        return;
      } else {
        int index = static_cast<int>(i - visible_columns_.begin());
        // This could be called before the native list view has been created
        // (in CreateNativeControl, called when the view is added to a
        // container). In that case since the column is not in
        // visible_columns_ it will not be added later on when it is created.
        if (list_view_)
          SendMessage(list_view_, LVM_DELETECOLUMN, index, 0);
        visible_columns_.erase(i);
        changed = true;
        break;
      }
    }
  }
  if (is_visible) {
    visible_columns_.push_back(id);
    TableColumn& column = all_columns_[id];
    InsertColumn(column, column_count_);
    if (column.min_visible_width == 0) {
      // ListView_GetStringWidth must be padded or else truncation will occur.
      column.min_visible_width = ListView_GetStringWidth(list_view_,
                                                         column.title.c_str()) +
                                 kListViewTextPadding;
    }
    changed = true;
  }
  if (changed)
    OnColumnsChanged();
}

void TableView::SetVisibleColumns(const std::vector<int>& columns) {
  size_t old_count = visible_columns_.size();
  size_t new_count = columns.size();
  // remove the old columns
  if (list_view_) {
    for (std::vector<int>::reverse_iterator i = visible_columns_.rbegin();
         i != visible_columns_.rend(); ++i) {
      int index = static_cast<int>(i - visible_columns_.rend());
      SendMessage(list_view_, LVM_DELETECOLUMN, index, 0);
    }
  }
  visible_columns_ = columns;
  // Insert the new columns.
  if (list_view_) {
    for (std::vector<int>::iterator i = visible_columns_.begin();
         i != visible_columns_.end(); ++i) {
      int index = static_cast<int>(i - visible_columns_.end());
      InsertColumn(all_columns_[*i], index);
    }
  }
  OnColumnsChanged();
}

bool TableView::IsColumnVisible(int id) const {
  for (std::vector<int>::const_iterator i = visible_columns_.begin();
       i != visible_columns_.end(); ++i)
    if (*i == id) {
      return true;
    }
  return false;
}

const TableColumn& TableView::GetColumnAtPosition(int pos) {
  return all_columns_[visible_columns_[pos]];
}

bool TableView::HasColumn(int id) {
  return all_columns_.count(id) > 0;
}

void TableView::SetCustomColorsEnabled(bool custom_colors_enabled) {
  custom_colors_enabled_ = custom_colors_enabled;
}

bool TableView::GetCellColors(int row,
                              int column,
                              ItemColor* foreground,
                              ItemColor* background,
                              LOGFONT* logfont) {
  return false;
}

LRESULT CALLBACK TableView::TableWndProc(HWND window,
                                         UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  TableView* table_view = reinterpret_cast<TableViewWrapper*>(
      GetWindowLongPtr(window, GWLP_USERDATA))->table_view;

  switch (message) {
    case WM_ERASEBKGND:
      // We make WM_ERASEBKGND do nothing (returning 1 indicates we handled
      // the request). We do this so that the table view doesn't flicker during
      // resizing.
      return 1;

    default:
      break;
  }
  DCHECK(table_view->original_handler_);
  return CallWindowProc(table_view->original_handler_, window, message, w_param,
                        l_param);
}

LRESULT CALLBACK TableView::TableHeaderWndProc(HWND window, UINT message,
                                               WPARAM w_param, LPARAM l_param) {
  TableView* table_view = reinterpret_cast<TableViewWrapper*>(
      GetWindowLongPtr(window, GWLP_USERDATA))->table_view;

  switch (message) {
    case WM_SETCURSOR:
      if (!table_view->resizable_columns_)
        // Prevents the cursor from changing to the resize cursor.
        return TRUE;
      break;
    case WM_LBUTTONDBLCLK:
      if (!table_view->resizable_columns_)
        // Prevents the double-click on the column separator from auto-resizing
        // the column.
        return TRUE;
      break;
    default:
      break;
  }
  DCHECK(table_view->header_original_handler_);
  return CallWindowProc(table_view->header_original_handler_,
                        window, message, w_param, l_param);
}

HWND TableView::CreateNativeControl(HWND parent_container) {
  int style = WS_CHILD | LVS_REPORT | LVS_SHOWSELALWAYS;
  if (single_selection_)
    style |= LVS_SINGLESEL;
  if (!cache_data_)
    style |= LVS_OWNERDATA;
  // If there's only one column and the title string is empty, don't show a
  // header.
  if (all_columns_.size() == 1) {
    std::map<int, ChromeViews::TableColumn>::const_iterator first =
        all_columns_.begin();
    if (first->second.title.empty())
      style |= LVS_NOCOLUMNHEADER;
  }
  list_view_ = ::CreateWindowEx(WS_EX_CLIENTEDGE | GetAdditionalExStyle(),
                                WC_LISTVIEW,
                                L"",
                                style,
                                0, 0, GetWidth(), GetHeight(),
                                parent_container, NULL, NULL, NULL);
  model_->SetObserver(this);

  // Make the selection extend across the row.
  // Reduce overdraw/flicker artifacts by double buffering.
  DWORD list_view_style = LVS_EX_FULLROWSELECT;
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    list_view_style |= LVS_EX_DOUBLEBUFFER;
  }
  if (table_type_ == CHECK_BOX_AND_TEXT)
    list_view_style |= LVS_EX_CHECKBOXES;
  ListView_SetExtendedListViewStyleEx(list_view_, 0, list_view_style);

  // Add the columns.
  for (std::vector<int>::iterator i = visible_columns_.begin();
       i != visible_columns_.end(); ++i) {
    InsertColumn(all_columns_[*i],
                 static_cast<int>(i - visible_columns_.begin()));
  }

  // Add the groups.
  if (model_->HasGroups() &&
      win_util::GetWinVersion() > win_util::WINVERSION_2000) {
    ListView_EnableGroupView(list_view_, true);

    TableModel::Groups groups = model_->GetGroups();
    LVGROUP group = { 0 };
    group.cbSize = sizeof(LVGROUP);
    group.mask = LVGF_HEADER | LVGF_ALIGN | LVGF_GROUPID;
    group.uAlign = LVGA_HEADER_LEFT;
    for (size_t i = 0; i < groups.size(); ++i) {
      group.pszHeader = const_cast<wchar_t*>(groups[i].title.c_str());
      group.iGroupId = groups[i].id;
      ListView_InsertGroup(list_view_, static_cast<int>(i), &group);
    }
  }

  // Set the # of rows.
  if (cache_data_) {
    UpdateListViewCache(0, model_->RowCount(), true);
  } else if (table_type_ == CHECK_BOX_AND_TEXT) {
    ListView_SetItemCount(list_view_, model_->RowCount());
    ListView_SetCallbackMask(list_view_, LVIS_STATEIMAGEMASK);
  }

  // Load the default icon.
  if (table_type_ == ICON_AND_TEXT) {
    HIMAGELIST image_list = ImageList_Create(18, 18, ILC_COLOR, 1, 1);
    // TODO(jcampan): include a default icon image.
    // This icon will not be found. The list view will still layout with the
    // right space for the icon so we can paint our own icon.
    HBITMAP image = LoadBitmap(NULL, L"IDR_WHATEVER");
    ImageList_Add(image_list, image, NULL);
    DeleteObject(image);
    // We create 2 phony images because we are going to switch images at every
    // refresh in order to force a refresh of the icon area (somehow the clip
    // rect does not include the icon).
    image = LoadBitmap(NULL, L"IDR_WHATEVER_AGAIN");
    ImageList_Add(image_list, image, NULL);
    DeleteObject(image);
    ListView_SetImageList(list_view_, image_list, LVSIL_SMALL);
  }

  if (!resizable_columns_) {
    // To disable the resizing of columns we'll filter the events happening on
    // the header. We also need to intercept the HDM_LAYOUT to size the header
    // for the Chrome headers.
    HWND header = ListView_GetHeader(list_view_);
    DCHECK(header);
    SetWindowLongPtr(header, GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(&table_view_wrapper_));
    header_original_handler_ = win_util::SetWindowProc(header,
        &TableView::TableHeaderWndProc);
  }

  SetWindowLongPtr(list_view_, GWLP_USERDATA,
      reinterpret_cast<LONG_PTR>(&table_view_wrapper_));
  original_handler_ =
      win_util::SetWindowProc(list_view_, &TableView::TableWndProc);

  // Bug 964884: detach the IME attached to this window.
  // We should attach IMEs only when we need to input CJK strings.
  ::ImmAssociateContextEx(list_view_, NULL, 0);

  UpdateContentOffset();

  return list_view_;
}

void TableView::InsertColumn(const TableColumn& tc, int index) {
  if (!list_view_)
    return;

  LVCOLUMN column = { 0 };
  column.mask = LVCF_TEXT|LVCF_FMT;
  column.pszText = const_cast<LPWSTR>(tc.title.c_str());
  switch (tc.alignment) {
      case TableColumn::LEFT:
        column.fmt = LVCFMT_LEFT;
        break;
      case TableColumn::RIGHT:
        column.fmt = LVCFMT_RIGHT;
        break;
      case TableColumn::CENTER:
        column.fmt = LVCFMT_CENTER;
        break;
      default:
        NOTREACHED();
  }
  if (tc.width != -1) {
    column.mask |= LVCF_WIDTH;
    column.cx = tc.width;
  }
  column.mask |= LVCF_SUBITEM;
  // Sub-items are 1s indexed.
  column.iSubItem = index + 1;
  SendMessage(list_view_, LVM_INSERTCOLUMN, index,
              reinterpret_cast<LPARAM>(&column));
}

LRESULT TableView::OnNotify(int w_param, NMHDR* hdr) {
  switch (hdr->code) {
    case NM_CUSTOMDRAW: {
      // Draw notification. dwDragState indicates the current stage of drawing.
      return OnCustomDraw(reinterpret_cast<NMLVCUSTOMDRAW*>(hdr));
    }
    case LVN_ITEMCHANGED: {
      // Notification that the state of an item has changed. The state
      // includes such things as whether the item is selected or checked.
      NMLISTVIEW* state_change = reinterpret_cast<NMLISTVIEW*>(hdr);
      if ((state_change->uChanged & LVIF_STATE) != 0) {
        if ((state_change->uOldState & LVIS_SELECTED) !=
            (state_change->uNewState & LVIS_SELECTED)) {
          // Selected state of the item changed.
          bool is_selected = ((state_change->uNewState & LVIS_SELECTED) != 0);
          OnSelectedStateChanged(state_change->iItem, is_selected);
        }
        if ((state_change->uOldState & LVIS_STATEIMAGEMASK) !=
            (state_change->uNewState & LVIS_STATEIMAGEMASK)) {
          // Checked state of the item changed.
          bool is_checked =
            ((state_change->uNewState & LVIS_STATEIMAGEMASK) ==
             INDEXTOSTATEIMAGEMASK(2));
          OnCheckedStateChanged(state_change->iItem, is_checked);
        }
      }
      break;
    }
    case HDN_BEGINTRACKW:
    case HDN_BEGINTRACKA:
      // Prevent clicks so columns cannot be resized.
      if (!resizable_columns_)
        return TRUE;
      break;
    case NM_DBLCLK:
      OnDoubleClick();
      break;

    // If we see a key down message, we need to invoke the OnKeyDown handler
    // in order to give our class (or any subclass) and opportunity to perform
    // a key down triggered action, if such action is necessary.
    case LVN_KEYDOWN: {
      NMLVKEYDOWN* key_down_message = reinterpret_cast<NMLVKEYDOWN*>(hdr);
      OnKeyDown(key_down_message->wVKey);
      break;
    }

    default:
      break;
  }
  return 0;
}

void TableView::OnDestroy() {
  if (table_type_ == ICON_AND_TEXT) {
    HIMAGELIST image_list =
      ListView_GetImageList(GetNativeControlHWND(), LVSIL_SMALL);
    DCHECK(image_list);
    if (image_list)
      ImageList_Destroy(image_list);
  }
}

LRESULT TableView::OnCustomDraw(NMLVCUSTOMDRAW* draw_info) {
  switch (draw_info->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: {
      return CDRF_NOTIFYITEMDRAW;
    }
    case CDDS_ITEMPREPAINT: {
      // The list-view is about to paint an item, tell it we want to
      // notified when it paints every subitem.
      LRESULT r = CDRF_NOTIFYSUBITEMDRAW;
      if (table_type_ == ICON_AND_TEXT)
        r |= CDRF_NOTIFYPOSTPAINT;
      return r;
    }
    case (CDDS_ITEMPREPAINT | CDDS_SUBITEM): {
      // The list-view is painting a subitem. See if the colors should be
      // changed from the default.
      if (custom_colors_enabled_) {
        // At this time, draw_info->clrText and draw_info->clrTextBk are not
        // set.  So we pass in an ItemColor to GetCellColors.  If
        // ItemColor.color_is_set is true, then we use the provided color.
        ItemColor foreground = {0};
        ItemColor background = {0};

        LOGFONT logfont;
        GetObject(GetWindowFont(list_view_), sizeof(logfont), &logfont);

        if (GetCellColors(static_cast<int>(draw_info->nmcd.dwItemSpec),
                          draw_info->iSubItem,
                          &foreground,
                          &background,
                          &logfont)) {
          // TODO(tc): Creating/deleting a font for every cell seems like a
          // waste if the font hasn't changed.  Maybe we should use a struct
          // with a bool like we do with colors?
          if (custom_cell_font_)
            DeleteObject(custom_cell_font_);
          custom_cell_font_ = CreateFontIndirect(&logfont);
          SelectObject(draw_info->nmcd.hdc, custom_cell_font_);
          draw_info->clrText = foreground.color_is_set
                               ? gfx::SkColorToCOLORREF(foreground.color)
                               : CLR_DEFAULT;
          draw_info->clrTextBk = background.color_is_set
                                 ? gfx::SkColorToCOLORREF(background.color)
                                 : CLR_DEFAULT;
          return CDRF_NEWFONT;
        }
      }
      return CDRF_DODEFAULT;
    }
    case CDDS_ITEMPOSTPAINT: {
      DCHECK((table_type_ == ICON_AND_TEXT) || (ImplementPostPaint()));
      int n_item = static_cast<int>(draw_info->nmcd.dwItemSpec);
      // We get notifications for empty items, just ignore them.
      if (n_item >= model_->RowCount()) {
        return CDRF_DODEFAULT;
      }
      LRESULT r = CDRF_DODEFAULT;
      // First let's take care of painting the right icon.
      if (table_type_ == ICON_AND_TEXT) {
        SkBitmap image = model_->GetIcon(n_item);
        if (!image.isNull()) {
          // Get the rect that holds the icon.
          CRect icon_rect, client_rect;
          if (ListView_GetItemRect(list_view_, n_item, &icon_rect,
                                   LVIR_ICON) &&
              GetClientRect(list_view_, &client_rect)) {
            CRect intersection;
            // Client rect includes the header but we need to make sure we don't
            // paint into it.
            client_rect.top += content_offset_;
            // Make sure the region need to paint is visible.
            if (intersection.IntersectRect(&icon_rect, &client_rect)) {
              ChromeCanvas canvas(icon_rect.Width(), icon_rect.Height(), false);

              // It seems the state in nmcd.uItemState is not correct.
              // We'll retrieve it explicitly.
              int selected = ListView_GetItemState(list_view_, n_item,
                                                   LVIS_SELECTED);
              int bg_color_index;
              if (!IsEnabled())
                bg_color_index = COLOR_3DFACE;
              else if (selected)
                bg_color_index = HasFocus() ? COLOR_HIGHLIGHT : COLOR_3DFACE;
              else
                bg_color_index = COLOR_WINDOW;
              // NOTE: This may be invoked without the ListView filling in the
              // background (or rather windows paints background, then invokes
              // this twice). As such, we always fill in the background.
              canvas.drawColor(
                  gfx::COLORREFToSkColor(GetSysColor(bg_color_index)),
                  SkPorterDuff::kSrc_Mode);
              // + 1 for padding (we declared the image as 18x18 in the list-
              // view when they are 16x16 so we get an extra pixel of padding).
              canvas.DrawBitmapInt(image, 0, 0,
                                   image.width(), image.height(),
                                   1, 1, kFavIconSize, kFavIconSize, true);

              // Only paint the visible region of the icon.
              RECT to_draw = { intersection.left - icon_rect.left,
                               intersection.top - icon_rect.top,
                               0, 0 };
              to_draw.right = to_draw.left +
                              (intersection.right - intersection.left);
              to_draw.bottom = to_draw.top +
                              (intersection.bottom - intersection.top);
              canvas.getTopPlatformDevice().drawToHDC(draw_info->nmcd.hdc,
                                                      intersection.left,
                                                      intersection.top,
                                                      &to_draw);
              r = CDRF_SKIPDEFAULT;
            }
          }
        }
      }
      if (ImplementPostPaint()) {
        CRect cell_rect;
        if (ListView_GetItemRect(list_view_, n_item, &cell_rect, LVIR_BOUNDS)) {
          PostPaint(n_item, 0, false, cell_rect, draw_info->nmcd.hdc);
          r = CDRF_SKIPDEFAULT;
        }
      }
      return r;
    }
    default:
      return CDRF_DODEFAULT;
  }
}

void TableView::UpdateListViewCache(int start, int length, bool add) {
  ignore_listview_change_ = true;
  UpdateListViewCache0(start, length, add);
  ignore_listview_change_ = false;
}

void TableView::ResetColumnSizes() {
  if (!list_view_)
    return;

  // See comment in TableColumn for what this does.
  CRect bounds;
  GetLocalBounds(&bounds, false);  // false so it doesn't include the border.
  int width = bounds.Width();
  if (GetClientRect(GetNativeControlHWND(), &bounds) &&
      bounds.Width() > 0) {
    // Prefer the bounds of the window over our bounds, which may be different.
    width = bounds.Width();
  }

  float percent = 0;
  int fixed_width = 0;
  int autosize_width = 0;

  for (std::vector<int>::const_iterator i = visible_columns_.begin();
       i != visible_columns_.end(); ++i) {
    TableColumn& col = all_columns_[*i];
    int col_index = static_cast<int>(i - visible_columns_.begin());
    if (col.width == -1) {
      if (col.percent > 0) {
        percent += col.percent;
      } else {
        autosize_width += col.min_visible_width;
      }
    } else {
      fixed_width += ListView_GetColumnWidth(list_view_, col_index);
    }
  }

  // Now do a pass to set the actual sizes of auto-sized and
  // percent-sized columns.
  int available_width = width - fixed_width - autosize_width;
  for (std::vector<int>::const_iterator i = visible_columns_.begin();
       i != visible_columns_.end(); ++i) {
    TableColumn& col = all_columns_[*i];
    if (col.width == -1) {
      int col_index = static_cast<int>(i - visible_columns_.begin());
      if (col.percent > 0) {
        if (available_width > 0) {
          int col_width =
            static_cast<int>(available_width * (col.percent / percent));
          available_width -= col_width;
          percent -= col.percent;
          ListView_SetColumnWidth(list_view_, col_index, col_width);
        }
      } else {
        int col_width = col.min_visible_width;
        // If no "percent" columns, the last column acts as one, if auto-sized.
        if (percent == 0.f && available_width > 0 &&
            col_index == column_count_ - 1) {
          col_width += available_width;
        }
        ListView_SetColumnWidth(list_view_, col_index, col_width);
      }
    }
  }
}

void TableView::SetPreferredSize(const CSize& preferred_size) {
  preferred_size_  = preferred_size;
}

void TableView::GetPreferredSize(CSize* out) {
  DCHECK(out);
  *out = preferred_size_;
}


void TableView::UpdateListViewCache0(int start, int length, bool add) {
  LVITEM item = {0};
  int start_column = 0;
  int max_row = start + length;
  const bool has_groups =
      (win_util::GetWinVersion() > win_util::WINVERSION_2000 &&
       model_->HasGroups());
  if (has_groups)
    item.mask = LVIF_GROUPID;
  if (add) {
    for (int i = start; i < max_row; ++i) {
      item.iItem = i;
      if (has_groups)
        item.iGroupId = model_->GetGroupID(i);
      ListView_InsertItem(list_view_, &item);
    }
  }

  memset(&item, 0, sizeof(LVITEM));

  // NOTE: I don't quite get why the iSubItem in the following is not offset
  // by 1. According to the docs it should be offset by one, but that doesn't
  // work.
  if (table_type_ == CHECK_BOX_AND_TEXT) {
    start_column = 1;
    item.iSubItem = 0;
    item.mask = LVIF_TEXT | LVIF_STATE;
    item.stateMask = LVIS_STATEIMAGEMASK;
    for (int i = start; i < max_row; ++i) {
      std::wstring text = model_->GetText(i, visible_columns_[0]);
      item.iItem = i;
      item.pszText = const_cast<LPWSTR>(text.c_str());
      item.state = INDEXTOSTATEIMAGEMASK(model_->IsChecked(i) ? 2 : 1) ;
      ListView_SetItem(list_view_, &item);
    }
  }
  if (start_column == column_count_)
    return;

  item.stateMask = 0;
  item.mask = LVIF_TEXT;
  if (table_type_ == ICON_AND_TEXT) {
    item.mask |= LVIF_IMAGE;
  }
  for (int j = start_column; j < column_count_; ++j) {
    TableColumn& col = all_columns_[visible_columns_[j]];
    int max_text_width = ListView_GetStringWidth(list_view_, col.title.c_str());
    for (int i = start; i < max_row; ++i) {
      item.iItem = i;
      item.iSubItem = j;
      std::wstring text = model_->GetText(i, visible_columns_[j]);
      item.pszText = const_cast<LPWSTR>(text.c_str());
      item.iImage = 0;
      ListView_SetItem(list_view_, &item);

      // Compute width in px, using current font.
      int string_width = ListView_GetStringWidth(list_view_, item.pszText);
      // The width of an icon belongs to the first column.
      if (j == 0 && table_type_ == ICON_AND_TEXT)
        string_width += kListViewIconWidthAndPadding;
      max_text_width = std::max(string_width, max_text_width);
    }

    // ListView_GetStringWidth must be padded or else truncation will occur
    // (MSDN). 15px matches the Win32/LVSCW_AUTOSIZE_USEHEADER behavior.
    max_text_width += kListViewTextPadding;

    // Protect against partial update.
    if (max_text_width > col.min_visible_width ||
      (start == 0 && length == model_->RowCount())) {
      col.min_visible_width = max_text_width;
    }
  }
}

void TableView::OnDoubleClick() {
  if (!ignore_listview_change_ && table_view_observer_) {
    table_view_observer_->OnDoubleClick();
  }
}

void TableView::OnSelectedStateChanged(int item, bool is_selected) {
  if (!ignore_listview_change_ && table_view_observer_) {
    table_view_observer_->OnSelectionChanged();
  }
}

void TableView::OnCheckedStateChanged(int item, bool is_checked) {
  if (!ignore_listview_change_) {
    model_->SetChecked(item, is_checked);
  }
}

int TableView::PreviousSelectedIndex(int item) {
  DCHECK(item >= 0);
  if (!list_view_ || item <= 0)
    return -1;

  int row_count = RowCount();
  if (row_count == 0)
    return -1;  // Empty table, nothing can be selected.

  // For some reason
  // ListView_GetNextItem(list_view_,item, LVNI_SELECTED | LVNI_ABOVE)
  // fails on Vista (always returns -1), so we iterate through the indices.
  item = std::min(item, row_count);
  while (--item >= 0 && !IsItemSelected(item));
  return item;
}

int TableView::LastSelectedIndex() {
  return PreviousSelectedIndex(RowCount());
}

void TableView::UpdateContentOffset() {
  content_offset_ = 0;

  if (!list_view_)
    return;

  HWND header = ListView_GetHeader(list_view_);
  if (!header)
    return;

  POINT origin = {0, 0};
  MapWindowPoints(header, list_view_, &origin, 1);

  CRect header_bounds;
  GetWindowRect(header, &header_bounds);

  content_offset_ = origin.y + header_bounds.Height();
}

//
// TableSelectionIterator
//
TableSelectionIterator::TableSelectionIterator(TableView* view,
                                               int index)
    : table_view_(view), index_(index) {
}

TableSelectionIterator& TableSelectionIterator::operator=(
    const TableSelectionIterator& other) {
  index_ = other.index_;
  return *this;
}

bool TableSelectionIterator::operator==(const TableSelectionIterator& other) {
  return (other.index_ == index_);
}

bool TableSelectionIterator::operator!=(const TableSelectionIterator& other) {
  return (other.index_ != index_);
}

TableSelectionIterator& TableSelectionIterator::operator++() {
  index_ = table_view_->PreviousSelectedIndex(index_);
  return *this;
}

int TableSelectionIterator::operator*() {
  return index_;
}

}  // namespace
