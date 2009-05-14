// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager.h"

#include "base/stats_table.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/accelerator.h"
#include "views/background.h"
#include "views/controls/button/native_button.h"
#include "views/controls/link.h"
#include "views/controls/menu/menu.h"
#include "views/controls/table/group_table_view.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

// The task manager window default size.
static const int kDefaultWidth = 460;
static const int kDefaultHeight = 270;

// An id for the most important column, made sufficiently large so as not to
// collide with anything else.
static const int64 kNuthMagicNumber = 1737350766;
static const int kBitMask = 0x7FFFFFFF;
static const int kGoatsTeleportedColumn =
    (94024 * kNuthMagicNumber) & kBitMask;

namespace {

////////////////////////////////////////////////////////////////////////////////
// TaskManagerTableModel class
////////////////////////////////////////////////////////////////////////////////

class TaskManagerTableModel : public views::GroupTableModel,
                              public TaskManagerModelObserver {
 public:
  explicit TaskManagerTableModel(TaskManagerModel* model)
      : model_(model),
        observer_(NULL) {
    model->SetObserver(this);
  }
  ~TaskManagerTableModel() {}

  // GroupTableModel.
  int RowCount();
  std::wstring GetText(int row, int column);
  SkBitmap GetIcon(int row);
  void GetGroupRangeForItem(int item, views::GroupRange* range);
  void SetObserver(views::TableModelObserver* observer);
  virtual int CompareValues(int row1, int row2, int column_id);

  // TaskManagerModelObserver.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

 private:
  const TaskManagerModel* model_;
  views::TableModelObserver* observer_;
};

int TaskManagerTableModel::RowCount() {
  return model_->ResourceCount();
}

std::wstring TaskManagerTableModel::GetText(int row, int col_id) {
  switch (col_id) {
    case IDS_TASK_MANAGER_PAGE_COLUMN:  // Process
      return model_->GetResourceTitle(row);

    case IDS_TASK_MANAGER_NET_COLUMN:  // Net
      return model_->GetResourceNetworkUsage(row);

    case IDS_TASK_MANAGER_CPU_COLUMN:  // CPU
      if (!model_->IsResourceFirstInGroup(row))
        return std::wstring();
      return model_->GetResourceCPUUsage(row);

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return std::wstring();
      return model_->GetResourcePrivateMemory(row);

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return std::wstring();
      return model_->GetResourceSharedMemory(row);

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return std::wstring();
      return model_->GetResourcePhysicalMemory(row);

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      if (!model_->IsResourceFirstInGroup(row))
        return std::wstring();
      return model_->GetResourceProcessId(row);

    case kGoatsTeleportedColumn:  // Goats Teleported!
      return model_->GetResourceGoatsTeleported(row);

    default:
      return model_->GetResourceStatsValue(row, col_id);
  }
}

SkBitmap TaskManagerTableModel::GetIcon(int row) {
  return model_->GetResourceIcon(row);
}

void TaskManagerTableModel::GetGroupRangeForItem(int item,
                                                 views::GroupRange* range) {
  std::pair<int, int> range_pair = model_->GetGroupRangeForResource(item);
  range->start = range_pair.first;
  range->length = range_pair.second;
}

void TaskManagerTableModel::SetObserver(views::TableModelObserver* observer) {
  observer_ = observer;
}

int TaskManagerTableModel::CompareValues(int row1, int row2, int column_id) {
  return model_->CompareValues(row1, row2, column_id);
}

void TaskManagerTableModel::OnModelChanged() {
  if (observer_)
    observer_->OnModelChanged();
}

void TaskManagerTableModel::OnItemsChanged(int start, int length) {
  if (observer_)
    observer_->OnItemsChanged(start, length);
}

void TaskManagerTableModel::OnItemsAdded(int start, int length) {
  if (observer_)
    observer_->OnItemsAdded(start, length);
}

void TaskManagerTableModel::OnItemsRemoved(int start, int length) {
  if (observer_)
    observer_->OnItemsRemoved(start, length);
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerViewImpl class
//
// The view containing the different widgets.
//
////////////////////////////////////////////////////////////////////////////////

class TaskManagerViewImpl : public TaskManagerView,
                            public views::View,
                            public views::ButtonListener,
                            public views::DialogDelegate,
                            public views::TableViewObserver,
                            public views::LinkController,
                            public views::ContextMenuController,
                            public views::Menu::Delegate {
 public:
  TaskManagerViewImpl(TaskManager* task_manager,
                      TaskManagerModel* model);
  virtual ~TaskManagerViewImpl();

  void Init();
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

  // TaskManagerView
  virtual void GetSelection(std::vector<int>* selection);
  virtual void GetFocused(std::vector<int>* focused);
  virtual void OpenWindow();

  // ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender);

  // views::DialogDelegate
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetWindowName() const;
  virtual int GetDialogButtons() const;
  virtual void WindowClosing();
  virtual void DeleteDelegate();
  virtual views::View* GetContentsView();

  // views::TableViewObserver implementation.
  virtual void OnSelectionChanged();
  virtual void OnDoubleClick();
  virtual void OnKeyDown(unsigned short virtual_keycode);

  // views::LinkController implementation.
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Called by the column picker to pick up any new stat counters that
  // may have appeared since last time.
  void UpdateStatsCounters();

  // Menu::Delegate
  virtual void ShowContextMenu(views::View* source,
                               int x,
                               int y,
                               bool is_mouse_gesture);
  virtual bool IsItemChecked(int id) const;
  virtual void ExecuteCommand(int id);

 private:
  scoped_ptr<views::NativeButton> kill_button_;
  scoped_ptr<views::Link> about_memory_link_;
  views::GroupTableView* tab_table_;

  TaskManager* task_manager_;

  TaskManagerModel* model_;

  // all possible columns, not necessarily visible
  std::vector<views::TableColumn> columns_;

  scoped_ptr<TaskManagerTableModel> table_model_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerViewImpl);
};

TaskManagerViewImpl::TaskManagerViewImpl(TaskManager* task_manager,
                                         TaskManagerModel* model)
    : task_manager_(task_manager),
      model_(model) {
  Init();
}

TaskManagerViewImpl::~TaskManagerViewImpl() {
}

void TaskManagerViewImpl::Init() {
  table_model_.reset(new TaskManagerTableModel(model_));

  columns_.push_back(views::TableColumn(IDS_TASK_MANAGER_PAGE_COLUMN,
                                        views::TableColumn::LEFT, -1, 1));
  columns_.back().sortable = true;
  columns_.push_back(views::TableColumn(IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN,
                                        views::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(views::TableColumn(IDS_TASK_MANAGER_SHARED_MEM_COLUMN,
                                        views::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(views::TableColumn(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN,
                                        views::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(views::TableColumn(IDS_TASK_MANAGER_CPU_COLUMN,
                                        views::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(views::TableColumn(IDS_TASK_MANAGER_NET_COLUMN,
                                        views::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(views::TableColumn(IDS_TASK_MANAGER_PROCESS_ID_COLUMN,
                                        views::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;

  tab_table_ = new views::GroupTableView(table_model_.get(), columns_,
                                         views::ICON_AND_TEXT, false, true,
                                         true);

  // Hide some columns by default
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_PROCESS_ID_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_SHARED_MEM_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN, false);

  UpdateStatsCounters();
  views::TableColumn col(kGoatsTeleportedColumn, L"Goats Teleported",
                         views::TableColumn::RIGHT, -1, 0);
  col.sortable = true;
  columns_.push_back(col);
  tab_table_->AddColumn(col);
  tab_table_->SetObserver(this);
  SetContextMenuController(this);
  kill_button_.reset(new views::NativeButton(
      this, l10n_util::GetString(IDS_TASK_MANAGER_KILL)));
  kill_button_->AddAccelerator(views::Accelerator('E', false, false, false));
  kill_button_->SetAccessibleKeyboardShortcut(L"E");
  about_memory_link_.reset(new views::Link(
      l10n_util::GetString(IDS_TASK_MANAGER_ABOUT_MEMORY_LINK)));
  about_memory_link_->SetController(this);

  AddChildView(tab_table_);

  // Makes sure our state is consistent.
  OnSelectionChanged();
}

void TaskManagerViewImpl::UpdateStatsCounters() {
  StatsTable* stats = StatsTable::current();
  if (stats != NULL) {
    int max = stats->GetMaxCounters();
    // skip the first row (it's header data)
    for (int i = 1; i < max; i++) {
      const char* row = stats->GetRowName(i);
      if (row != NULL && row[0] != '\0' && !tab_table_->HasColumn(i)) {
        // TODO(erikkay): Use l10n to get display names for stats.  Right
        // now we're just displaying the internal counter name.  Perhaps
        // stat names not in the string table would be filtered out.
        // TODO(erikkay): Width is hard-coded right now, so many column
        // names are clipped.
        views::TableColumn col(i, ASCIIToWide(row), views::TableColumn::RIGHT,
                               90, 0);
        col.sortable = true;
        columns_.push_back(col);
        tab_table_->AddColumn(col);
      }
    }
  }
}

void TaskManagerViewImpl::ViewHierarchyChanged(bool is_add,
                                               views::View* parent,
                                               views::View* child) {
  // Since we want the Kill button and the Memory Details link to show up in
  // the same visual row as the close button, which is provided by the
  // framework, we must add the buttons to the non-client view, which is the
  // parent of this view. Similarly, when we're removed from the view
  // hierarchy, we must take care to clean up those items as well.
  if (child == this) {
    if (is_add) {
      parent->AddChildView(kill_button_.get());
      parent->AddChildView(about_memory_link_.get());
    } else {
      parent->RemoveChildView(kill_button_.get());
      parent->RemoveChildView(about_memory_link_.get());
      // Note that these items aren't deleted here, since this object is owned
      // by the TaskManager, whose lifetime surpasses the window, and the next
      // time we are inserted into a window these items will need to be valid.
    }
  }
}

void TaskManagerViewImpl::Layout() {
  // kPanelHorizMargin is too big.
  const int kTableButtonSpacing = 12;

  gfx::Size size = kill_button_->GetPreferredSize();
  int prefered_width = size.width();
  int prefered_height = size.height();

  tab_table_->SetBounds(x() + kPanelHorizMargin,
                        y() + kPanelVertMargin,
                        width() - 2 * kPanelHorizMargin,
                        height() - 2 * kPanelVertMargin - prefered_height);

  // y-coordinate of button top left.
  gfx::Rect parent_bounds = GetParent()->GetLocalBounds(false);
  int y_buttons = parent_bounds.bottom() - prefered_height - kButtonVEdgeMargin;

  kill_button_->SetBounds(x() + width() - prefered_width - kPanelHorizMargin,
                          y_buttons,
                          prefered_width,
                          prefered_height);

  size = about_memory_link_->GetPreferredSize();
  int link_prefered_width = size.width();
  int link_prefered_height = size.height();
  // center between the two buttons horizontally, and line up with
  // bottom of buttons vertically.
  int link_y_offset = std::max(0, prefered_height - link_prefered_height) / 2;
  about_memory_link_->SetBounds(
      x() + kPanelHorizMargin,
      y_buttons + prefered_height - link_prefered_height - link_y_offset,
      link_prefered_width,
      link_prefered_height);
}

gfx::Size TaskManagerViewImpl::GetPreferredSize() {
  return gfx::Size(kDefaultWidth, kDefaultHeight);
}

void TaskManagerViewImpl::GetSelection(std::vector<int>* selection) {
  DCHECK(selection);
  for (views::TableSelectionIterator iter  = tab_table_->SelectionBegin();
       iter != tab_table_->SelectionEnd(); ++iter) {
    // The TableView returns the selection starting from the end.
    selection->insert(selection->begin(), *iter);
  }
}

void TaskManagerViewImpl::GetFocused(std::vector<int>* focused) {
  DCHECK(focused);
  int row_count = tab_table_->RowCount();
  for (int i = 0; i < row_count; ++i) {
    // The TableView returns the selection starting from the end.
    if (tab_table_->ItemHasTheFocus(i))
      focused->insert(focused->begin(), i);
  }
}

void TaskManagerViewImpl::OpenWindow() {
  if (window()) {
    window()->Activate();
  } else {
    views::Window::CreateChromeWindow(NULL, gfx::Rect(), this);
    model_->StartUpdating();
    window()->Show();
  }
}

// ButtonListener implementation.
void TaskManagerViewImpl::ButtonPressed(views::Button* sender) {
  if (sender == kill_button_.get())
    task_manager_->KillSelectedProcesses();
}

// DialogDelegate implementation.
bool TaskManagerViewImpl::CanResize() const {
  return true;
}

bool TaskManagerViewImpl::CanMaximize() const {
  return true;
}

bool TaskManagerViewImpl::IsAlwaysOnTop() const {
  return true;
}

bool TaskManagerViewImpl::HasAlwaysOnTopMenu() const {
  return true;
};

std::wstring TaskManagerViewImpl::GetWindowTitle() const {
  return l10n_util::GetString(IDS_TASK_MANAGER_TITLE);
}

std::wstring TaskManagerViewImpl::GetWindowName() const {
  return prefs::kTaskManagerWindowPlacement;
}

int TaskManagerViewImpl::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_NONE;
}

void TaskManagerViewImpl::WindowClosing() {
  // Remove the view from its parent to trigger the contents'
  // ViewHierarchyChanged notification to unhook the extra buttons from the
  // non-client view.
  GetParent()->RemoveChildView(this);
  task_manager_->Close();
}

void TaskManagerViewImpl::DeleteDelegate() {
  ReleaseWindow();
}

views::View* TaskManagerViewImpl::GetContentsView() {
  return this;
}

// views::TableViewObserver implementation.
void TaskManagerViewImpl::OnSelectionChanged() {
  kill_button_->SetEnabled(!task_manager_->BrowserProcessIsSelected() &&
                           tab_table_->SelectedRowCount() > 0);
}

void TaskManagerViewImpl::OnDoubleClick() {
  task_manager_->ActivateFocusedTab();
}

void TaskManagerViewImpl::OnKeyDown(unsigned short virtual_keycode) {
  if (virtual_keycode == VK_RETURN)
    task_manager_->ActivateFocusedTab();
}

// views::LinkController implementation
void TaskManagerViewImpl::LinkActivated(views::Link* source,
                                        int event_flags) {
  DCHECK(source == about_memory_link_);
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  browser->OpenURL(GURL("about:memory"), GURL(), NEW_FOREGROUND_TAB,
                   PageTransition::LINK);
  // In case the browser window is minimzed, show it.
  browser->window()->Show();
}

void TaskManagerViewImpl::ShowContextMenu(views::View* source,
                                          int x,
                                          int y,
                                          bool is_mouse_gesture) {
  UpdateStatsCounters();
  scoped_ptr<views::Menu> menu(views::Menu::Create(
      this, views::Menu::TOPLEFT, source->GetWidget()->GetNativeView()));
  for (std::vector<views::TableColumn>::iterator i =
       columns_.begin(); i != columns_.end(); ++i) {
    menu->AppendMenuItem(i->id, i->title, views::Menu::CHECKBOX);
  }
  menu->RunMenuAt(x, y);
}

bool TaskManagerViewImpl::IsItemChecked(int id) const {
  return tab_table_->IsColumnVisible(id);
}

void TaskManagerViewImpl::ExecuteCommand(int id) {
  tab_table_->SetColumnVisibility(id, !tab_table_->IsColumnVisible(id));
}

}  // namespace

void TaskManager::Init() {
  view_.reset(new TaskManagerViewImpl(this, model_.get()));
}
