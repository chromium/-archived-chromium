// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/navigation_controller.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/navigation_types.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/scoped_vector.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/dom_ui_host.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/repost_form_warning_dialog.h"
#include "chrome/browser/site_instance.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/net_util.h"

// TabContentsCollector ---------------------------------------------------

// We never destroy a TabContents synchronously because there are some
// complex code path that cause the current TabContents to be in the call
// stack. So instead, we use a TabContentsCollector which either destroys
// the TabContents or does nothing if it has been cancelled.
class TabContentsCollector : public Task {
 public:
  TabContentsCollector(NavigationController* target,
                       TabContentsType target_type)
      : target_(target),
        target_type_(target_type) {
  }

  void Cancel() {
    target_ = NULL;
  }

  virtual void Run() {
    if (target_) {
      // Note: this will cancel this task as a side effect so target_ is
      // now null.
      TabContents* tc = target_->GetTabContents(target_type_);
      tc->Destroy();
    }
  }

 private:
  // The NavigationController we are acting on.
  NavigationController* target_;

  // The TabContentsType that needs to be collected.
  TabContentsType target_type_;

  DISALLOW_EVIL_CONSTRUCTORS(TabContentsCollector);
};

// NavigationController ---------------------------------------------------

// static
bool NavigationController::check_for_repost_ = true;

// Creates a new NavigationEntry for each TabNavigation in navigations, adding
// the NavigationEntry to entries. This is used during session restore.
static void CreateNavigationEntriesFromTabNavigations(
    const std::vector<TabNavigation>& navigations,
    std::vector<NavigationEntry*>* entries) {
  // Create a NavigationEntry for each of the navigations.
  for (std::vector<TabNavigation>::const_iterator i =
           navigations.begin(); i != navigations.end(); ++i) {
    const TabNavigation& navigation = *i;

    GURL real_url = navigation.url;
    TabContentsType type = TabContents::TypeForURL(&real_url);
    DCHECK(type != TAB_CONTENTS_UNKNOWN_TYPE);

    NavigationEntry* entry = new NavigationEntry(
        type,
        NULL,  // The site instance for restored tabs is sent on naviagion
               // (WebContents::GetSiteInstanceForEntry).
        static_cast<int>(i - navigations.begin()),
        real_url,
        navigation.title,
        // Use a transition type of reload so that we don't incorrectly
        // increase the typed count.
        PageTransition::RELOAD);
    entry->SetDisplayURL(navigation.url);
    entry->SetContentState(navigation.state);
    entry->SetHasPostData(navigation.type_mask & TabNavigation::HAS_POST_DATA);
    entries->push_back(entry);
  }
}

// Configure all the NavigationEntries in entries for restore. This resets
// the transition type to reload and makes sure the content state isn't empty.
static void ConfigureEntriesForRestore(
    std::vector<NavigationEntry*>* entries) {
  for (size_t i = 0, count = entries->size(); i < count; ++i) {
    // Use a transition type of reload so that we don't incorrectly increase
    // the typed count.
    (*entries)[i]->SetTransitionType(PageTransition::RELOAD);
    (*entries)[i]->set_restored(true);
    // NOTE(darin): This code is only needed for backwards compat.
    NavigationControllerBase::SetContentStateIfEmpty((*entries)[i]);
  }
}

NavigationController::NavigationController(TabContents* contents,
                                           Profile* profile)
    : profile_(profile),
      active_contents_(contents),
      alternate_nav_url_fetcher_entry_unique_id_(0),
      max_restored_page_id_(-1),
      ssl_manager_(this, NULL),
      needs_reload_(false),
      load_pending_entry_when_active_(false) {
  if (contents)
    RegisterTabContents(contents);
  DCHECK(profile_);
  profile_->RegisterNavigationController(this);
}

NavigationController::NavigationController(
    Profile* profile,
    const std::vector<TabNavigation>& navigations,
    int selected_navigation,
    HWND parent)
    : profile_(profile),
      active_contents_(NULL),
      alternate_nav_url_fetcher_entry_unique_id_(0),
      max_restored_page_id_(-1),
      ssl_manager_(this, NULL),
      needs_reload_(true),
      load_pending_entry_when_active_(false) {
  DCHECK(profile_);
  DCHECK(selected_navigation >= 0 &&
         selected_navigation < static_cast<int>(navigations.size()));

  profile_->RegisterNavigationController(this);

  // Populate entries_ from the supplied TabNavigations.
  CreateNavigationEntriesFromTabNavigations(navigations, &entries_);

  // And finish the restore.
  FinishRestore(parent, selected_navigation);
}

NavigationController::~NavigationController() {
  DCHECK(tab_contents_map_.empty());
  DCHECK(tab_contents_collector_map_.empty());

  profile_->UnregisterNavigationController(this);
  NotificationService::current()->Notify(NOTIFY_TAB_CLOSED,
                                         Source<NavigationController>(this),
                                         NotificationService::NoDetails());
}

TabContents* NavigationController::GetTabContents(TabContentsType t) {
  // Make sure the TabContents is no longer scheduled for collection.
  CancelTabContentsCollection(t);
  return tab_contents_map_[t];
}

void NavigationController::Reset() {
  NavigationControllerBase::Reset();

  NotifyPrunedEntries();
}

void NavigationController::Reload() {
  // TODO(pkasting): http://b/1113085 Should this use DiscardPendingEntry()?
  DiscardPendingEntryInternal();
  int current_index = GetCurrentEntryIndex();
  if (check_for_repost_ && current_index != -1 &&
      GetEntryAtIndex(current_index)->HasPostData() &&
      active_contents_->AsWebContents() &&
      !active_contents_->AsWebContents()->showing_repost_interstitial()) {
    // The user is asking to reload a page with POST data and we're not showing
    // the POST interstitial. Prompt to make sure they really want to do this.
    // If they do, RepostFormWarningDialog calls us back with
    // ReloadDontCheckForRepost.
    active_contents_->Activate();
    RepostFormWarningDialog::RunRepostFormWarningDialog(this);
  } else {
    NavigationControllerBase::Reload();
  }
}

void NavigationController::ReloadDontCheckForRepost() {
  NavigationControllerBase::Reload();
}

void NavigationController::Destroy() {
  // Close all tab contents owned by this controller.  We make a list on the
  // stack because they are removed from the map as they are Destroyed
  // (invalidating the iterators), which may or may not occur synchronously.
  // We also keep track of any NULL entries in the map so that we can clean
  // them out.
  std::list<TabContents*> tabs_to_destroy;
  std::list<TabContentsType> tab_types_to_erase;
  for (TabContentsMap::iterator i = tab_contents_map_.begin();
       i != tab_contents_map_.end(); ++i) {
    if (i->second)
      tabs_to_destroy.push_back(i->second);
    else
      tab_types_to_erase.push_back(i->first);
  }

  // Clean out all NULL entries in the map so that we know empty map means all
  // tabs destroyed.  This is needed since TabContentsWasDestroyed() won't get
  // called for types that are in our map with a NULL contents.  (We don't do
  // this by iterating over TAB_CONTENTS_NUM_TYPES because some tests create
  // additional types.)
  for (std::list<TabContentsType>::iterator i = tab_types_to_erase.begin();
       i != tab_types_to_erase.end(); ++i) {
    TabContentsMap::iterator map_iterator = tab_contents_map_.find(*i);
    if (map_iterator != tab_contents_map_.end()) {
      DCHECK(!map_iterator->second);
      tab_contents_map_.erase(map_iterator);
    }
  }

  // Cancel all the TabContentsCollectors.
  for (TabContentsCollectorMap::iterator i =
           tab_contents_collector_map_.begin();
       i != tab_contents_collector_map_.end(); ++i) {
    DCHECK(i->second);
    i->second->Cancel();
  }
  tab_contents_collector_map_.clear();


  // Finally destroy all the tab contents.
  for (std::list<TabContents*>::iterator i = tabs_to_destroy.begin();
       i != tabs_to_destroy.end(); ++i) {
    (*i)->Destroy();
  }
  // We are deleted at this point.
}

void NavigationController::TabContentsWasDestroyed(TabContentsType type) {
  TabContentsMap::iterator i = tab_contents_map_.find(type);
  DCHECK(i != tab_contents_map_.end());
  tab_contents_map_.erase(i);

  // Make sure we cancel any collector for that TabContents.
  CancelTabContentsCollection(type);

  // If that was the last tab to be destroyed, delete ourselves.
  if (tab_contents_map_.empty())
    delete this;
}

NavigationEntry* NavigationController::CreateNavigationEntry(
    const GURL& url, PageTransition::Type transition) {
  GURL real_url = url;
  TabContentsType type;

  // If the active contents supports |url|, use it.
  // Note: in both cases, we give TabContents a chance to rewrite the URL.
  TabContents* active = active_contents();
  if (active && active->SupportsURL(&real_url))
    type = active->type();
  else
    type = TabContents::TypeForURL(&real_url);

  NavigationEntry* entry = new NavigationEntry(type, NULL, -1, real_url,
                                               std::wstring(), transition);
  entry->SetDisplayURL(url);
  entry->SetUserTypedURL(url);
  if (url.SchemeIsFile()) {
    entry->SetTitle(file_util::GetFilenameFromPath(UTF8ToWide(url.host() +
                                                              url.path())));
  }
  return entry;
}

void NavigationController::LoadURL(const GURL& url,
                                   PageTransition::Type transition) {
  // The user initiated a load, we don't need to reload anymore.
  needs_reload_ = false;

  NavigationEntry* entry = CreateNavigationEntry(url, transition);

  LoadEntry(entry);
}

void NavigationController::LoadURLLazily(const GURL& url,
                                         PageTransition::Type type,
                                         const std::wstring& title,
                                         SkBitmap* icon) {
  NavigationEntry* entry = CreateNavigationEntry(url, type);
  entry->SetTitle(title);
  if (icon)
    entry->SetFavIcon(*icon);

  // TODO(pkasting): http://b/1113085 Should this use DiscardPendingEntry()?
  DiscardPendingEntryInternal();
  pending_entry_ = entry;
  load_pending_entry_when_active_ = true;
}

bool NavigationController::LoadingURLLazily() {
  return load_pending_entry_when_active_;
}

const std::wstring& NavigationController::GetLazyTitle() const {
  if (pending_entry_)
    return pending_entry_->GetTitle();
  else
    return EmptyWString();
}

const SkBitmap& NavigationController::GetLazyFavIcon() const {
  if (pending_entry_) {
    return pending_entry_->GetFavIcon();
  } else {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    return *rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
  }
}

void NavigationController::SetAlternateNavURLFetcher(
    AlternateNavURLFetcher* alternate_nav_url_fetcher) {
  DCHECK(!alternate_nav_url_fetcher_.get());
  DCHECK(pending_entry_);
  alternate_nav_url_fetcher_.reset(alternate_nav_url_fetcher);
  alternate_nav_url_fetcher_entry_unique_id_ = pending_entry_->unique_id();
}

void NavigationController::DidNavigateToEntry(NavigationEntry* entry) {
  DCHECK(active_contents_);
  DCHECK(entry->GetType() == active_contents_->type());

  NavigationControllerBase::DidNavigateToEntry(entry);
  // entry is now deleted

  if (alternate_nav_url_fetcher_.get()) {
    // Because this call may synchronously show an infobar, we do it last, to
    // make sure all other state is stable and the infobar won't get blown away
    // by some transition.
    alternate_nav_url_fetcher_->OnNavigatedToEntry();
  }

  // It is now a safe time to schedule collection for any tab contents of a
  // different type, because a navigation is necessary to get back to them.
  int index = GetCurrentEntryIndex();
  if (index < 0 || GetPendingEntryIndex() != -1)
    return;

  TabContentsType active_type = GetEntryAtIndex(index)->GetType();
  for (TabContentsMap::iterator i = tab_contents_map_.begin();
       i != tab_contents_map_.end(); ++i) {
    if (i->first != active_type)
      ScheduleTabContentsCollection(i->first);
  }
}

void NavigationController::DiscardPendingEntry() {
  NavigationControllerBase::DiscardPendingEntry();

  // Synchronize the active_contents_ to the last committed entry.
  NavigationEntry* last_entry = GetLastCommittedEntry();
  if (last_entry && last_entry->GetType() != active_contents_->type()) {
    TabContents* from_contents = active_contents_;
    from_contents->SetActive(false);

    // Switch back to the previous tab contents.
    active_contents_ = GetTabContents(last_entry->GetType());
    DCHECK(active_contents_);

    active_contents_->SetActive(true);

    // If we are transitioning from two types of WebContents, we need to migrate
    // the download shelf if it is visible. The download shelf may have been
    // created before the error that caused us to discard the entry.
    WebContents::MigrateShelfView(from_contents, active_contents_);

    if (from_contents->delegate()) {
      from_contents->delegate()->ReplaceContents(from_contents,
                                                 active_contents_);
    }

    // The entry we just discarded needed a different TabContents type. We no
    // longer need it but we can't destroy it just yet because the TabContents
    // is very likely involved in the current stack.
    DCHECK(from_contents != active_contents_);
    ScheduleTabContentsCollection(from_contents->type());
  }
}

void NavigationController::InsertEntry(NavigationEntry* entry) {
  NavigationControllerBase::InsertEntry(entry);
  active_contents_->NotifyDidNavigate(NAVIGATION_NEW, 0);
}

void NavigationController::SetWindowID(const SessionID& id) {
  window_id_ = id;
  NotificationService::current()->Notify(NOTIFY_TAB_PARENTED,
                                         Source<NavigationController>(this),
                                         NotificationService::NoDetails());
}

void NavigationController::NavigateToPendingEntry(bool reload) {
  TabContents* from_contents = active_contents_;

  // For session history navigations only the pending_entry_index_ is set.
  if (!pending_entry_) {
    DCHECK(pending_entry_index_ != -1);
    pending_entry_ = entries_[pending_entry_index_];
  }

  // Reset the security states as any SSL error may have been resolved since we
  // last visited that page.
  pending_entry_->ResetSSLStates();

  if (from_contents && from_contents->type() != pending_entry_->GetType())
    from_contents->SetActive(false);

  HWND parent =
      from_contents ? GetParent(from_contents->GetContainerHWND()) : 0;
  TabContents* contents =
      GetTabContentsCreateIfNecessary(parent, *pending_entry_);

  contents->SetActive(true);
  active_contents_ = contents;

  if (from_contents && from_contents != contents) {
    if (from_contents->delegate())
      from_contents->delegate()->ReplaceContents(from_contents, contents);
  }

  if (!contents->Navigate(*pending_entry_, reload))
    DiscardPendingEntry();
}

void NavigationController::NotifyNavigationEntryCommitted() {
  // Reset the Alternate Nav URL Fetcher if we're loading some page it doesn't
  // care about.  We must do this before calling Notify() below as that may
  // result in the creation of a new fetcher.
  //
  // TODO(brettw) bug 1324500: this logic should be moved out of the controller!
  const NavigationEntry* const entry = GetActiveEntry();
  if (!entry ||
      (entry->unique_id() != alternate_nav_url_fetcher_entry_unique_id_)) {
    alternate_nav_url_fetcher_.reset();
    alternate_nav_url_fetcher_entry_unique_id_ = 0;
  }

  // TODO(pkasting): http://b/1113079 Probably these explicit notification paths
  // should be removed, and interested parties should just listen for the
  // notification below instead.
  ssl_manager_.NavigationStateChanged();
  active_contents_->NotifyNavigationStateChanged(
      TabContents::INVALIDATE_EVERYTHING);

  NotificationService::current()->Notify(NOTIFY_NAV_ENTRY_COMMITTED,
                                         Source<NavigationController>(this),
                                         NotificationService::NoDetails());
}

void NavigationController::NotifyPrunedEntries() {
  NotificationService::current()->Notify(NOTIFY_NAV_LIST_PRUNED,
                                         Source<NavigationController>(this),
                                         NotificationService::NoDetails());
}

void NavigationController::IndexOfActiveEntryChanged(
    int prev_committed_index) {
  NavigationType nav_type = NAVIGATION_BACK_FORWARD;
  int relative_navigation_offset =
      GetLastCommittedEntryIndex() - prev_committed_index;
  if (relative_navigation_offset == 0) {
    nav_type = NAVIGATION_REPLACE;
  }
  active_contents_->NotifyDidNavigate(nav_type, relative_navigation_offset);
}

TabContents* NavigationController::GetTabContentsCreateIfNecessary(
    HWND parent,
    const NavigationEntry& entry) {
  TabContents* contents = GetTabContents(entry.GetType());
  if (!contents) {
    contents = TabContents::CreateWithType(entry.GetType(), parent, profile_,
                                           entry.site_instance());
    if (!contents->AsWebContents()) {
      // Update the max page id, otherwise the newly created TabContents may
      // have reset its max page id resulting in all new navigations. We only
      // do this for non-WebContents as WebContents takes care of this via its
      // SiteInstance. If this creation is the result of a restore, WebContents
      // handles invoking ReservePageIDRange to make sure the renderer's
      // max_page_id is updated to reflect the restored range of page ids.
      int32 max_page_id = contents->GetMaxPageID();
      for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i]->GetType() == entry.GetType())
          max_page_id = std::max(max_page_id, entries_[i]->GetPageID());
      }
      contents->UpdateMaxPageID(max_page_id);
    }
    RegisterTabContents(contents);
  }

  // We should not be trying to collect this tab contents.
  DCHECK(tab_contents_collector_map_.find(contents->type()) ==
         tab_contents_collector_map_.end());

  return contents;
}

void NavigationController::RegisterTabContents(TabContents* some_contents) {
  DCHECK(some_contents);
  TabContentsType t = some_contents->type();
  TabContents* tc;
  if ((tc = tab_contents_map_[t]) != some_contents) {
    if (tc) {
      NOTREACHED() << "Should not happen. Multiple contents for one type";
    } else {
      tab_contents_map_[t] = some_contents;
      some_contents->set_controller(this);
    }
  }
  if (some_contents->AsDOMUIHost())
    some_contents->AsDOMUIHost()->AttachMessageHandlers();
}

void NavigationController::NotifyEntryChangedByPageID(
    TabContentsType type,
    SiteInstance *instance,
    int32 page_id) {
  int index = GetEntryIndexWithPageID(type, instance, page_id);
  if (index != -1)
    NotifyEntryChanged(entries_[index], index);
}

// static
void NavigationController::DisablePromptOnRepost() {
  check_for_repost_ = false;
}

void NavigationController::SetActive(bool is_active) {
  if (is_active) {
    if (needs_reload_) {
      LoadIfNecessary();
    } else if (load_pending_entry_when_active_) {
      NavigateToPendingEntry(false);
      load_pending_entry_when_active_ = false;
    }
  }
}

void NavigationController::LoadIfNecessary() {
  if (!needs_reload_)
    return;

  needs_reload_ = false;
  // Calling Reload() results in ignoring state, and not loading.
  // Explicitly use NavigateToPendingEntry so that the renderer uses the
  // cached state.
  pending_entry_index_ = last_committed_entry_index_;
  NavigateToPendingEntry(false);
}

void NavigationController::NotifyEntryChanged(const NavigationEntry* entry,
                                              int index) {
  EntryChangedDetails det;
  det.changed_entry = entry;
  det.index = index;
  NotificationService::current()->Notify(NOTIFY_NAV_ENTRY_CHANGED,
                                         Source<NavigationController>(this),
                                         Details<EntryChangedDetails>(&det));
}

int NavigationController::GetMaxPageID() const {
  return active_contents_->GetMaxPageID();
}

NavigationController* NavigationController::Clone(HWND parent_hwnd) {
  NavigationController* nc = new NavigationController(NULL, profile_);

  if (GetEntryCount() == 0)
    return nc;

  nc->needs_reload_ = true;

  nc->entries_.reserve(entries_.size());
  for (int i = 0, c = GetEntryCount(); i < c; ++i)
    nc->entries_.push_back(new NavigationEntry(*GetEntryAtIndex(i)));

  nc->FinishRestore(parent_hwnd, last_committed_entry_index_);

  return nc;
}

void NavigationController::ScheduleTabContentsCollection(TabContentsType t) {
  TabContentsCollectorMap::const_iterator i =
      tab_contents_collector_map_.find(t);

  // The tab contents is already scheduled for collection.
  if (i != tab_contents_collector_map_.end())
    return;

  // If we currently don't have a TabContents for t, skip.
  if (tab_contents_map_.find(t) == tab_contents_map_.end())
    return;

  // Create a collector and schedule it.
  TabContentsCollector* tcc = new TabContentsCollector(this, t);
  tab_contents_collector_map_[t] = tcc;
  MessageLoop::current()->PostTask(FROM_HERE, tcc);
}

void NavigationController::CancelTabContentsCollection(TabContentsType t) {
  TabContentsCollectorMap::iterator i = tab_contents_collector_map_.find(t);

  if (i != tab_contents_collector_map_.end()) {
    DCHECK(i->second);
    i->second->Cancel();
    tab_contents_collector_map_.erase(i);
  }
}

void NavigationController::FinishRestore(HWND parent_hwnd, int selected_index) {
  DCHECK(selected_index >= 0 && selected_index < GetEntryCount());
  ConfigureEntriesForRestore(&entries_);

  set_max_restored_page_id(GetEntryCount());

  last_committed_entry_index_ = selected_index;

  // Callers assume we have an active_contents after restoring, so set it now.
  active_contents_ =
      GetTabContentsCreateIfNecessary(parent_hwnd, *entries_[selected_index]);
}
