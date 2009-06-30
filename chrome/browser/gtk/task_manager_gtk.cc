// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/task_manager_gtk.h"

#include <vector>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "chrome/common/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// The task manager window default size.
const int kDefaultWidth = 460;
const int kDefaultHeight = 270;

// The resource id for the 'End process' button.
const gint kTaskManagerResponseKill = 1;

enum TaskManagerColumns {
  kTaskManagerPage,
  kTaskManagerPhysicalMem,
  kTaskManagerSharedMem,
  kTaskManagerPrivateMem,
  kTaskManagerCPU,
  kTaskManagerNetwork,
  kTaskManagerProcessID,
  kTaskManagerColumnCount,
};

void TreeViewColumnSetVisible(GtkWidget* treeview, TaskManagerColumns colid,
                              bool visible) {
  GtkTreeViewColumn* column = gtk_tree_view_get_column(GTK_TREE_VIEW(treeview),
                                                       colid);
  gtk_tree_view_column_set_visible(column, visible);
}

void TreeViewInsertColumn(GtkWidget* treeview,
                          TaskManagerColumns colid, int str) {
  GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1,
      l10n_util::GetStringUTF8(str).c_str(),
      renderer, "text", colid, NULL);
}

GtkWidget* CreateTaskManagerTreeview() {
  GtkWidget* treeview = gtk_tree_view_new();

  TreeViewInsertColumn(treeview, kTaskManagerPage,
                       IDS_TASK_MANAGER_PAGE_COLUMN);
  TreeViewInsertColumn(treeview, kTaskManagerPhysicalMem,
                       IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN);
  TreeViewInsertColumn(treeview, kTaskManagerSharedMem,
                       IDS_TASK_MANAGER_SHARED_MEM_COLUMN);
  TreeViewInsertColumn(treeview, kTaskManagerPrivateMem,
                       IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN);
  TreeViewInsertColumn(treeview, kTaskManagerCPU,
                       IDS_TASK_MANAGER_CPU_COLUMN);
  TreeViewInsertColumn(treeview, kTaskManagerNetwork,
                       IDS_TASK_MANAGER_NET_COLUMN);
  TreeViewInsertColumn(treeview, kTaskManagerProcessID,
                       IDS_TASK_MANAGER_PROCESS_ID_COLUMN);

  GtkListStore* store = gtk_list_store_new(kTaskManagerColumnCount,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT,
      G_TYPE_STRING, G_TYPE_UINT);

  gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(store));
  return treeview;
}

}  // namespace

TaskManagerGtk::TaskManagerGtk()
  : task_manager_(TaskManager::GetInstance()),
    model_(TaskManager::GetInstance()->model()),
    dialog_(NULL),
    treeview_(NULL) {
  Init();
}

// static
TaskManagerGtk* TaskManagerGtk::instance_ = NULL;

TaskManagerGtk::~TaskManagerGtk() {
}

void TaskManagerGtk::Init() {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_TASK_MANAGER_TITLE).c_str(),
      // Task Manager window is shared between all browsers.
      NULL,
      GTK_DIALOG_NO_SEPARATOR,
      l10n_util::GetStringUTF8(IDS_TASK_MANAGER_KILL).c_str(),
      kTaskManagerResponseKill,
      NULL);

  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  g_signal_connect(G_OBJECT(dialog_), "response", G_CALLBACK(OnResponse), this);

  treeview_ = CreateTaskManagerTreeview();
  gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(treeview_), TRUE);
  gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(treeview_),
                               GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);

  // Hide some columns by default
  TreeViewColumnSetVisible(treeview_, kTaskManagerSharedMem, false);
  TreeViewColumnSetVisible(treeview_, kTaskManagerPrivateMem, false);
  TreeViewColumnSetVisible(treeview_, kTaskManagerProcessID, false);

  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), treeview_);

  gtk_window_resize(GTK_WINDOW(dialog_), kDefaultWidth, kDefaultHeight);
  gtk_widget_show_all(dialog_);

  // TODO(jhawkins): mode_->SetObserver() and implement OnItems*.
}

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

// static
void TaskManagerGtk::OnResponse(GtkDialog* dialog, gint response_id,
                                TaskManagerGtk* task_manager) {
  if (response_id == GTK_RESPONSE_DELETE_EVENT) {
    instance_ = NULL;
    task_manager->task_manager_->OnWindowClosed();
    delete task_manager;
  }
}

// static
void TaskManagerGtk::Show() {
  if (instance_) {
    // If there's a Task manager window open already, just activate it.
    gtk_window_present(GTK_WINDOW(instance_->dialog_));
  } else {
    instance_ = new TaskManagerGtk;
    instance_->model_->StartUpdating();
  }
}
