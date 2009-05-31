// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/table/table_model.h"

#include "app/l10n_util.h"

namespace views {

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

}  // namespace views

