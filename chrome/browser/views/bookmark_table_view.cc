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
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/views/view_constants.h"
#include "grit/generated_resources.h"

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

void BookmarkTableView::DropInfo::Scrolled() {
  view_->UpdateDropInfo();
}

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

  BookmarkDragData drag_data;
  if (!drag_data.Read(data))
    return false;

  // Don't allow the user to drop an ancestor of the parent node onto the
  // parent node. This would create a cycle, which is definitely a no-no.
  std::vector<BookmarkNode*> nodes = drag_data.GetNodes(profile_);
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (parent_node_->HasAncestor(nodes[i]))
      return false;
  }

  drop_info_.reset(new DropInfo(this));
  drop_info_->SetData(drag_data);
  return true;
}

void BookmarkTableView::OnDragEntered(const views::DropTargetEvent& event) {
}

int BookmarkTableView::OnDragUpdated(const views::DropTargetEvent& event) {
  if (!parent_node_ || !drop_info_.get()) {
    drop_info_.reset(NULL);
    return false;
  }

  drop_info_->Update(event);
  return UpdateDropInfo();
}

void BookmarkTableView::OnDragExited() {
  SetDropPosition(DropPosition());
  drop_info_.reset();
}

int BookmarkTableView::OnPerformDrop(const views::DropTargetEvent& event) {
  OnPerformDropImpl();
  int drop_operation = drop_info_->drop_operation();
  SetDropPosition(DropPosition());
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

void BookmarkTableView::SetAltText(const std::wstring& alt_text) {
  if (alt_text == alt_text_)
    return;

  alt_text_ = alt_text;
  if (!GetNativeControlHWND())
    return;

  RECT alt_text_bounds = GetAltTextBounds().ToRECT();
  InvalidateRect(GetNativeControlHWND(), &alt_text_bounds, FALSE);
}

void BookmarkTableView::SetShowPathColumn(bool show_path_column) {
  if (show_path_column == show_path_column_)
    return;

  SaveColumnConfiguration();

  show_path_column_ = show_path_column;
  UpdateColumns();
}

void BookmarkTableView::PostPaint() {
  PaintAltText();

  if (!drop_info_.get() || drop_info_->position().index == -1 ||
      drop_info_->position().on) {
    return;
  }

  RECT bounds = GetDropBetweenHighlightRect(drop_info_->position().index);
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

int BookmarkTableView::UpdateDropInfo() {
  DropPosition position = CalculateDropPosition(drop_info_->last_y());

  drop_info_->set_drop_operation(CalculateDropOperation(position));

  if (drop_info_->drop_operation() == DragDropTypes::DRAG_NONE)
    position = DropPosition();

  SetDropPosition(position);

  return drop_info_->drop_operation();
}

void BookmarkTableView::BeginDrag() {
  std::vector<BookmarkNode*> nodes_to_drag;
  for (TableView::iterator i = SelectionBegin(); i != SelectionEnd(); ++i)
    nodes_to_drag.push_back(bookmark_table_model()->GetNodeForRow(*i));
  if (nodes_to_drag.empty())
    return;  // Nothing to drag.

  // Reverse the nodes so that they are put on the clipboard in visual order.
  // We do this as SelectionBegin starts at the end of the visual order.
  std::reverse(nodes_to_drag.begin(), nodes_to_drag.end());

  scoped_refptr<OSExchangeData> data = new OSExchangeData;
  BookmarkDragData(nodes_to_drag).Write(profile_, data);
  scoped_refptr<BaseDragSource> drag_source(new BaseDragSource);
  DWORD effects;
  DoDragDrop(data, drag_source,
             DROPEFFECT_LINK | DROPEFFECT_COPY | DROPEFFECT_MOVE, &effects);
}

int BookmarkTableView::CalculateDropOperation(const DropPosition& position) {
  if (drop_info_->data().IsFromProfile(profile_)) {
    // Data from the same profile. Prefer move, but do copy if the user wants
    // that.
    if (drop_info_->is_control_down())
      return DragDropTypes::DRAG_COPY;

    int real_drop_index;
    BookmarkNode* drop_parent = GetDropParentAndIndex(position,
                                                      &real_drop_index);
    if (!bookmark_utils::IsValidDropLocation(
        profile_, drop_info_->data(), drop_parent, real_drop_index)) {
      return DragDropTypes::DRAG_NONE;
    }
    return DragDropTypes::DRAG_MOVE;
  }
  // We're going to copy, but return an operation compatible with the source
  // operations so that the user can drop.
  return bookmark_utils::PreferredDropOperation(
      drop_info_->source_operations(),
      DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK);
}

void BookmarkTableView::OnPerformDropImpl() {
  int drop_index;
  BookmarkNode* drop_parent = GetDropParentAndIndex(
      drop_info_->position(), &drop_index);
  BookmarkModel* model = profile_->GetBookmarkModel();
  int min_selection;
  int max_selection;
  // If the data is not from this profile we return an operation compatible
  // with the source. As such, we need to need to check the data here too.
  if (!drop_info_->data().IsFromProfile(profile_) ||
      drop_info_->drop_operation() == DragDropTypes::DRAG_COPY) {
    bookmark_utils::CloneDragData(model, drop_info_->data().elements,
                                  drop_parent, drop_index);
    min_selection = drop_index;
    max_selection = drop_index +
        static_cast<int>(drop_info_->data().elements.size());
  } else {
    // else, move.
    std::vector<BookmarkNode*> nodes = drop_info_->data().GetNodes(profile_);
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
  if (drop_info_->position().on) {
    // The user dropped on a folder, select it.
    int index = parent_node_->IndexOfChild(drop_parent);
    if (index != -1)
      Select(index);
  } else if (min_selection < RowCount() && max_selection <= RowCount()) {
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

void BookmarkTableView::SetDropPosition(const DropPosition& position) {
  if (drop_info_->position().equals(position))
    return;

  UpdateDropIndicator(drop_info_->position(), false);

  drop_info_->set_position(position);

  UpdateDropIndicator(drop_info_->position(), true);
}

void BookmarkTableView::UpdateDropIndicator(const DropPosition& position,
                                            bool turn_on) {
  if (position.index == -1)
    return;

  if (position.on) {
    ListView_SetItemState(GetNativeControlHWND(), position.index,
                          turn_on ? LVIS_DROPHILITED : 0, LVIS_DROPHILITED);
  } else {
    RECT bounds = GetDropBetweenHighlightRect(position.index);
    InvalidateRect(GetNativeControlHWND(), &bounds, FALSE);
  }
}

BookmarkTableView::DropPosition
    BookmarkTableView::CalculateDropPosition(int y) {
  HWND hwnd = GetNativeControlHWND();
  int row_count = RowCount();
  int top_index = ListView_GetTopIndex(hwnd);
  if (row_count == 0 || top_index < 0)
    return DropPosition(0, false);

  for (int i = top_index; i < row_count; ++i) {
    RECT bounds;
    ListView_GetItemRect(hwnd, i, &bounds, LVIR_BOUNDS);
    if (y < bounds.top)
      return DropPosition(i, false);
    if (y < bounds.bottom) {
      if (bookmark_table_model()->GetNodeForRow(i)->is_folder()) {
        if (y < bounds.top + views::kDropBetweenPixels)
          return DropPosition(i, false);
        if (y >= bounds.bottom - views::kDropBetweenPixels)
          return DropPosition(i + 1, false);
        return DropPosition(i, true);
      }
      if (y < (bounds.bottom - bounds.top) / 2 + bounds.top)
        return DropPosition(i, false);
      return DropPosition(i + 1, false);
    }
  }
  return DropPosition(row_count, false);
}

BookmarkNode* BookmarkTableView::GetDropParentAndIndex(
    const DropPosition& position,
    int* index) {
  if (position.on) {
    BookmarkNode* parent = parent_node_->GetChild(position.index);
    *index = parent->GetChildCount();
    return parent;
  }
  *index = position.index;
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

void BookmarkTableView::PaintAltText() {
  if (alt_text_.empty())
    return;

  HDC dc = GetDC(GetNativeControlHWND());
  ChromeFont font = GetAltTextFont();
  gfx::Rect bounds = GetAltTextBounds();
  ChromeCanvas canvas(bounds.width(), bounds.height(), false);
  // Pad by 1 for halo.
  canvas.DrawStringWithHalo(alt_text_, font, SK_ColorDKGRAY, SK_ColorWHITE, 1,
                            1, bounds.width() - 2, bounds.height() - 2,
                            ChromeCanvas::TEXT_ALIGN_LEFT);
  canvas.getTopPlatformDevice().drawToHDC(dc, bounds.x(), bounds.y(), NULL);
  ReleaseDC(GetNativeControlHWND(), dc);
}

gfx::Rect BookmarkTableView::GetAltTextBounds() {
  static const int kXOffset = 16;
  DCHECK(GetNativeControlHWND());
  CRect client_rect;
  GetClientRect(GetNativeControlHWND(), client_rect);
  ChromeFont font = GetAltTextFont();
  // Pad height by 2 for halo.
  return gfx::Rect(kXOffset, content_offset(), client_rect.Width() - kXOffset,
                   std::max(kImageSize, font.height() + 2));
}

ChromeFont BookmarkTableView::GetAltTextFont() {
  return ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);
}
