// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager.h"

#include "app/l10n_util.h"
#include "app/table_model_observer.h"
#include "base/stats_table.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/views/browser_dialogs.h"
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
#include "views/controls/table/table_view_observer.h"
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
  void SetObserver(TableModelObserver* observer);
  virtual int CompareValues(int row1, int row2, int column_id);

  // TaskManagerModelObserver.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

 private:
  const TaskManagerModel* model_;
  TableModelObserver* observer_;
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

void TaskManagerTableModel::SetObserver(TableModelObserver* observer) {
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

// The Task manager UI container.
class TaskManagerView : public views::View,
                        public views::ButtonListener,
                        public views::DialogDelegate,
                        public views::TableViewObserver,
                        public views::LinkController,
                        public views::ContextMenuController,
                        public views::Menu::Delegate {
 public:
  TaskManagerView();
  virtual ~TaskManagerView();

  // Shows the Task manager window, or re-activates an existing one.
  static void Show();

  // views::View
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

  // ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender);

  // views::DialogDelegate
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool ExecuteWindowsCommand(int command_id);
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
  // Creates the child controls.
  void Init();

  // Initializes the state of the always-on-top setting as the window is shown.
  void InitAlwaysOnTopState();

  // Activates the tab associated with the focused row.
  void ActivateFocusedTab();

  // Adds an always on top item to the window's system menu.
  void AddAlwaysOnTopSystemMenuItem();

  // Restores saved always on top state from a previous session.
  bool GetSavedAlwaysOnTopState(bool* always_on_top) const;

  views::NativeButton* kill_button_;
  views::Link* about_memory_link_;
  views::GroupTableView* tab_table_;

  TaskManager* task_manager_;

  TaskManagerModel* model_;

  // all possible columns, not necessarily visible
  std::vector<TableColumn> columns_;

  scoped_ptr<TaskManagerTableModel> table_model_;

  // True when the Task Manager window should be shown on top of other windows.
  bool is_always_on_top_;

  // We need to own the text of the menu, the Windows API does not copy it.
  std::wstring always_on_top_menu_text_;

  // An open Task manager window. There can only be one open at a time. This
  // is reset to NULL when the window is closed.
  static TaskManagerView* instance_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerView);
};

// static
TaskManagerView* TaskManagerView::instance_ = NULL;


TaskManagerView::TaskManagerView()
    : task_manager_(TaskManager::GetInstance()),
      model_(TaskManager::GetInstance()->model()),
      is_always_on_top_(false) {
  Init();
}

TaskManagerView::~TaskManagerView() {
  // Delete child views now, while our table model still exists.
  RemoveAllChildViews(true);
}

void TaskManagerView::Init() {
  table_model_.reset(new TaskManagerTableModel(model_));

  columns_.push_back(TableColumn(IDS_TASK_MANAGER_PAGE_COLUMN,
                                 TableColumn::LEFT, -1, 1));
  columns_.back().sortable = true;
  columns_.push_back(TableColumn(IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN,
                                 TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(TableColumn(IDS_TASK_MANAGER_SHARED_MEM_COLUMN,
                                 TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(TableColumn(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN,
                                 TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(TableColumn(IDS_TASK_MANAGER_CPU_COLUMN,
                                 TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(TableColumn(IDS_TASK_MANAGER_NET_COLUMN,
                                 TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(TableColumn(IDS_TASK_MANAGER_PROCESS_ID_COLUMN,
                                 TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;

  tab_table_ = new views::GroupTableView(table_model_.get(), columns_,
                                         views::ICON_AND_TEXT, false, true,
                                         true);

  // Hide some columns by default
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_PROCESS_ID_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_SHARED_MEM_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN, false);

  UpdateStatsCounters();
  TableColumn col(kGoatsTeleportedColumn, L"Goats Teleported",
                  TableColumn::RIGHT, -1, 0);
  col.sortable = true;
  columns_.push_back(col);
  tab_table_->AddColumn(col);
  tab_table_->SetObserver(this);
  SetContextMenuController(this);
  kill_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_TASK_MANAGER_KILL));
  kill_button_->AddAccelerator(views::Accelerator('E', false, false, false));
  kill_button_->SetAccessibleKeyboardShortcut(L"E");
  about_memory_link_ = new views::Link(
      l10n_util::GetString(IDS_TASK_MANAGER_ABOUT_MEMORY_LINK));
  about_memory_link_->SetController(this);

  // Makes sure our state is consistent.
  OnSelectionChanged();
}

void TaskManagerView::UpdateStatsCounters() {
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
        TableColumn col(i, ASCIIToWide(row), TableColumn::RIGHT, 90, 0);
        col.sortable = true;
        columns_.push_back(col);
        tab_table_->AddColumn(col);
      }
    }
  }
}

void TaskManagerView::ViewHierarchyChanged(bool is_add,
                                           views::View* parent,
                                           views::View* child) {
  // Since we want the Kill button and the Memory Details link to show up in
  // the same visual row as the close button, which is provided by the
  // framework, we must add the buttons to the non-client view, which is the
  // parent of this view. Similarly, when we're removed from the view
  // hierarchy, we must take care to clean up those items as well.
  if (child == this) {
    if (is_add) {
      parent->AddChildView(about_memory_link_);
      parent->AddChildView(kill_button_);
      AddChildView(tab_table_);
    } else {
      parent->RemoveChildView(kill_button_);
      parent->RemoveChildView(about_memory_link_);
    }
  }
}

void TaskManagerView::Layout() {
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

gfx::Size TaskManagerView::GetPreferredSize() {
  return gfx::Size(kDefaultWidth, kDefaultHeight);
}

// static
void TaskManagerView::Show() {
  if (instance_) {
    // If there's a Task manager window open already, just activate it.
    instance_->window()->Activate();
  } else {
    instance_ = new TaskManagerView;
    views::Window::CreateChromeWindow(NULL, gfx::Rect(), instance_);
    instance_->InitAlwaysOnTopState();
    instance_->model_->StartUpdating();
    instance_->window()->Show();
  }
}

// ButtonListener implementation.
void TaskManagerView::ButtonPressed(views::Button* sender) {
  DCHECK(sender == kill_button_);
  for (views::TableSelectionIterator iter  = tab_table_->SelectionBegin();
       iter != tab_table_->SelectionEnd(); ++iter) {
    task_manager_->KillProcess(*iter);
  }
}

// DialogDelegate implementation.
bool TaskManagerView::CanResize() const {
  return true;
}

bool TaskManagerView::CanMaximize() const {
  return true;
}

bool TaskManagerView::ExecuteWindowsCommand(int command_id) {
  if (command_id == IDC_ALWAYS_ON_TOP) {
    is_always_on_top_ = !is_always_on_top_;

    // Change the menu check state.
    HMENU system_menu = GetSystemMenu(GetWindow()->GetNativeWindow(), FALSE);
    MENUITEMINFO menu_info;
    memset(&menu_info, 0, sizeof(MENUITEMINFO));
    menu_info.cbSize = sizeof(MENUITEMINFO);
    BOOL r = GetMenuItemInfo(system_menu, IDC_ALWAYS_ON_TOP,
                             FALSE, &menu_info);
    DCHECK(r);
    menu_info.fMask = MIIM_STATE;
    if (is_always_on_top_)
      menu_info.fState = MFS_CHECKED;
    r = SetMenuItemInfo(system_menu, IDC_ALWAYS_ON_TOP, FALSE, &menu_info);

    // Now change the actual window's behavior.
    window()->SetIsAlwaysOnTop(is_always_on_top_);

    // Save the state.
    if (g_browser_process->local_state()) {
      DictionaryValue* window_preferences =
          g_browser_process->local_state()->GetMutableDictionary(
              GetWindowName().c_str());
      window_preferences->SetBoolean(L"always_on_top", is_always_on_top_);
    }
    return true;
  }
  return false;
}

std::wstring TaskManagerView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_TASK_MANAGER_TITLE);
}

std::wstring TaskManagerView::GetWindowName() const {
  return prefs::kTaskManagerWindowPlacement;
}

int TaskManagerView::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_NONE;
}

void TaskManagerView::WindowClosing() {
  // Now that the window is closed, we can allow a new one to be opened.
  instance_ = NULL;
  task_manager_->OnWindowClosed();
}

void TaskManagerView::DeleteDelegate() {
  ReleaseWindow();
}

views::View* TaskManagerView::GetContentsView() {
  return this;
}

// views::TableViewObserver implementation.
void TaskManagerView::OnSelectionChanged() {
  bool selection_contains_browser_process = false;
  for (views::TableSelectionIterator iter  = tab_table_->SelectionBegin();
       iter != tab_table_->SelectionEnd(); ++iter) {
    if (task_manager_->IsBrowserProcess(*iter)) {
      selection_contains_browser_process = true;
      break;
    }
  }
  kill_button_->SetEnabled(!selection_contains_browser_process &&
                           tab_table_->SelectedRowCount() > 0);
}

void TaskManagerView::OnDoubleClick() {
  ActivateFocusedTab();
}

void TaskManagerView::OnKeyDown(unsigned short virtual_keycode) {
  if (virtual_keycode == VK_RETURN)
    ActivateFocusedTab();
}

// views::LinkController implementation
void TaskManagerView::LinkActivated(views::Link* source, int event_flags) {
  DCHECK(source == about_memory_link_);
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  browser->OpenURL(GURL("about:memory"), GURL(), NEW_FOREGROUND_TAB,
                   PageTransition::LINK);
  // In case the browser window is minimzed, show it. If this is an application
  // or popup, we can only have one tab, hence we need to process this in a
  // tabbed browser window. Currently, |browser| is pointing to the application,
  // popup window. Therefore, we have to retrieve the last active tab again,
  // since a new window has been used.
  if (browser->type() & Browser::TYPE_APP_POPUP) {
    browser = BrowserList::GetLastActive();
    DCHECK(browser);
  }
  browser->window()->Show();
}

void TaskManagerView::ShowContextMenu(views::View* source, int x, int y,
                                      bool is_mouse_gesture) {
  UpdateStatsCounters();
  scoped_ptr<views::Menu> menu(views::Menu::Create(
      this, views::Menu::TOPLEFT, source->GetWidget()->GetNativeView()));
  for (std::vector<TableColumn>::iterator i =
       columns_.begin(); i != columns_.end(); ++i) {
    menu->AppendMenuItem(i->id, i->title, views::Menu::CHECKBOX);
  }
  menu->RunMenuAt(x, y);
}

bool TaskManagerView::IsItemChecked(int id) const {
  return tab_table_->IsColumnVisible(id);
}

void TaskManagerView::ExecuteCommand(int id) {
  tab_table_->SetColumnVisibility(id, !tab_table_->IsColumnVisible(id));
}

void TaskManagerView::InitAlwaysOnTopState() {
  is_always_on_top_ = false;
  if (GetSavedAlwaysOnTopState(&is_always_on_top_))
    window()->SetIsAlwaysOnTop(is_always_on_top_);
  AddAlwaysOnTopSystemMenuItem();
}

void TaskManagerView::ActivateFocusedTab() {
  int row_count = tab_table_->RowCount();
  for (int i = 0; i < row_count; ++i) {
    if (tab_table_->ItemHasTheFocus(i)) {
      task_manager_->ActivateProcess(i);
      break;
    }
  }
}

void TaskManagerView::AddAlwaysOnTopSystemMenuItem() {
  // The Win32 API requires that we own the text.
  always_on_top_menu_text_ = l10n_util::GetString(IDS_ALWAYS_ON_TOP);

  // Let's insert a menu to the window.
  HMENU system_menu = ::GetSystemMenu(GetWindow()->GetNativeWindow(), FALSE);
  int index = ::GetMenuItemCount(system_menu) - 1;
  if (index < 0) {
    // Paranoia check.
    NOTREACHED();
    index = 0;
  }
  // First we add the separator.
  MENUITEMINFO menu_info;
  memset(&menu_info, 0, sizeof(MENUITEMINFO));
  menu_info.cbSize = sizeof(MENUITEMINFO);
  menu_info.fMask = MIIM_FTYPE;
  menu_info.fType = MFT_SEPARATOR;
  ::InsertMenuItem(system_menu, index, TRUE, &menu_info);

  // Then the actual menu.
  menu_info.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING | MIIM_STATE;
  menu_info.fType = MFT_STRING;
  menu_info.fState = MFS_ENABLED;
  if (is_always_on_top_)
    menu_info.fState |= MFS_CHECKED;
  menu_info.wID = IDC_ALWAYS_ON_TOP;
  menu_info.dwTypeData = const_cast<wchar_t*>(always_on_top_menu_text_.c_str());
  ::InsertMenuItem(system_menu, index, TRUE, &menu_info);
}

bool TaskManagerView::GetSavedAlwaysOnTopState(bool* always_on_top) const {
  if (!g_browser_process->local_state())
    return false;

  const DictionaryValue* dictionary =
      g_browser_process->local_state()->GetDictionary(GetWindowName().c_str());
  return dictionary &&
      dictionary->GetBoolean(L"always_on_top", always_on_top) && always_on_top;
}

}  // namespace

namespace browser {

// Declared in browser_dialogs.h so others don't need to depend on our header.
void ShowTaskManager() {
  TaskManagerView::Show();
}

}  // namespace browser

