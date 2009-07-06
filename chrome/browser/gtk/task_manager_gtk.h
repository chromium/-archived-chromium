// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TASK_MANAGER_GTK_H_
#define CHROME_BROWSER_GTK_TASK_MANAGER_GTK_H_

#include <gtk/gtk.h>

#include <string>

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

  // Creates the task manager if it doesn't exist; otherwise, it activates the
  // existing task manager window.
  static void Show();

 private:
  class ContextMenuController;
  friend class ContextMenuController;

  // Initializes the task manager dialog.
  void Init();

  // Sets up the treeview widget.
  void CreateTaskManagerTreeview();

  // Returns the model data for a given |row| and |col_id|.
  std::string GetModelText(int row, int col_id);

  // Retrieves the resource icon from the model for |row|.
  GdkPixbuf* GetModelIcon(int row);

  // Sets the treeview row data.  |row| is an index into the model and |iter|
  // is the current position in the treeview.
  void SetRowDataFromModel(int row, GtkTreeIter* iter);

  // Queries the treeview for the selected rows, and kills those processes.
  void KillSelectedProcesses();

  // Opens the context menu used to select the task manager columns.
  void ShowContextMenu();

  // Activates the tab associated with the focused row.
  void ActivateFocusedTab();

  // response signal handler that notifies us of dialog responses.
  static void OnResponse(GtkDialog* dialog, gint response_id,
                         TaskManagerGtk* task_manager);

  // changed signal handler that is sent when the treeview selection changes.
  static void OnSelectionChanged(GtkTreeSelection* selection,
                                 TaskManagerGtk* task_manager);

  // button-press-event handler that activates a process on double-click.
  static gboolean OnButtonPressEvent(GtkWidget* widget, GdkEventButton* event,
                                     TaskManagerGtk* task_manager);

  // button-release-event handler that opens the right-click context menu.
  static gboolean OnButtonReleaseEvent(GtkWidget* widget, GdkEventButton* event,
                                       TaskManagerGtk* task_manager);

  // The task manager.
  TaskManager* task_manager_;

  // Our model.
  TaskManagerModel* model_;

  // The task manager dialog window.
  GtkWidget* dialog_;

  // The treeview that contains the process list.
  GtkWidget* treeview_;

  // The list of processes.
  GtkListStore* process_list_;

  // The number of processes in |process_list_|.
  int process_count_;

  // The context menu controller.
  scoped_ptr<ContextMenuController> menu_controller_;

  // An open task manager window. There can only be one open at a time. This
  // is reset to NULL when the window is closed.
  static TaskManagerGtk* instance_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerGtk);
};

#endif  // CHROME_BROWSER_GTK_TASK_MANAGER_GTK_H_
