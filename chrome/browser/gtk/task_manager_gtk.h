// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TASK_MANAGER_GTK_H_
#define CHROME_BROWSER_GTK_TASK_MANAGER_GTK_H_

#include "chrome/browser/task_manager.h"

class TaskManagerGtk : public TaskManagerModelObserver {
 public:
  TaskManagerGtk();
  virtual ~TaskManagerGtk();

  // TaskManagerModelObserver
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

  void Init();

  // response signal handler that notifies us of dialog responses.
  static void OnResponse(GtkDialog* dialog, gint response_id,
                         TaskManagerGtk* task_manager);

  // Creates the task manager if it doesn't exist; otherwise, it activates the
  // existing task manager window.
  static void Show();

 private:
  // The task manager.
  TaskManager* task_manager_;

  // Our model.
  TaskManagerModel* model_;

  // The task manager dialog window.
  GtkWidget* dialog_;

  // The treeview that contains the process list.
  GtkWidget* treeview_;

  // An open task manager window. There can only be one open at a time. This
  // is reset to NULL when the window is closed.
  static TaskManagerGtk* instance_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerGtk);
};

#endif  // CHROME_BROWSER_GTK_TASK_MANAGER_GTK_H_
