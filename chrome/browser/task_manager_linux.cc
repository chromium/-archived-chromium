// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager.h"

#include <vector>

#include "base/logging.h"

namespace {

class TaskManagerViewImpl : public TaskManagerView,
                            public TaskManagerModelObserver {
 public:
  TaskManagerViewImpl(TaskManagerModel* model) {
    model->SetObserver(this);
  }

  // TaskManagerView
  virtual void GetSelection(std::vector<int>* selection);
  virtual void GetFocused(std::vector<int>* focused);
  virtual void OpenWindow();
  virtual void ActivateWindow();
  virtual void CloseWindow();

  // TaskManagerModelObserver
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);
};

void TaskManagerViewImpl::GetSelection(std::vector<int>* selection) {
  NOTIMPLEMENTED();
}

void TaskManagerViewImpl::GetFocused(std::vector<int>* focused) {
  NOTIMPLEMENTED();
}

void TaskManagerViewImpl::OpenWindow() {
  NOTIMPLEMENTED();
}

void TaskManagerViewImpl::ActivateWindow() {
  NOTIMPLEMENTED();
}

void TaskManagerViewImpl::CloseWindow() {
  NOTIMPLEMENTED();
}

void TaskManagerViewImpl::OnModelChanged() {
  NOTIMPLEMENTED();
}

void TaskManagerViewImpl::OnItemsChanged(int start, int length) {
  NOTIMPLEMENTED();
}

void TaskManagerViewImpl::OnItemsAdded(int start, int length) {
  NOTIMPLEMENTED();
}

void TaskManagerViewImpl::OnItemsRemoved(int start, int length) {
  NOTIMPLEMENTED();
}

}  // namespace

void TaskManager::CreateView() {
  DCHECK(!view_);
  view_= new TaskManagerViewImpl(model_.get());
}
