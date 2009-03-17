// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/controls/table/group_table_view.h"

#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/common/gfx/chrome_canvas.h"

namespace views {

static const COLORREF kSeparatorLineColor = RGB(208, 208, 208);
static const int kSeparatorLineThickness = 1;

const char GroupTableView::kViewClassName[] = "chrome/views/GroupTableView";

GroupTableView::GroupTableView(GroupTableModel* model,
                               const std::vector<TableColumn>& columns,
                               TableTypes table_type,
                               bool single_selection,
                               bool resizable_columns,
                               bool autosize_columns)
    : TableView(model, columns, table_type, false, resizable_columns,
                autosize_columns),
    model_(model),
    sync_selection_factory_(this) {
}

GroupTableView::~GroupTableView() {
}

void GroupTableView::SyncSelection() {
  int index = 0;
  int row_count = model_->RowCount();
  GroupRange group_range;
  while (index < row_count) {
    model_->GetGroupRangeForItem(index, &group_range);
    if (group_range.length == 1) {
      // No synching required for single items.
      index++;
    }  else {
      // We need to select the group if at least one item is selected.
      bool should_select = false;
      for (int i = group_range.start;
           i < group_range.start + group_range.length; ++i) {
        if (IsItemSelected(i)) {
          should_select = true;
          break;
        }
      }
      if (should_select) {
        for (int i = group_range.start;
             i < group_range.start + group_range.length; ++i) {
          SetSelectedState(i, true);
        }
      }
      index += group_range.length;
    }
  }
}

void GroupTableView::OnKeyDown(unsigned short virtual_keycode) {
  // In a list view, multiple items can be selected but only one item has the
  // focus. This creates a problem when the arrow keys are used for navigating
  // between items in the list view. An example will make this more clear:
  //
  // Suppose we have 5 items in the list view, and three of these items are
  // part of one group:
  //
  // Index0: ItemA (No Group)
  // Index1: ItemB (GroupX)
  // Index2: ItemC (GroupX)
  // Index3: ItemD (GroupX)
  // Index4: ItemE (No Group)
  //
  // When GroupX is selected (say, by clicking on ItemD with the mouse),
  // GroupTableView::SyncSelection() will make sure ItemB, ItemC and ItemD are
  // selected. Also, the item with the focus will be ItemD (simply because
  // this is the item the user happened to click on). If then the UP arrow is
  // pressed once, the focus will be switched to ItemC and not to ItemA and the
  // end result is that we are stuck in GroupX even though the intention was to
  // switch to ItemA.
  //
  // For that exact reason, we need to set the focus appropriately when we
  // detect that one of the arrow keys is pressed. Thus, when it comes time
  // for the list view control to actually switch the focus, the right item
  // will be selected.
  if ((virtual_keycode != VK_UP) && (virtual_keycode != VK_DOWN)) {
    TableView::OnKeyDown(virtual_keycode);
    return;
  }

  // We start by finding the index of the item with the focus. If no item
  // currently has the focus, then this routine doesn't do anything.
  int focused_index;
  int row_count = model_->RowCount();
  for (focused_index = 0; focused_index < row_count; focused_index++) {
    if (ItemHasTheFocus(focused_index)) {
      break;
    }
  }

  if (focused_index == row_count) {
    return;
  }
  DCHECK_LT(focused_index, row_count);

  // Nothing to do if the item which has the focus is not part of a group.
  GroupRange group_range;
  model_->GetGroupRangeForItem(focused_index, &group_range);
  if (group_range.length == 1) {
    return;
  }

  // If the user pressed the UP key, then the focus should be set to the
  // topmost element in the group. If the user pressed the DOWN key, the focus
  // should be set to the bottommost element.
  if (virtual_keycode == VK_UP) {
    SetFocusOnItem(group_range.start);
  } else {
    DCHECK_EQ(virtual_keycode, VK_DOWN);
    SetFocusOnItem(group_range.start + group_range.length - 1);
  }
}

void GroupTableView::PrepareForSort() {
  GroupRange range;
  int row_count = RowCount();
  model_index_to_range_start_map_.clear();
  for (int model_row = 0; model_row < row_count;) {
    model_->GetGroupRangeForItem(model_row, &range);
    for (int range_counter = 0; range_counter < range.length; range_counter++)
      model_index_to_range_start_map_[range_counter + model_row] = model_row;
    model_row += range.length;
  }
}

int GroupTableView::CompareRows(int model_row1, int model_row2) {
  int range1 = model_index_to_range_start_map_[model_row1];
  int range2 = model_index_to_range_start_map_[model_row2];
  if (range1 == range2) {
    // The two rows are in the same group, sort so that items in the same group
    // always appear in the same order.
    return model_row1 - model_row2;
  }
  // Sort by the first entry of each of the groups.
  return TableView::CompareRows(range1, range2);
}

void GroupTableView::OnSelectedStateChanged() {
  // The goal is to make sure all items for a same group are in a consistent
  // state in term of selection. When a user clicks an item, several selection
  // messages are sent, possibly including unselecting all currently selected
  // items. For that reason, we post a task to be performed later, after all
  // selection messages have been processed. In the meantime we just ignore all
  // selection notifications.
  if (sync_selection_factory_.empty()) {
    MessageLoop::current()->PostTask(FROM_HERE,
        sync_selection_factory_.NewRunnableMethod(
            &GroupTableView::SyncSelection));
  }
  TableView::OnSelectedStateChanged();
}

// Draws the line separator betweens the groups.
void GroupTableView::PostPaint(int model_row, int column, bool selected,
                               const CRect& bounds, HDC hdc) {
  GroupRange group_range;
  model_->GetGroupRangeForItem(model_row, &group_range);

  // We always paint a vertical line at the end of the last cell.
  HPEN hPen = CreatePen(PS_SOLID, kSeparatorLineThickness, kSeparatorLineColor);
  HPEN hPenOld = (HPEN) SelectObject(hdc, hPen);
  int x = static_cast<int>(bounds.right - kSeparatorLineThickness);
  MoveToEx(hdc, x, bounds.top, NULL);
  LineTo(hdc, x, bounds.bottom);

  // We paint a separator line after the last item of a group.
  if (model_row == (group_range.start + group_range.length - 1)) {
    int y = static_cast<int>(bounds.bottom - kSeparatorLineThickness);
    MoveToEx(hdc, 0, y, NULL);
    LineTo(hdc, bounds.Width(), y);
  }
  SelectObject(hdc, hPenOld);
  DeleteObject(hPen);
}

std::string GroupTableView::GetClassName() const {
  return kViewClassName;
}

}  // namespace views
