// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/bookmark_table_view.h"

#include "base/base_drag_source.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_table_model.h"
#include "chrome/browser/profile.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/views/chrome_menu.h"

#include "generated_resources.h"

namespace {

// Height of the drop indicator used when dropping between rows.
const int kDropHighlightHeight = 2;

int GetWidthOfColumn(const std::vector<views::TableColumn>& columns,
                     const std::vector<int> widths,
                     int column_id) {
  for (size_t i = 0; i < columns.size(); ++i) {
    if (columns[i].id == column_id)
      return widths[i];
  }
  NOTREACHED();
  return -1;
}

}  // namespace

BookmarkTableView::BookmarkTableView(Profile* profile,
                                     BookmarkTableModel* model)
    : views::TableView(model, std::vector<views::TableColumn>(),
                       views::ICON_AND_TEXT, false, true, true),
      profile_(profile),
      show_path_column_(false) {
  UpdateColumns();
}

// static
void BookmarkTableView::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kBookmarkTableNameWidth1, -1);
  prefs->RegisterIntegerPref(prefs::kBookmarkTableURLWidth1, -1);
  prefs->RegisterIntegerPref(prefs::kBookmarkTableNameWidth2, -1);
  prefs->RegisterIntegerPref(prefs::kBookmarkTableURLWidth2, -1);
  prefs->RegisterIntegerPref(prefs::kBookmarkTablePathWidth, -1);
}

bool BookmarkTableView::CanDrop(const OSExchangeData& data) {
  if (!parent_node_ || !profile_->GetBookmarkModel()->IsLoaded())
    return false;

  drop_info_.reset(new DropInfo());
  if (!drop_info_->drag_data.Read(data))
    return false;

  // Don't allow the user to drop an ancestor of the parent node onto the
  // parent node. This would create a cycle, which is definitely a no-no.
  std::vector<BookmarkNode*> nodes = drop_info_->drag_data.GetNodes(profile_);
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (parent_node_->HasAncestor(nodes[i]))
      return false;
  }
  return true;
}

void BookmarkTableView::OnDragEntered(const views::DropTargetEvent& event) {
}

int BookmarkTableView::OnDragUpdated(const views::DropTargetEvent& event) {
  if (!parent_node_)
    return false;

  LVHITTESTINFO hit_info = {0};
  hit_info.pt.x = event.x();
  hit_info.pt.y = event.y();
  // TODO(sky): need to support auto-scroll and all that good stuff.

  int drop_index;
  bool drop_on;
  drop_index = CalculateDropIndex(event.y(), &drop_on);

  drop_info_->drop_operation =
      CalculateDropOperation(event, drop_index, drop_on);

  if (drop_info_->drop_operation == DragDropTypes::DRAG_NONE) {
    drop_index = -1;
    drop_on = false;
  }

  SetDropIndex(drop_index, drop_on);

  return drop_info_->drop_operation;
}

void BookmarkTableView::OnDragExited() {
  SetDropIndex(-1, false);
  drop_info_.reset();
}

int BookmarkTableView::OnPerformDrop(const views::DropTargetEvent& event) {
  OnPerformDropImpl();
  int drop_operation = drop_info_->drop_operation;
  SetDropIndex(-1, false);
  drop_info_.reset();
  return drop_operation;
}

BookmarkTableModel* BookmarkTableView::bookmark_table_model() const {
  return static_cast<BookmarkTableModel*>(model());
}

void BookmarkTableView::SaveColumnConfiguration() {
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs)
    return;

  if (show_path_column_) {
    prefs->SetInteger(prefs::kBookmarkTableNameWidth2,
                      GetColumnWidth(IDS_BOOKMARK_TABLE_TITLE));
    prefs->SetInteger(prefs::kBookmarkTableURLWidth2,
                      GetColumnWidth(IDS_BOOKMARK_TABLE_URL));
    prefs->SetInteger(prefs::kBookmarkTablePathWidth,
                      GetColumnWidth(IDS_BOOKMARK_TABLE_PATH));
  } else {
    prefs->SetInteger(prefs::kBookmarkTableNameWidth1,
                      GetColumnWidth(IDS_BOOKMARK_TABLE_TITLE));
    prefs->SetInteger(prefs::kBookmarkTableURLWidth1,
                      GetColumnWidth(IDS_BOOKMARK_TABLE_URL));
  }
}

void BookmarkTableView::SetShowPathColumn(bool show_path_column) {
  if (show_path_column == show_path_column_)
    return;

  SaveColumnConfiguration();

  show_path_column_ = show_path_column;
  UpdateColumns();
}

void BookmarkTableView::PostPaint() {
  if (!drop_info_.get() || drop_info_->drop_index == -1 ||
      drop_info_->drop_on) {
    return;
  }

  RECT bounds = GetDropBetweenHighlightRect(drop_info_->drop_index);
  HDC dc = GetDC(GetNativeControlHWND());
  HBRUSH brush = CreateSolidBrush(GetSysColor(COLOR_WINDOWTEXT));
  FillRect(dc, &bounds, brush);
  DeleteObject(brush);
  ReleaseDC(GetNativeControlHWND(), dc);
}

LRESULT BookmarkTableView::OnNotify(int w_param, LPNMHDR l_param) {
  switch(l_param->code) {
    case LVN_BEGINDRAG:
      BeginDrag();
      return 0;  // Return value doesn't matter for this message.
  }

  return TableView::OnNotify(w_param, l_param);
}

void BookmarkTableView::BeginDrag() {
  std::vector<BookmarkNode*> nodes_to_drag;
  for (TableView::iterator i = SelectionBegin(); i != SelectionEnd(); ++i)
    nodes_to_drag.push_back(bookmark_table_model()->GetNodeForRow(*i));
  if (nodes_to_drag.empty())
    return;  // Nothing to drag.

  scoped_refptr<OSExchangeData> data = new OSExchangeData;
  BookmarkDragData(nodes_to_drag).Write(profile_, data);
  scoped_refptr<BaseDragSource> drag_source(new BaseDragSource);
  DWORD effects;
  DoDragDrop(data, drag_source,
             DROPEFFECT_LINK | DROPEFFECT_COPY | DROPEFFECT_MOVE, &effects);
}

int BookmarkTableView::CalculateDropOperation(
    const views::DropTargetEvent& event,
    int drop_index,
    bool drop_on) {
  if (drop_info_->drag_data.IsFromProfile(profile_)) {
    // Data from the same profile. Prefer move, but do copy if the user wants
    // that.
    if (event.IsControlDown())
      return DragDropTypes::DRAG_COPY;

    int real_drop_index;
    BookmarkNode* drop_parent = GetDropParentAndIndex(drop_index, drop_on,
                                                      &real_drop_index);
    if (!bookmark_utils::IsValidDropLocation(
        profile_, drop_info_->drag_data, drop_parent, real_drop_index)) {
      return DragDropTypes::DRAG_NONE;
    }
    return DragDropTypes::DRAG_MOVE;
  }
  // We're going to copy, but return an operation compatible with the source
  // operations so that the user can drop.
  return bookmark_utils::PreferredDropOperation(
      event, DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK);
}

void BookmarkTableView::OnPerformDropImpl() {
  int drop_index;
  BookmarkNode* drop_parent = GetDropParentAndIndex(
      drop_info_->drop_index, drop_info_->drop_on, &drop_index);
  BookmarkModel* model = profile_->GetBookmarkModel();
  int min_selection;
  int max_selection;
  // If the data is not from this profile we return an operation compatible
  // with the source. As such, we need to need to check the data here too.
  if (!drop_info_->drag_data.IsFromProfile(profile_) ||
      drop_info_->drop_operation == DragDropTypes::DRAG_COPY) {
    bookmark_utils::CloneDragData(model, drop_info_->drag_data.elements,
                                  drop_parent, drop_index);
    min_selection = drop_index;
    max_selection = drop_index +
        static_cast<int>(drop_info_->drag_data.elements.size());
  } else {
    // else, move.
    std::vector<BookmarkNode*> nodes = drop_info_->drag_data.GetNodes(profile_);
    if (nodes.empty())
      return;

    for (size_t i = 0; i < nodes.size(); ++i) {
      model->Move(nodes[i], drop_parent, drop_index);
      // Reset the drop_index, just in case the index didn't really change.
      drop_index = drop_parent->IndexOfChild(nodes[i]) + 1;
    }
    min_selection = drop_parent->IndexOfChild(nodes[0]);
    max_selection = min_selection + static_cast<int>(nodes.size());
  }
  if (min_selection < RowCount() && max_selection < RowCount()) {
    // Select the moved/copied rows.
    Select(min_selection);
    if (min_selection + 1 < max_selection) {
      // SetSelectedState doesn't send notification, so we manually do it.
      for (int i = min_selection + 1; i < max_selection; ++i)
        SetSelectedState(i, true);
      if (observer())
        observer()->OnSelectionChanged();
    }
  }
}

void BookmarkTableView::SetDropIndex(int index, bool drop_on) {
  if (drop_info_->drop_index == index && drop_info_->drop_on == drop_on)
    return;

  UpdateDropIndex(drop_info_->drop_index, drop_info_->drop_on, false);

  drop_info_->drop_index = index;
  drop_info_->drop_on = drop_on;

  UpdateDropIndex(drop_info_->drop_index, drop_info_->drop_on, true);
}

void BookmarkTableView::UpdateDropIndex(int index, bool drop_on, bool turn_on) {
  if (index == -1)
    return;

  if (drop_on) {
    ListView_SetItemState(GetNativeControlHWND(), index,
                          turn_on ? LVIS_DROPHILITED : 0, LVIS_DROPHILITED);
  } else {
    RECT bounds = GetDropBetweenHighlightRect(index);
    InvalidateRect(GetNativeControlHWND(), &bounds, FALSE);
  }
}

int BookmarkTableView::CalculateDropIndex(int y, bool* drop_on) {
  *drop_on = false;
  HWND hwnd = GetNativeControlHWND();
  int row_count = RowCount();
  int top_index = ListView_GetTopIndex(hwnd);
  if (row_count == 0 || top_index < 0)
    return 0;

  for (int i = top_index; i < row_count; ++i) {
    RECT bounds;
    ListView_GetItemRect(hwnd, i, &bounds, LVIR_BOUNDS);
    if (y < bounds.top)
      return i;
    if (y < bounds.bottom) {
      if (bookmark_table_model()->GetNodeForRow(i)->is_folder()) {
        if (y < bounds.top + views::MenuItemView::kDropBetweenPixels)
          return i;
        if (y >= bounds.bottom - views::MenuItemView::kDropBetweenPixels)
          return i + 1;
        *drop_on = true;
        return i;
      }
      if (y < (bounds.bottom - bounds.top) / 2 + bounds.top)
        return i;
      return i + 1;
    }
  }
  return row_count;
}

BookmarkNode* BookmarkTableView::GetDropParentAndIndex(int visual_drop_index,
                                                       bool drop_on,
                                                       int* index) {
  if (drop_on) {
    BookmarkNode* parent = parent_node_->GetChild(visual_drop_index);
    *index = parent->GetChildCount();
    return parent;
  }
  *index = visual_drop_index;
  return parent_node_;
}

RECT BookmarkTableView::GetDropBetweenHighlightRect(int index) {
  RECT bounds = { 0 };
  if (RowCount() == 0) {
    bounds.top = content_offset();
    bounds.left = 0;
    bounds.right = width();
  } else if (index >= RowCount()) {
    ListView_GetItemRect(GetNativeControlHWND(), index - 1, &bounds,
                         LVIR_BOUNDS);
    bounds.top = bounds.bottom - kDropHighlightHeight / 2;
  } else {
    ListView_GetItemRect(GetNativeControlHWND(), index, &bounds, LVIR_BOUNDS);
    bounds.top -= kDropHighlightHeight / 2;
  }
  bounds.bottom = bounds.top + kDropHighlightHeight;
  return bounds;
}
void BookmarkTableView::UpdateColumns() {
  PrefService* prefs = profile_->GetPrefs();
  views::TableColumn name_column =
      views::TableColumn(IDS_BOOKMARK_TABLE_TITLE, views::TableColumn::LEFT,
                         -1);
  views::TableColumn url_column =
      views::TableColumn(IDS_BOOKMARK_TABLE_URL, views::TableColumn::LEFT, -1);
  views::TableColumn path_column =
      views::TableColumn(IDS_BOOKMARK_TABLE_PATH, views::TableColumn::LEFT, -1);

  std::vector<views::TableColumn> columns;
  if (show_path_column_) {
    int name_width = -1;
    int url_width = -1;
    int path_width = -1;
    if (prefs) {
      name_width = prefs->GetInteger(prefs::kBookmarkTableNameWidth2);
      url_width = prefs->GetInteger(prefs::kBookmarkTableURLWidth2);
      path_width = prefs->GetInteger(prefs::kBookmarkTablePathWidth);
    }
    if (name_width != -1 && url_width != -1 && path_width != -1) {
      name_column.width = name_width;
      url_column.width = url_width;
      path_column.width = path_width;
    } else {
      name_column.percent = .5;
      url_column.percent = .25;
      path_column.percent= .25;
    }
    columns.push_back(name_column);
    columns.push_back(url_column);
    columns.push_back(path_column);
  } else {
    int name_width = -1;
    int url_width = -1;
    if (prefs) {
      name_width = prefs->GetInteger(prefs::kBookmarkTableNameWidth1);
      url_width = prefs->GetInteger(prefs::kBookmarkTableURLWidth1);
    }
    if (name_width != -1 && url_width != -1) {
      name_column.width = name_width;
      url_column.width = url_width;
    } else {
      name_column.percent = .5;
      url_column.percent = .5;
    }
    columns.push_back(name_column);
    columns.push_back(url_column);
  }
  SetColumns(columns);
  for (size_t i = 0; i < columns.size(); ++i)
    SetColumnVisibility(columns[i].id, true);
  OnModelChanged();
}
