// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/task_manager_gtk.h"

#include <vector>

#include "app/l10n_util.h"
#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/common/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// The task manager window default size.
const int kDefaultWidth = 460;
const int kDefaultHeight = 270;

// The resource id for the 'End process' button.
const gint kTaskManagerResponseKill = 1;

enum TaskManagerColumn {
  kTaskManagerIcon,
  kTaskManagerPage,
  kTaskManagerPhysicalMem,
  kTaskManagerSharedMem,
  kTaskManagerPrivateMem,
  kTaskManagerCPU,
  kTaskManagerNetwork,
  kTaskManagerProcessID,
  kTaskManagerGoatsTeleported,
  kTaskManagerColumnCount,
};

TaskManagerColumn TaskManagerResourceIDToColumnID(int id) {
  switch (id) {
    case IDS_TASK_MANAGER_PAGE_COLUMN:
      return kTaskManagerPage;
    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:
      return kTaskManagerPhysicalMem;
    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:
      return kTaskManagerSharedMem;
    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:
      return kTaskManagerPrivateMem;
    case IDS_TASK_MANAGER_CPU_COLUMN:
      return kTaskManagerCPU;
    case IDS_TASK_MANAGER_NET_COLUMN:
      return kTaskManagerNetwork;
    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      return kTaskManagerProcessID;
    default:
      NOTREACHED();
      return static_cast<TaskManagerColumn>(-1);
  }
}

int TaskManagerColumnIDToResourceID(int id) {
  printf("id: %d\n", id);
  switch (id) {
    case kTaskManagerPage:
      return IDS_TASK_MANAGER_PAGE_COLUMN;
    case kTaskManagerPhysicalMem:
      return IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN;
    case kTaskManagerSharedMem:
      return IDS_TASK_MANAGER_SHARED_MEM_COLUMN;
    case kTaskManagerPrivateMem:
      return IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN;
    case kTaskManagerCPU:
      return IDS_TASK_MANAGER_CPU_COLUMN;
    case kTaskManagerNetwork:
      return IDS_TASK_MANAGER_NET_COLUMN;
    case kTaskManagerProcessID:
      return IDS_TASK_MANAGER_PROCESS_ID_COLUMN;
    default:
      NOTREACHED();
      return -1;
  }
}

// Should be used for all gtk_tree_view functions that require a column index on
// input.
//
// We need colid - 1 because the gtk_tree_view function is asking for the
// column index, not the column id, and both kTaskManagerIcon and
// kTaskManagerPage are in the same column index, so all column IDs are off by
// one.
int TreeViewColumnIndexFromID(TaskManagerColumn colid) {
  return colid - 1;
}

// Shows or hides a treeview column.
void TreeViewColumnSetVisible(GtkWidget* treeview, TaskManagerColumn colid,
                              bool visible) {
  GtkTreeViewColumn* column = gtk_tree_view_get_column(
      GTK_TREE_VIEW(treeview), TreeViewColumnIndexFromID(colid));
  gtk_tree_view_column_set_visible(column, visible);
}

bool TreeViewColumnIsVisible(GtkWidget* treeview, TaskManagerColumn colid) {
  GtkTreeViewColumn* column = gtk_tree_view_get_column(
      GTK_TREE_VIEW(treeview), TreeViewColumnIndexFromID(colid));
  return gtk_tree_view_column_get_visible(column);
}

void TreeViewInsertColumnWithPixbuf(GtkWidget* treeview, int resid) {
  int colid = TaskManagerResourceIDToColumnID(resid);
  GtkTreeViewColumn* column = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(column,
                                 l10n_util::GetStringUTF8(resid).c_str());
  GtkCellRenderer* image_renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(column, image_renderer, FALSE);
  gtk_tree_view_column_add_attribute(column, image_renderer,
                                     "pixbuf", kTaskManagerIcon);
  GtkCellRenderer* text_renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
  gtk_tree_view_column_add_attribute(column, text_renderer, "text", colid);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
}

// Inserts a column with a column id of |colid| and |name|.
void TreeViewInsertColumnWithName(GtkWidget* treeview,
                                  TaskManagerColumn colid, const char* name) {
  GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1,
                                              name, renderer,
                                              "text", colid,
                                              NULL);
  GtkTreeViewColumn* column = gtk_tree_view_get_column(
      GTK_TREE_VIEW(treeview), TreeViewColumnIndexFromID(colid));
  gtk_tree_view_column_set_resizable(column, TRUE);
}

// Loads the column name from |resid| and uses the corresponding
// TaskManagerColumn value as the column id to insert into the treeview.
void TreeViewInsertColumn(GtkWidget* treeview, int resid) {
  TreeViewInsertColumnWithName(treeview, TaskManagerResourceIDToColumnID(resid),
                               l10n_util::GetStringUTF8(resid).c_str());
}

// Get the row number corresponding to |path|.
gint GetRowNumForPath(GtkTreePath* path) {
  gint* indices = gtk_tree_path_get_indices(path);
  if (!indices) {
    NOTREACHED();
    return -1;
  }
  return indices[0];
}

}  // namespace

class TaskManagerGtk::ContextMenuController : public MenuGtk::Delegate {
 public:
  explicit ContextMenuController(TaskManagerGtk* task_manager)
      : task_manager_(task_manager) {
    menu_.reset(new MenuGtk(this, false));
    for (int i = kTaskManagerPage; i < kTaskManagerColumnCount - 1; i++) {
      menu_->AppendCheckMenuItemWithLabel(
          i, l10n_util::GetStringUTF8(TaskManagerColumnIDToResourceID(i)));
    }

    menu_->AppendCheckMenuItemWithLabel(kTaskManagerGoatsTeleported,
                                        "Goats Teleported");
  }

  virtual ~ContextMenuController() {}

  void RunMenu() {
    menu_->PopupAsContext(gtk_get_current_event_time());
  }

  void Cancel() {
    task_manager_ = NULL;
    menu_->Cancel();
  }

 private:
  // MenuGtk::Delegate implementation:
  virtual bool IsCommandEnabled(int command_id) const {
    if (!task_manager_)
      return false;

    return true;
  }

  virtual bool IsItemChecked(int command_id) const {
    if (!task_manager_)
      return false;

    TaskManagerColumn colid = static_cast<TaskManagerColumn>(command_id);
    return TreeViewColumnIsVisible(task_manager_->treeview_, colid);
  }

  virtual void ExecuteCommand(int command_id) {
    if (!task_manager_)
      return;

    TaskManagerColumn colid = static_cast<TaskManagerColumn>(command_id);
    bool visible = !TreeViewColumnIsVisible(task_manager_->treeview_, colid);
    TreeViewColumnSetVisible(task_manager_->treeview_, colid, visible);
  }

  // The context menu.
  scoped_ptr<MenuGtk> menu_;

  // The TaskManager the context menu was brought up for. Set to NULL when the
  // menu is canceled.
  TaskManagerGtk* task_manager_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuController);
};

TaskManagerGtk::TaskManagerGtk()
  : task_manager_(TaskManager::GetInstance()),
    model_(TaskManager::GetInstance()->model()),
    dialog_(NULL),
    treeview_(NULL),
    process_list_(NULL),
    process_count_(0) {
  Init();
}

// static
TaskManagerGtk* TaskManagerGtk::instance_ = NULL;

TaskManagerGtk::~TaskManagerGtk() {
  task_manager_->OnWindowClosed();
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerGtk, TaskManagerModelObserver implementation:

void TaskManagerGtk::OnModelChanged() {
  // Nothing to do.
}

void TaskManagerGtk::OnItemsChanged(int start, int length) {
  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(process_list_), &iter,
                                NULL, start);

  for (int i = start; i < start + length; i++) {
    SetRowDataFromModel(i, &iter);
    gtk_tree_model_iter_next(GTK_TREE_MODEL(process_list_), &iter);
  }
}

void TaskManagerGtk::OnItemsAdded(int start, int length) {
  GtkTreeIter iter;
  if (start == 0) {
    gtk_list_store_prepend(process_list_, &iter);
  } else if (start >= process_count_) {
    gtk_list_store_append(process_list_, &iter);
  } else {
    GtkTreeIter sibling;
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(process_list_), &sibling,
                                  NULL, start);
    gtk_list_store_insert_before(process_list_, &iter, &sibling);
  }

  SetRowDataFromModel(start, &iter);

  for (int i = start + 1; i < start + length; i++) {
    gtk_list_store_insert_after(process_list_, &iter, &iter);
    SetRowDataFromModel(i, &iter);
  }

  process_count_ += length;
}

void TaskManagerGtk::OnItemsRemoved(int start, int length) {
  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(process_list_), &iter,
                                NULL, start);

  for (int i = 0; i < length; i++) {
    // |iter| is moved to the next valid node when the current node is removed.
    gtk_list_store_remove(process_list_, &iter);
  }

  process_count_ -= length;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerGtk, public:

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

////////////////////////////////////////////////////////////////////////////////
// TaskManagerGtk, private:

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
  g_signal_connect(G_OBJECT(dialog_), "button-release-event",
                   G_CALLBACK(OnButtonReleaseEvent), this);
  gtk_widget_add_events(dialog_,
                        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

  CreateTaskManagerTreeview();
  gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(treeview_), TRUE);
  gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(treeview_),
                               GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
  g_signal_connect(G_OBJECT(treeview_), "button-release-event",
                   G_CALLBACK(OnButtonReleaseEvent), this);
  gtk_widget_add_events(treeview_,
                        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

  // Hide some columns by default
  TreeViewColumnSetVisible(treeview_, kTaskManagerSharedMem, false);
  TreeViewColumnSetVisible(treeview_, kTaskManagerPrivateMem, false);
  TreeViewColumnSetVisible(treeview_, kTaskManagerProcessID, false);
  TreeViewColumnSetVisible(treeview_, kTaskManagerGoatsTeleported, false);

  // |selection| is owned by |treeview_|.
  GtkTreeSelection* selection = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(treeview_));
  gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
  g_signal_connect(G_OBJECT(selection), "changed",
                   G_CALLBACK(OnSelectionChanged), this);

  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), treeview_);

  gtk_window_resize(GTK_WINDOW(dialog_), kDefaultWidth, kDefaultHeight);
  gtk_widget_show_all(dialog_);

  model_->SetObserver(this);
}

void TaskManagerGtk::CreateTaskManagerTreeview() {
  treeview_ = gtk_tree_view_new();

  TreeViewInsertColumnWithPixbuf(treeview_, IDS_TASK_MANAGER_PAGE_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_SHARED_MEM_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_CPU_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_NET_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_PROCESS_ID_COLUMN);

  TreeViewInsertColumnWithName(treeview_, kTaskManagerGoatsTeleported,
                               "Goats Teleported");

  process_list_ = gtk_list_store_new(kTaskManagerColumnCount,
      GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
      G_TYPE_STRING);

  gtk_tree_view_set_model(GTK_TREE_VIEW(treeview_),
                          GTK_TREE_MODEL(process_list_));
  g_object_unref(process_list_);
}

std::string TaskManagerGtk::GetModelText(int row, int col_id) {
  switch (col_id) {
    case IDS_TASK_MANAGER_PAGE_COLUMN:  // Process
      return WideToUTF8(model_->GetResourceTitle(row));

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourcePrivateMemory(row));

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourceSharedMemory(row));

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourcePhysicalMemory(row));

    case IDS_TASK_MANAGER_CPU_COLUMN:  // CPU
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourceCPUUsage(row));

    case IDS_TASK_MANAGER_NET_COLUMN:  // Net
      return WideToUTF8(model_->GetResourceNetworkUsage(row));

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:  // Process ID
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourceProcessId(row));

    case kTaskManagerGoatsTeleported:  // Goats Teleported!
      return WideToUTF8(model_->GetResourceGoatsTeleported(row));

    default:
      return WideToUTF8(model_->GetResourceStatsValue(row, col_id));
  }
}

GdkPixbuf* TaskManagerGtk::GetModelIcon(int row) {
  SkBitmap icon = model_->GetResourceIcon(row);
  return gfx::GdkPixbufFromSkBitmap(&icon);
}

void TaskManagerGtk::SetRowDataFromModel(int row, GtkTreeIter* iter) {
  GdkPixbuf* icon = GetModelIcon(row);
  std::string page = GetModelText(row, IDS_TASK_MANAGER_PAGE_COLUMN);
  std::string phys_mem = GetModelText(
      row, IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN);
  std::string shared_mem = GetModelText(
      row, IDS_TASK_MANAGER_SHARED_MEM_COLUMN);
  std::string priv_mem = GetModelText(row, IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN);
  std::string cpu = GetModelText(row, IDS_TASK_MANAGER_CPU_COLUMN);
  std::string net = GetModelText(row, IDS_TASK_MANAGER_NET_COLUMN);
  std::string procid = GetModelText(row, IDS_TASK_MANAGER_PROCESS_ID_COLUMN);
  std::string goats = GetModelText(row, kTaskManagerGoatsTeleported);
  gtk_list_store_set(process_list_, iter,
                     kTaskManagerIcon, icon,
                     kTaskManagerPage, page.c_str(),
                     kTaskManagerPhysicalMem, phys_mem.c_str(),
                     kTaskManagerSharedMem, shared_mem.c_str(),
                     kTaskManagerPrivateMem, priv_mem.c_str(),
                     kTaskManagerCPU, cpu.c_str(),
                     kTaskManagerNetwork, net.c_str(),
                     kTaskManagerProcessID, procid.c_str(),
                     kTaskManagerGoatsTeleported, goats.c_str(),
                     -1);
  g_object_unref(icon);
}

void TaskManagerGtk::KillSelectedProcesses() {
  GtkTreeSelection* selection = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(treeview_));

  GtkTreeModel* model;
  GList* paths = gtk_tree_selection_get_selected_rows(selection, &model);
  for (GList* item = paths; item; item = item->next) {
    int row = GetRowNumForPath(reinterpret_cast<GtkTreePath*>(item->data));
    task_manager_->KillProcess(row);
  }
  g_list_free(paths);
}

void TaskManagerGtk::ShowContextMenu() {
  if (!menu_controller_.get())
    menu_controller_.reset(new ContextMenuController(this));

  menu_controller_->RunMenu();
}

// static
void TaskManagerGtk::OnResponse(GtkDialog* dialog, gint response_id,
                                TaskManagerGtk* task_manager) {
  if (response_id == GTK_RESPONSE_DELETE_EVENT) {
    instance_ = NULL;
    delete task_manager;
  } else if (response_id == kTaskManagerResponseKill) {
    task_manager->KillSelectedProcesses();
  }
}

// static
void TaskManagerGtk::OnSelectionChanged(GtkTreeSelection* selection,
                                        TaskManagerGtk* task_manager) {
  bool selection_contains_browser_process = false;

  GtkTreeModel* model;
  GList* paths = gtk_tree_selection_get_selected_rows(selection, &model);
  for (GList* item = paths; item; item = item->next) {
    int row = GetRowNumForPath(reinterpret_cast<GtkTreePath*>(item->data));
    if (task_manager->task_manager_->IsBrowserProcess(row)) {
      selection_contains_browser_process = true;
      break;
    }
  }
  g_list_free(paths);

  bool sensitive = (paths != NULL) && !selection_contains_browser_process;
  gtk_dialog_set_response_sensitive(GTK_DIALOG(task_manager->dialog_),
                                    kTaskManagerResponseKill, sensitive);
}

// static
gboolean TaskManagerGtk::OnButtonReleaseEvent(GtkWidget* widget,
                                              GdkEventButton* event,
                                              TaskManagerGtk* task_manager) {
  // We don't want to open the context menu in the treeview.
  if (widget == task_manager->treeview_)
    return TRUE;

  if (event->button == 3)
    task_manager->ShowContextMenu();

  return FALSE;
}
