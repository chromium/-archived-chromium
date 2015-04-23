// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/table_model.h"

#include "app/l10n_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

// TableColumn -----------------------------------------------------------------

TableColumn::TableColumn()
    : id(0),
      title(),
      alignment(LEFT),
      width(-1),
      percent(),
      min_visible_width(0),
      sortable(false) {
}

TableColumn::TableColumn(int id, const std::wstring& title,
                         Alignment alignment,
                         int width)
    : id(id),
      title(title),
      alignment(alignment),
      width(width),
      percent(0),
      min_visible_width(0),
      sortable(false) {
}

TableColumn::TableColumn(int id, const std::wstring& title,
                         Alignment alignment, int width, float percent)
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
TableColumn::TableColumn(int id, Alignment alignment, int width)
    : id(id),
      alignment(alignment),
      width(width),
      percent(0),
      min_visible_width(0),
      sortable(false) {
  title = l10n_util::GetString(id);
}

TableColumn::TableColumn(int id, Alignment alignment, int width, float percent)
    : id(id),
      alignment(alignment),
      width(width),
      percent(percent),
      min_visible_width(0),
      sortable(false) {
  title = l10n_util::GetString(id);
}

// TableModel -----------------------------------------------------------------

// Used for sorting.
static Collator* collator = NULL;

SkBitmap TableModel::GetIcon(int row) {
  return SkBitmap();
}

int TableModel::CompareValues(int row1, int row2, int column_id) {
  DCHECK(row1 >= 0 && row1 < RowCount() &&
         row2 >= 0 && row2 < RowCount());
  std::wstring value1 = GetText(row1, column_id);
  std::wstring value2 = GetText(row2, column_id);
  Collator* collator = GetCollator();

  if (collator)
    return l10n_util::CompareStringWithCollator(collator, value1, value2);

  NOTREACHED();
  return 0;
}

Collator* TableModel::GetCollator() {
  if (!collator) {
    UErrorCode create_status = U_ZERO_ERROR;
    collator = Collator::createInstance(create_status);
    if (!U_SUCCESS(create_status)) {
      collator = NULL;
      NOTREACHED();
    }
  }
  return collator;
}

void TableModel::ClearCollator() {
  delete collator;
  collator = NULL;
}
