// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/back_forward_menu_model.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/dom_ui/history_ui.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/l10n_util.h"
#include "grit/generated_resources.h"
#include "net/base/registry_controlled_domain.h"

#if defined(OS_WIN)
// TODO(port): port these headers and remove the platform defines.
#include "chrome/browser/tab_contents/tab_contents.h"
#elif defined(OS_POSIX)
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

const int BackForwardMenuModel::kMaxHistoryItems = 12;
const int BackForwardMenuModel::kMaxChapterStops = 5;

int BackForwardMenuModel::GetHistoryItemCount() const {
  TabContents* contents = GetTabContents();
  NavigationController* controller = contents->controller();

  int items = 0;

  if (model_type_ == FORWARD_MENU_DELEGATE) {
    // Only count items from n+1 to end (if n is current entry)
    items = controller->GetEntryCount() -
            controller->GetCurrentEntryIndex() - 1;
  } else {
    items = controller->GetCurrentEntryIndex();
  }

  if (items > kMaxHistoryItems)
    items = kMaxHistoryItems;
  else if (items < 0)
    items = 0;

  return items;
}

int BackForwardMenuModel::GetChapterStopCount(int history_items) const {
  TabContents* contents = GetTabContents();
  NavigationController* controller = contents->controller();

  int chapter_stops = 0;
  int current_entry = controller->GetCurrentEntryIndex();

  if (history_items == kMaxHistoryItems) {
    int chapter_id = current_entry;
    if (model_type_ == FORWARD_MENU_DELEGATE) {
      chapter_id += history_items;
    } else {
      chapter_id -= history_items;
    }

    do {
      chapter_id = GetIndexOfNextChapterStop(chapter_id,
          model_type_ == FORWARD_MENU_DELEGATE);
      if (chapter_id != -1)
        ++chapter_stops;
    } while (chapter_id != -1 && chapter_stops < kMaxChapterStops);
  }

  return chapter_stops;
}

int BackForwardMenuModel::GetTotalItemCount() const {
  int items = GetHistoryItemCount();

  if (items > 0) {
    int chapter_stops = 0;

    // Next, we count ChapterStops, if any.
    if (items == kMaxHistoryItems)
      chapter_stops = GetChapterStopCount(items);

    if (chapter_stops)
      items += chapter_stops + 1;  // Chapter stops also need a separator.

    // If the menu is not empty, add two positions in the end
    // for a separator and a "Show Full History" item.
    items += 2;
  }

  return items;
}

int BackForwardMenuModel::GetIndexOfNextChapterStop(int start_from,
                                                    bool forward) const {
  TabContents* contents = GetTabContents();
  NavigationController* controller = contents->controller();

  int max_count = controller->GetEntryCount();
  if (start_from < 0 || start_from >= max_count)
    return -1;  // Out of bounds.

  if (forward) {
    if (start_from < max_count - 1) {
      // We want to advance over the current chapter stop, so we add one.
      // We don't need to do this when direction is backwards.
      start_from++;
    } else {
      return -1;
    }
  }

  NavigationEntry* start_entry = controller->GetEntryAtIndex(start_from);
  const GURL& url = start_entry->url();

  if (!forward) {
    // When going backwards we return the first entry we find that has a
    // different domain.
    for (int i = start_from - 1; i >= 0; --i) {
      if (!net::RegistryControlledDomainService::SameDomainOrHost(url,
              controller->GetEntryAtIndex(i)->url()))
        return i;
    }
    // We have reached the beginning without finding a chapter stop.
    return -1;
  } else {
    // When going forwards we return the entry before the entry that has a
    // different domain.
    for (int i = start_from + 1; i < max_count; ++i) {
      if (!net::RegistryControlledDomainService::SameDomainOrHost(url,
              controller->GetEntryAtIndex(i)->url()))
        return i - 1;
    }
    // Last entry is always considered a chapter stop.
    return max_count - 1;
  }
}

int BackForwardMenuModel::FindChapterStop(int offset,
                                          bool forward,
                                          int skip) const {
  if (offset < 0 || skip < 0)
    return -1;

  if (!forward)
    offset *= -1;

  TabContents* contents = GetTabContents();
  NavigationController* controller = contents->controller();

  int entry = controller->GetCurrentEntryIndex() + offset;

  for (int i = 0; i < skip + 1; i++)
    entry = GetIndexOfNextChapterStop(entry, forward);

  return entry;
}

void BackForwardMenuModel::ExecuteCommandById(int menu_id) {
  TabContents* contents = GetTabContents();
  NavigationController* controller = contents->controller();

  DCHECK(!IsSeparator(menu_id));

  // Execute the command for the last item: "Show Full History".
  if (menu_id == GetTotalItemCount()) {
    UserMetrics::RecordComputedAction(BuildActionName(L"ShowFullHistory", -1),
                                      controller->profile());
#if defined(OS_WIN)
    browser_->ShowSingleDOMUITab(HistoryUI::GetBaseURL());
#else
    NOTIMPLEMENTED();
#endif
    return;
  }

  // Log whether it was a history or chapter click.
  if (menu_id <= GetHistoryItemCount()) {
    UserMetrics::RecordComputedAction(
        BuildActionName(L"HistoryClick", menu_id), controller->profile());
  } else {
    UserMetrics::RecordComputedAction(
        BuildActionName(L"ChapterClick", menu_id - GetHistoryItemCount() - 1),
        controller->profile());
  }

  int index = MenuIdToNavEntryIndex(menu_id);
  if (index >= 0 && index < controller->GetEntryCount())
    controller->GoToIndex(index);
}

bool BackForwardMenuModel::IsSeparator(int menu_id) const {
  int history_items = GetHistoryItemCount();
  // If the menu_id is higher than the number of history items + separator,
  // we then consider if it is a chapter-stop entry.
  if (menu_id > history_items + 1) {
    // We either are in ChapterStop area, or at the end of the list (the "Show
    // Full History" link).
    int chapter_stops = GetChapterStopCount(history_items);
    if (chapter_stops == 0)
      return false;  // We must have reached the "Show Full History" link.
    // Otherwise, look to see if we have reached the separator for the
    // chapter-stops. If not, this is a chapter stop.
    return (menu_id == history_items + 1 +
                       chapter_stops + 1);
  }

  // Look to see if we have reached the separator for the history items.
  return menu_id == history_items + 1;
}

std::wstring BackForwardMenuModel::GetItemLabel(int menu_id) const {
  // Return label "Show Full History" for the last item of the menu.
  if (menu_id == GetTotalItemCount())
    return l10n_util::GetString(IDS_SHOWFULLHISTORY_LINK);

  // Return an empty string for a separator.
  if (IsSeparator(menu_id))
    return L"";

  NavigationEntry* entry = GetNavigationEntry(menu_id);
  return entry->title();
}

const SkBitmap& BackForwardMenuModel::GetItemIcon(int menu_id) const {
  DCHECK(ItemHasIcon(menu_id));

  NavigationEntry* entry = GetNavigationEntry(menu_id);
  return entry->favicon().bitmap();
}

bool BackForwardMenuModel::ItemHasIcon(int menu_id) const {
  // Using "id" not "id - 1" because the last item "Show Full History"
  // doesn't have an icon.
  return menu_id < GetTotalItemCount() && !IsSeparator(menu_id);
}

bool BackForwardMenuModel::ItemHasCommand(int menu_id) const {
  return menu_id - 1 < GetTotalItemCount() && !IsSeparator(menu_id);
}

std::wstring BackForwardMenuModel::GetShowFullHistoryLabel() const {
  return l10n_util::GetString(IDS_SHOWFULLHISTORY_LINK);
}

TabContents* BackForwardMenuModel::GetTabContents() const {
  // We use the test tab contents if the unit test has specified it.
  return test_tab_contents_ ? test_tab_contents_ :
                              browser_->GetSelectedTabContents();
}

int BackForwardMenuModel::MenuIdToNavEntryIndex(int menu_id) const {
  TabContents* contents = GetTabContents();
  NavigationController* controller = contents->controller();

  int history_items = GetHistoryItemCount();

  DCHECK(menu_id > 0);

  // Convert anything above the History items separator.
  if (menu_id <= history_items) {
    if (model_type_ == FORWARD_MENU_DELEGATE) {
      // The |menu_id| is relative to our current position, so we need to add.
      menu_id += controller->GetCurrentEntryIndex();
    } else {
      // Back menu is reverse.
      menu_id = controller->GetCurrentEntryIndex() - menu_id;
    }
    return menu_id;
  }
  if (menu_id == history_items + 1)
    return -1;           // Don't translate the separator for history items.

  if (menu_id >= history_items + 1 + GetChapterStopCount(history_items) + 1)
    return -1;           // This is beyond the last chapter stop so we abort.

  // This menu item is a chapter stop located between the two separators.
  menu_id = FindChapterStop(history_items,
                            model_type_ == FORWARD_MENU_DELEGATE,
                            menu_id - history_items - 1 - 1);

  return menu_id;
}

NavigationEntry* BackForwardMenuModel::GetNavigationEntry(int menu_id) const {
  TabContents* contents = GetTabContents();
  NavigationController* controller = contents->controller();

  int index = MenuIdToNavEntryIndex(menu_id);
  return controller->GetEntryAtIndex(index);
}

std::wstring BackForwardMenuModel::BuildActionName(
    const std::wstring& action, int index) const {
  DCHECK(!action.empty());
  DCHECK(index >= -1);
  std::wstring metric_string;
  if (model_type_ == FORWARD_MENU_DELEGATE)
    metric_string += L"ForwardMenu_";
  else
    metric_string += L"BackMenu_";
  metric_string += action;
  if (index != -1)
    metric_string += IntToWString(index);
  return metric_string;
}
