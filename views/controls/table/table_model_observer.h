// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROL_TABLE_TABLE_MODEL_OBSERVER_H_
#define VIEWS_CONTROL_TABLE_TABLE_MODEL_OBSERVER_H_

namespace views {

// Observer for a TableModel. Anytime the model changes, it must notify its
// observer.
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

}  // namespace views

#endif  // VIEWS_CONTROL_TABLE_TABLE_MODEL_OBSERVER_H_
