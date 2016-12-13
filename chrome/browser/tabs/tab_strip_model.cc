// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tabs/tab_strip_model.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/tabs/tab_strip_model_order_controller.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"

namespace {

// Returns true if the specified transition is one of the types that cause the
// opener relationships for the tab in which the transition occured to be
// forgotten. This is generally any navigation that isn't a link click (i.e.
// any navigation that can be considered to be the start of a new task distinct
// from what had previously occurred in that tab).
bool ShouldForgetOpenersForTransition(PageTransition::Type transition) {
  return transition == PageTransition::TYPED ||
      transition == PageTransition::AUTO_BOOKMARK ||
      transition == PageTransition::GENERATED ||
      transition == PageTransition::KEYWORD ||
      transition == PageTransition::START_PAGE;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, public:

TabStripModel::TabStripModel(TabStripModelDelegate* delegate, Profile* profile)
    : delegate_(delegate),
      selected_index_(kNoTab),
      profile_(profile),
      closing_all_(false),
      order_controller_(NULL) {
  DCHECK(delegate_);
  registrar_.Add(this,
      NotificationType::TAB_CONTENTS_DESTROYED,
      NotificationService::AllSources());
  order_controller_ = new TabStripModelOrderController(this);
}

TabStripModel::~TabStripModel() {
  STLDeleteContainerPointers(contents_data_.begin(), contents_data_.end());
  delete order_controller_;
}

void TabStripModel::AddObserver(TabStripModelObserver* observer) {
  observers_.AddObserver(observer);
}

void TabStripModel::RemoveObserver(TabStripModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool TabStripModel::ContainsIndex(int index) const {
  return index >= 0 && index < count();
}

void TabStripModel::AppendTabContents(TabContents* contents, bool foreground) {
  // Tabs opened in the foreground using this method inherit the group of the
  // previously selected tab.
  InsertTabContentsAt(count(), contents, foreground, foreground);
}

void TabStripModel::InsertTabContentsAt(int index,
                                        TabContents* contents,
                                        bool foreground,
                                        bool inherit_group) {
  // In tab dragging situations, if the last tab in the window was detached
  // then the user aborted the drag, we will have the |closing_all_| member
  // set (see DetachTabContentsAt) which will mess with our mojo here. We need
  // to clear this bit.
  closing_all_ = false;

  // Have to get the selected contents before we monkey with |contents_|
  // otherwise we run into problems when we try to change the selected contents
  // since the old contents and the new contents will be the same...
  TabContents* selected_contents = GetSelectedTabContents();
  TabContentsData* data = new TabContentsData(contents);
  data->pinned = (index != count() && index < IndexOfFirstNonPinnedTab());
  if (inherit_group && selected_contents) {
    if (foreground) {
      // Forget any existing relationships, we don't want to make things too
      // confusing by having multiple groups active at the same time.
      ForgetAllOpeners();
    }
    // Anything opened by a link we deem to have an opener.
    data->SetGroup(&selected_contents->controller());
  }
  contents_data_.insert(contents_data_.begin() + index, data);

  FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
      TabInsertedAt(contents, index, foreground));

  if (foreground) {
    ChangeSelectedContentsFrom(selected_contents, index, false);
  } else if (index <= selected_index_) {
    // If a tab is inserted before the current selected index that is not
    // foreground, |selected_index| needs to be incremented.
    ++selected_index_;
  }
}

void TabStripModel::ReplaceNavigationControllerAt(
    int index, NavigationController* controller) {
  // This appears to be OK with no flicker since no redraw event
  // occurs between the call to add an aditional tab and one to close
  // the previous tab.
  InsertTabContentsAt(index + 1, controller->tab_contents(), true, true);
  InternalCloseTabContentsAt(index, false);
}

TabContents* TabStripModel::DetachTabContentsAt(int index) {
  if (contents_data_.empty())
    return NULL;

  DCHECK(ContainsIndex(index));
  TabContents* removed_contents = GetContentsAt(index);
  next_selected_index_ = order_controller_->DetermineNewSelectedIndex(index);
  delete contents_data_.at(index);
  contents_data_.erase(contents_data_.begin() + index);
  if (contents_data_.empty())
    closing_all_ = true;
  TabStripModelObservers::Iterator iter(observers_);
  while (TabStripModelObserver* obs = iter.GetNext()) {
    obs->TabDetachedAt(removed_contents, index);
    if (empty())
      obs->TabStripEmpty();
  }
  if (!contents_data_.empty()) {
    if (index == selected_index_) {
      ChangeSelectedContentsFrom(removed_contents, next_selected_index_,
                                 false);
    } else if (index < selected_index_) {
      // If the removed tab was before the selected index, we need to account
      // for this in the selected index...
      SelectTabContentsAt(selected_index_ - 1, false);
    }
  }
  next_selected_index_ = selected_index_;
  return removed_contents;
}

void TabStripModel::SelectTabContentsAt(int index, bool user_gesture) {
  DCHECK(ContainsIndex(index));
  ChangeSelectedContentsFrom(GetSelectedTabContents(), index, user_gesture);
}

void TabStripModel::MoveTabContentsAt(int index, int to_position,
                                      bool select_after_move) {
  MoveTabContentsAtImpl(index, to_position, select_after_move, true);
}

TabContents* TabStripModel::GetSelectedTabContents() const {
  return GetTabContentsAt(selected_index_);
}

TabContents* TabStripModel::GetTabContentsAt(int index) const {
  if (ContainsIndex(index))
    return GetContentsAt(index);
  return NULL;
}

int TabStripModel::GetIndexOfTabContents(const TabContents* contents) const {
  int index = 0;
  TabContentsDataVector::const_iterator iter = contents_data_.begin();
  for (; iter != contents_data_.end(); ++iter, ++index) {
    if ((*iter)->contents == contents)
      return index;
  }
  return kNoTab;
}

int TabStripModel::GetIndexOfController(
    const NavigationController* controller) const {
  int index = 0;
  TabContentsDataVector::const_iterator iter = contents_data_.begin();
  for (; iter != contents_data_.end(); ++iter, ++index) {
    if (&(*iter)->contents->controller() == controller)
      return index;
  }
  return kNoTab;
}

void TabStripModel::UpdateTabContentsStateAt(int index, bool loading_only) {
  DCHECK(ContainsIndex(index));

  FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
      TabChangedAt(GetContentsAt(index), index, loading_only));
}

void TabStripModel::CloseAllTabs() {
  // Set state so that observers can adjust their behavior to suit this
  // specific condition when CloseTabContentsAt causes a flurry of
  // Close/Detach/Select notifications to be sent.
  closing_all_ = true;
  for (int i = count() - 1; i >= 0; --i)
    CloseTabContentsAt(i);
}

bool TabStripModel::TabsAreLoading() const {
  TabContentsDataVector::const_iterator iter = contents_data_.begin();
  for (; iter != contents_data_.end(); ++iter) {
    if ((*iter)->contents->is_loading())
      return true;
  }
  return false;
}

NavigationController* TabStripModel::GetOpenerOfTabContentsAt(int index) {
  DCHECK(ContainsIndex(index));
  return contents_data_.at(index)->opener;
}

int TabStripModel::GetIndexOfNextTabContentsOpenedBy(
    const NavigationController* opener, int start_index, bool use_group) const {
  DCHECK(opener);
  DCHECK(ContainsIndex(start_index));

  TabContentsData* start_data = contents_data_.at(start_index);
  TabContentsDataVector::const_iterator iter =
      find(contents_data_.begin(), contents_data_.end(), start_data);
  TabContentsDataVector::const_iterator next;
  for (; iter != contents_data_.end(); ++iter) {
    next = iter + 1;
    if (next == contents_data_.end())
      break;
    if (OpenerMatches(*next, opener, use_group))
      return static_cast<int>(next - contents_data_.begin());
  }
  iter = find(contents_data_.begin(), contents_data_.end(), start_data);
  if (iter != contents_data_.begin()) {
    for (--iter; iter > contents_data_.begin(); --iter) {
      if (OpenerMatches(*iter, opener, use_group))
        return static_cast<int>(iter - contents_data_.begin());
    }
  }
  return kNoTab;
}

int TabStripModel::GetIndexOfLastTabContentsOpenedBy(
    const NavigationController* opener, int start_index) const {
  DCHECK(opener);
  DCHECK(ContainsIndex(start_index));

  TabContentsData* start_data = contents_data_.at(start_index);
  TabContentsDataVector::const_iterator end =
      find(contents_data_.begin(), contents_data_.end(), start_data);
  TabContentsDataVector::const_iterator iter =
      contents_data_.end();
  TabContentsDataVector::const_iterator next;
  for (; iter != end; --iter) {
    next = iter - 1;
    if (next == end)
      break;
    if ((*next)->opener == opener)
      return static_cast<int>(next - contents_data_.begin());
  }
  return kNoTab;
}

void TabStripModel::TabNavigating(TabContents* contents,
                                  PageTransition::Type transition) {
  if (ShouldForgetOpenersForTransition(transition)) {
    // Don't forget the openers if this tab is a New Tab page opened at the
    // end of the TabStrip (e.g. by pressing Ctrl+T). Give the user one
    // navigation of one of these transition types before resetting the
    // opener relationships (this allows for the use case of opening a new
    // tab to do a quick look-up of something while viewing a tab earlier in
    // the strip). We can make this heuristic more permissive if need be.
    if (!IsNewTabAtEndOfTabStrip(contents)) {
      // If the user navigates the current tab to another page in any way
      // other than by clicking a link, we want to pro-actively forget all
      // TabStrip opener relationships since we assume they're beginning a
      // different task by reusing the current tab.
      ForgetAllOpeners();
      // In this specific case we also want to reset the group relationship,
      // since it is now technically invalid.
      ForgetGroup(contents);
    }
  }
}

void TabStripModel::ForgetAllOpeners() {
  // Forget all opener memories so we don't do anything weird with tab
  // re-selection ordering.
  TabContentsDataVector::const_iterator iter = contents_data_.begin();
  for (; iter != contents_data_.end(); ++iter)
    (*iter)->ForgetOpener();
}

void TabStripModel::ForgetGroup(TabContents* contents) {
  int index = GetIndexOfTabContents(contents);
  DCHECK(ContainsIndex(index));
  contents_data_.at(index)->SetGroup(NULL);
  contents_data_.at(index)->ForgetOpener();
}

bool TabStripModel::ShouldResetGroupOnSelect(TabContents* contents) const {
  int index = GetIndexOfTabContents(contents);
  DCHECK(ContainsIndex(index));
  return contents_data_.at(index)->reset_group_on_select;
}

void TabStripModel::SetTabPinned(int index, bool pinned) {
  DCHECK(ContainsIndex(index));
  if (contents_data_[index]->pinned == pinned)
    return;

  int first_non_pinned_tab = IndexOfFirstNonPinnedTab();

  contents_data_[index]->pinned = pinned;

  if (pinned && index > first_non_pinned_tab) {
    // The tab is being pinned but beyond the pinned tabs. Move it to the end
    // of the pinned tabs.
    MoveTabContentsAtImpl(index, first_non_pinned_tab,
                          selected_index() == index, false);
  } else if (!pinned && index < first_non_pinned_tab - 1) {
    // The tab is being unpinned, but is within the pinned tabs, move it to
    // be after the set of pinned tabs.
    MoveTabContentsAtImpl(index, first_non_pinned_tab - 1,
                          selected_index() == index, false);
  } else {
    // Tab didn't move, but it's pinned state changed. Notify observers.
    FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                      TabPinnedStateChanged(contents_data_[index]->contents,
                                            index));
  }
}

bool TabStripModel::IsTabPinned(int index) const {
  return contents_data_[index]->pinned;
}

int TabStripModel::IndexOfFirstNonPinnedTab() const {
  for (size_t i = 0; i < contents_data_.size(); ++i) {
    if (!contents_data_[i]->pinned)
      return static_cast<int>(i);
  }
  // No pinned tabs.
  return count();
}

//  static
bool TabStripModel::IsTabPinningEnabled() {
  static bool checked = false;
  static bool enabled = false;
  if (!checked) {
    enabled = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableTabPinning);
  }
  return enabled;
}

void TabStripModel::AddTabContents(TabContents* contents,
                                   int index,
                                   bool force_index,
                                   PageTransition::Type transition,
                                   bool foreground) {
  if (transition == PageTransition::LINK && !force_index) {
    // Only try to be clever if we're opening a LINK.
    index = order_controller_->DetermineInsertionIndex(
        contents, transition, foreground);
  } else {
    // For all other types, respect what was passed to us, normalizing -1s.
    if (index < 0)
      index = count();
  }

  // Tabs opened from links inherit the "group" attribute of the Tab from which
  // they were opened. This means when they're closed, that Tab will be
  // selected again.
  bool inherit_group = transition == PageTransition::LINK;
  if (!inherit_group) {
    // Also, any tab opened at the end of the TabStrip with a "TYPED"
    // transition inherit group as well. This covers the cases where the user
    // creates a New Tab (e.g. Ctrl+T, or clicks the New Tab button), or types
    // in the address bar and presses Alt+Enter. This allows for opening a new
    // Tab to quickly look up something. When this Tab is closed, the old one
    // is re-selected, not the next-adjacent.
    inherit_group = transition == PageTransition::TYPED && index == count();
  }
  InsertTabContentsAt(index, contents, foreground, inherit_group);
  if (inherit_group && transition == PageTransition::TYPED)
    contents_data_.at(index)->reset_group_on_select = true;
}

void TabStripModel::CloseSelectedTab() {
  CloseTabContentsAt(selected_index_);
}

void TabStripModel::SelectNextTab() {
  // This may happen during automated testing or if a user somehow buffers
  // many key accelerators.
  if (empty())
    return;

  int next_index = (selected_index_ + 1) % count();
  SelectTabContentsAt(next_index, true);
}

void TabStripModel::SelectPreviousTab() {
  int prev_index = selected_index_ - 1;
  if (prev_index < 0)
    prev_index = count() + prev_index;
  SelectTabContentsAt(prev_index, true);
}

void TabStripModel::SelectLastTab() {
  SelectTabContentsAt(count() - 1, true);
}

Browser* TabStripModel::TearOffTabContents(TabContents* detached_contents,
                                           const gfx::Rect& window_bounds,
                                           const DockInfo& dock_info) {
  DCHECK(detached_contents);
  return delegate_->CreateNewStripWithContents(detached_contents, window_bounds,
                                               dock_info);
}

// Context menu functions.
bool TabStripModel::IsContextMenuCommandEnabled(
    int context_index, ContextMenuCommand command_id) const {
  DCHECK(command_id > CommandFirst && command_id < CommandLast);
  switch (command_id) {
    case CommandNewTab:
    case CommandReload:
    case CommandCloseTab:
      return true;
    case CommandCloseOtherTabs:
      return count() > 1;
    case CommandCloseTabsToRight:
      return context_index < (count() - 1);
    case CommandCloseTabsOpenedBy: {
      int next_index = GetIndexOfNextTabContentsOpenedBy(
          &GetTabContentsAt(context_index)->controller(), context_index, true);
      return next_index != kNoTab;
    }
    case CommandDuplicate:
      return delegate_->CanDuplicateContentsAt(context_index);
    case CommandRestoreTab:
      return delegate_->CanRestoreTab();
    case CommandTogglePinned:
      return true;
    default:
      NOTREACHED();
  }
  return false;
}

void TabStripModel::ExecuteContextMenuCommand(
    int context_index, ContextMenuCommand command_id) {
  DCHECK(command_id > CommandFirst && command_id < CommandLast);
  switch (command_id) {
    case CommandNewTab:
      UserMetrics::RecordAction(L"TabContextMenu_NewTab", profile_);
      delegate()->AddBlankTabAt(context_index + 1, true);
      break;
    case CommandReload:
      UserMetrics::RecordAction(L"TabContextMenu_Reload", profile_);
      GetContentsAt(context_index)->controller().Reload(true);
      break;
    case CommandDuplicate:
      UserMetrics::RecordAction(L"TabContextMenu_Duplicate", profile_);
      delegate_->DuplicateContentsAt(context_index);
      break;
    case CommandCloseTab:
      UserMetrics::RecordAction(L"TabContextMenu_CloseTab", profile_);
      CloseTabContentsAt(context_index);
      break;
    case CommandCloseOtherTabs: {
      UserMetrics::RecordAction(L"TabContextMenu_CloseOtherTabs", profile_);
      TabContents* contents = GetTabContentsAt(context_index);
      for (int i = count() - 1; i >= 0; --i) {
        if (GetTabContentsAt(i) != contents)
          CloseTabContentsAt(i);
      }
      break;
    }
    case CommandCloseTabsToRight: {
      UserMetrics::RecordAction(L"TabContextMenu_CloseTabsToRight", profile_);
      for (int i = count() - 1; i > context_index; --i)
        CloseTabContentsAt(i);
      break;
    }
    case CommandCloseTabsOpenedBy: {
      UserMetrics::RecordAction(L"TabContextMenu_CloseTabsOpenedBy", profile_);
      NavigationController* opener =
          &GetTabContentsAt(context_index)->controller();

      for (int i = count() - 1; i >= 0; --i) {
        if (OpenerMatches(contents_data_.at(i), opener, true))
          CloseTabContentsAt(i);
      }

      break;
    }
    case CommandRestoreTab: {
      UserMetrics::RecordAction(L"TabContextMenu_RestoreTab", profile_);
      delegate_->RestoreTab();
      break;
    }
    case CommandTogglePinned: {
      UserMetrics::RecordAction(L"TabContextMenu_TogglePinned", profile_);

      SelectTabContentsAt(context_index, true);
      SetTabPinned(context_index, !IsTabPinned(context_index));
      break;
    }
    default:
      NOTREACHED();
  }
}

std::vector<int> TabStripModel::GetIndexesOpenedBy(int index) const {
  std::vector<int> indices;
  NavigationController* opener = &GetTabContentsAt(index)->controller();
  for (int i = count() - 1; i >= 0; --i) {
    if (OpenerMatches(contents_data_.at(i), opener, true))
      indices.push_back(i);
  }
  return indices;
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, NotificationObserver implementation:

void TabStripModel::Observe(NotificationType type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  // Sometimes, on qemu, it seems like a TabContents object can be destroyed
  // while we still have a reference to it. We need to break this reference
  // here so we don't crash later.
  int index = GetIndexOfTabContents(Source<TabContents>(source).ptr());
  if (index != TabStripModel::kNoTab) {
    // Note that we only detach the contents here, not close it - it's already
    // been closed. We just want to undo our bookkeeping.
    DetachTabContentsAt(index);
  }
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, private:

bool TabStripModel::IsNewTabAtEndOfTabStrip(TabContents* contents) const {
  return LowerCaseEqualsASCII(contents->GetURL().spec(),
                              chrome::kChromeUINewTabURL) &&
      contents == GetContentsAt(count() - 1) &&
      contents->controller().entry_count() == 1;
}

bool TabStripModel::InternalCloseTabContentsAt(int index,
                                               bool create_historical_tab) {
  if (!delegate_->CanCloseContentsAt(index))
    return false;

  TabContents* detached_contents = GetContentsAt(index);

  if (delegate_->RunUnloadListenerBeforeClosing(detached_contents))
    return false;

  // TODO: Now that we know the tab has no unload/beforeunload listeners,
  // we should be able to do a fast shutdown of the RenderViewProcess.
  // Make sure that we actually do.

  FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
      TabClosingAt(detached_contents, index));

  if (detached_contents) {
    // Ask the delegate to save an entry for this tab in the historical tab
    // database if applicable.
    if (create_historical_tab)
      delegate_->CreateHistoricalTab(detached_contents);

    // Deleting the TabContents will call back to us via NotificationObserver
    // and detach it.
    delete detached_contents;
  }
  return true;
}

void TabStripModel::MoveTabContentsAtImpl(int index, int to_position,
                                          bool select_after_move,
                                          bool update_pinned_state) {
  DCHECK(ContainsIndex(index));
  if (index == to_position)
    return;

  bool pinned_state_changed = !update_pinned_state;

  if (update_pinned_state) {
    bool is_pinned = IsTabPinned(index);
    if (is_pinned && to_position >= IndexOfFirstNonPinnedTab()) {
      contents_data_[index]->pinned = false;
    } else if (!is_pinned && to_position < IndexOfFirstNonPinnedTab()) {
      contents_data_[index]->pinned = true;
    }
    pinned_state_changed = is_pinned != contents_data_[index]->pinned;
  }

  TabContentsData* moved_data = contents_data_.at(index);
  contents_data_.erase(contents_data_.begin() + index);
  contents_data_.insert(contents_data_.begin() + to_position, moved_data);

  // if !select_after_move, keep the same tab selected as was selected before.
  if (select_after_move || index == selected_index_) {
    selected_index_ = to_position;
  } else if (index < selected_index_ && to_position >= selected_index_) {
    selected_index_--;
  } else if (index > selected_index_ && to_position <= selected_index_) {
    selected_index_++;
  }

  FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                    TabMoved(moved_data->contents, index, to_position,
                             pinned_state_changed));
}

TabContents* TabStripModel::GetContentsAt(int index) const {
  CHECK(ContainsIndex(index)) <<
      "Failed to find: " << index << " in: " << count() << " entries.";
  return contents_data_.at(index)->contents;
}

void TabStripModel::ChangeSelectedContentsFrom(
    TabContents* old_contents, int to_index, bool user_gesture) {
  DCHECK(ContainsIndex(to_index));
  TabContents* new_contents = GetContentsAt(to_index);
  if (old_contents == new_contents)
    return;

  TabContents* last_selected_contents = old_contents;
  if (last_selected_contents) {
    FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                      TabDeselectedAt(last_selected_contents, selected_index_));
  }

  selected_index_ = to_index;
  FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
      TabSelectedAt(last_selected_contents, new_contents, selected_index_,
                    user_gesture));
}

void TabStripModel::SetOpenerForContents(TabContents* contents,
                                    TabContents* opener) {
  int index = GetIndexOfTabContents(contents);
  contents_data_.at(index)->opener = &opener->controller();
}

// static
bool TabStripModel::OpenerMatches(const TabContentsData* data,
                                  const NavigationController* opener,
                                  bool use_group) {
  return data->opener == opener || (use_group && data->group == opener);
}
