// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TABLE_TABLE_VIEW_OBSERVER_H_
#define VIEWS_CONTROLS_TABLE_TABLE_VIEW_OBSERVER_H_

namespace views {

class TableView;

// TableViewObserver is notified about the TableView selection.
class TableViewObserver {
 public:
  virtual ~TableViewObserver() {}

  // Invoked when the selection changes.
  virtual void OnSelectionChanged() = 0;

  // Optional method invoked when the user double clicks on the table.
  virtual void OnDoubleClick() {}

  // Optional method invoked when the user middle clicks on the table.
  virtual void OnMiddleClick() {}

  // Optional method invoked when the user hits a key with the table in focus.
  virtual void OnKeyDown(unsigned short virtual_keycode) {}

  // Invoked when the user presses the delete key.
  virtual void OnTableViewDelete(TableView* table_view) {}
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TABLE_TABLE_VIEW_OBSERVER_H_
