// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager.h"

#include <vector>

#include "base/logging.h"

namespace {

class TaskManagerGtk : public TaskManagerModelObserver {
 public:
  TaskManagerGtk(TaskManagerModel* model) {
    model->SetObserver(this);
  }

  // TaskManagerModelObserver
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);
};

void TaskManagerGtk::OnModelChanged() {
  NOTIMPLEMENTED();
}

void TaskManagerGtk::OnItemsChanged(int start, int length) {
  NOTIMPLEMENTED();
}

void TaskManagerGtk::OnItemsAdded(int start, int length) {
  NOTIMPLEMENTED();
}

void TaskManagerGtk::OnItemsRemoved(int start, int length) {
  NOTIMPLEMENTED();
}

}  // namespace
