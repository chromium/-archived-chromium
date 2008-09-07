// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "webkit/glue/webkit_glue.h"

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

// The maximum number of entries that a navigation controller can store.
// static
const static size_t kMaxEntryCount = 50;

// static
bool NavigationController::check_for_repost_ = true;

// Creates a new NavigationEntry for each TabNavigation in navigations, adding
// the NavigationEntry to entries. This is used during session restore.
static void CreateNavigationEntriesFromTabNavigations(
    const std::vector<TabNavigation>& navigations,
    std::vector<linked_ptr<NavigationEntry> >* entries) {
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
    entry->set_display_url(navigation.url);
    entry->set_content_state(navigation.state);
    entry->set_has_post_data(
        navigation.type_mask & TabNavigation::HAS_POST_DATA);
    entries->push_back(linked_ptr<NavigationEntry>(entry));
  }
}

// Configure all the NavigationEntries in entries for restore. This resets
// the transition type to reload and makes sure the content state isn't empty.
static void ConfigureEntriesForRestore(
    std::vector<linked_ptr<NavigationEntry> >* entries) {
  for (size_t i = 0, count = entries->size(); i < count; ++i) {
    // Use a transition type of reload so that we don't incorrectly increase
    // the typed count.
    (*entries)[i]->set_transition_type(PageTransition::RELOAD);
    (*entries)[i]->set_restored(true);
    // NOTE(darin): This code is only needed for backwards compat.
    NavigationController::SetContentStateIfEmpty((*entries)[i].get());
  }
}

NavigationController::NavigationController(TabContents* contents,
                                           Profile* profile)
    : profile_(profile),
      pending_entry_(NULL),
      last_committed_entry_index_(-1),
      pending_entry_index_(-1),
      max_entry_count_(kMaxEntryCount),
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
      pending_entry_(NULL),
      last_committed_entry_index_(-1),
      pending_entry_index_(-1),
      max_entry_count_(kMaxEntryCount),
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

  DiscardPendingEntryInternal();

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

void NavigationController::Reload() {
  // TODO(pkasting): http://b/1113085 Should this use DiscardPendingEntry()?
  DiscardPendingEntryInternal();
  int current_index = GetCurrentEntryIndex();
  if (check_for_repost_ && current_index != -1 &&
      GetEntryAtIndex(current_index)->has_post_data() &&
      active_contents_->AsWebContents() &&
      !active_contents_->AsWebContents()->showing_repost_interstitial()) {
    // The user is asking to reload a page with POST data and we're not showing
    // the POST interstitial. Prompt to make sure they really want to do this.
    // If they do, RepostFormWarningDialog calls us back with
    // ReloadDontCheckForRepost.
    active_contents_->Activate();
    RepostFormWarningDialog::RunRepostFormWarningDialog(this);
  } else {
    // Base the navigation on where we are now...
    int current_index = GetCurrentEntryIndex();

    // If we are no where, then we can't reload.  TODO(darin): We should add a
    // CanReload method.
    if (current_index == -1)
      return;

    // TODO(pkasting): http://b/1113085 Should this use DiscardPendingEntry()?
    DiscardPendingEntryInternal();

    pending_entry_index_ = current_index;
    entries_[pending_entry_index_]->set_transition_type(PageTransition::RELOAD);
    NavigateToPendingEntry(true);
  }
}

NavigationEntry* NavigationController::GetEntryWithPageID(
    TabContentsType type, SiteInstance* instance, int32 page_id) const {
  int index = GetEntryIndexWithPageID(type, instance, page_id);
  return (index != -1) ? entries_[index].get() : NULL;
}

void NavigationController::LoadEntry(NavigationEntry* entry) {
  // When navigating to a new page, we don't know for sure if we will actually
  // end up leaving the current page.  The new page load could for example
  // result in a download or a 'no content' response (e.g., a mailto: URL).

  // TODO(pkasting): http://b/1113085 Should this use DiscardPendingEntry()?
  DiscardPendingEntryInternal();
  pending_entry_ = entry;
  NotificationService::current()->Notify(
      NOTIFY_NAV_ENTRY_PENDING,
      Source<NavigationController>(this),
      NotificationService::NoDetails());
  NavigateToPendingEntry(false);
}

/* static */
void NavigationController::SetContentStateIfEmpty(
    NavigationEntry* entry) {
  if (entry->content_state().empty() &&
      (entry->tab_type() == TAB_CONTENTS_WEB ||
       entry->tab_type() == TAB_CONTENTS_NEW_TAB_UI ||
       entry->tab_type() == TAB_CONTENTS_ABOUT_UI ||
       entry->tab_type() == TAB_CONTENTS_HTML_DIALOG)) {
    // The state is empty and the url will be rendered by WebKit. An empty
    // state is treated as a new navigation by WebKit, which would mean
    // losing the navigation entries and generating a new navigation
    // entry after this one. We don't want that. To avoid this we create
    // a valid state which WebKit will not treat as a new navigation.
    entry->set_content_state(
        webkit_glue::CreateHistoryStateForURL(entry->url()));
  }
}

NavigationEntry* NavigationController::GetActiveEntry() const {
  NavigationEntry* entry = pending_entry_;
  if (!entry)
    entry = GetLastCommittedEntry();
  return entry;
}

int NavigationController::GetCurrentEntryIndex() const {
  if (pending_entry_index_ != -1)
    return pending_entry_index_;
  return last_committed_entry_index_;
}

NavigationEntry* NavigationController::GetLastCommittedEntry() const {
  if (last_committed_entry_index_ == -1)
    return NULL;
  return entries_[last_committed_entry_index_].get();
}

NavigationEntry* NavigationController::GetEntryAtOffset(int offset) const {
  int index = last_committed_entry_index_ + offset;
  if (index < 0 || index >= GetEntryCount())
    return NULL;

  return entries_[index].get();
}

bool NavigationController::CanStop() const {
  // TODO(darin): do we have something pending that we can stop?
  return false;
}

bool NavigationController::CanGoBack() const {
  return entries_.size() > 1 && GetCurrentEntryIndex() > 0;
}

bool NavigationController::CanGoForward() const {
  int index = GetCurrentEntryIndex();
  return index >= 0 && index < (static_cast<int>(entries_.size()) - 1);
}

void NavigationController::GoBack() {
  if (!CanGoBack()) {
    NOTREACHED();
    return;
  }

  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  DiscardPendingEntry();

  pending_entry_index_ = current_index - 1;
  NavigateToPendingEntry(false);
}

void NavigationController::GoForward() {
  if (!CanGoForward()) {
    NOTREACHED();
    return;
  }

  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  DiscardPendingEntry();

  pending_entry_index_ = current_index + 1;
  NavigateToPendingEntry(false);
}

void NavigationController::GoToIndex(int index) {
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    NOTREACHED();
    return;
  }

  DiscardPendingEntry();

  pending_entry_index_ = index;
  NavigateToPendingEntry(false);
}

void NavigationController::GoToOffset(int offset) {
  int index = last_committed_entry_index_ + offset;
  if (index < 0 || index >= GetEntryCount())
    return;

  GoToIndex(index);
}

void NavigationController::Stop() {
  DCHECK(CanStop());

  // TODO(darin): we probably want to just call Stop on the active tab
  // contents, but should we also call DiscardPendingEntry?
  NOTREACHED() << "implement me";
}

void NavigationController::ReloadDontCheckForRepost() {
  Reload();
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
  entry->set_display_url(url);
  entry->set_user_typed_url(url);
  if (url.SchemeIsFile()) {
    entry->set_title(file_util::GetFilenameFromPath(UTF8ToWide(url.host() +
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
  entry->set_title(title);
  if (icon)
    entry->favicon().set_bitmap(*icon);

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
    return pending_entry_->title();
  else
    return EmptyWString();
}

const SkBitmap& NavigationController::GetLazyFavIcon() const {
  if (pending_entry_) {
    return pending_entry_->favicon().bitmap();
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

void NavigationController::DidNavigateToEntry(NavigationEntry* entry,
                                              LoadCommittedDetails* details) {
  DCHECK(active_contents_);
  DCHECK(entry->tab_type() == active_contents_->type());

  SetContentStateIfEmpty(entry);

  entry->set_restored(false);

  // Update the details to list the last URL. Later, we'll update the current
  // entry (after it's committed) and the details will be complete.
  if (GetLastCommittedEntry())
    details->previous_url = GetLastCommittedEntry()->url();

  // If the entry is that of a page with PageID larger than any this Tab has
  // seen before, then consider it a new navigation.  Note that if the entry
  // has a SiteInstance, it should be the same as the SiteInstance of the
  // active WebContents, because we have just navigated to it.
  DCHECK(entry->page_id() >= 0) << "Page ID must be set before calling us.";
  if (entry->page_id() > GetMaxPageID()) {
    InsertEntry(entry);
    NotifyNavigationEntryCommitted(details);
    // It is now a safe time to schedule collection for any tab contents of a
    // different type, because a navigation is necessary to get back to them.
    ScheduleTabContentsCollectionForInactiveTabs();
    return;
  }

  // Otherwise, we just need to update an existing entry with matching PageID.
  // If the existing entry corresponds to the entry which is pending, then we
  // must update the current entry index accordingly.  When navigating to the
  // same URL, a new PageID is not created.

  int existing_entry_index = GetEntryIndexWithPageID(entry->tab_type(),
                                                     entry->site_instance(),
                                                     entry->page_id());
  NavigationEntry* existing_entry = (existing_entry_index != -1) ?
      entries_[existing_entry_index].get() : NULL;
  if (!existing_entry) {
    // No existing entry, then simply ignore this navigation!
    DLOG(WARNING) << "ignoring navigation for page: " << entry->page_id();
  } else if ((existing_entry != pending_entry_) && pending_entry_ &&
             (pending_entry_->page_id() == -1) &&
             (pending_entry_->url() == existing_entry->url())) {
    // In this case, we have a pending entry for a URL but WebCore didn't do a
    // new navigation. This happens when you press enter in the URL bar to
    // reload. We will create a pending entry, but WebCore will convert it to
    // a reload since it's the same page and not create a new entry for it
    // (the user doesn't want to have a new back/forward entry when they do
    // this). In this case, we want to just ignore the pending entry and go back
    // to where we were.
    existing_entry->set_unique_id(pending_entry_->unique_id());
    DiscardPendingEntry();
  } else {
    DCHECK(existing_entry != entry);
    // The given entry might provide a new URL... e.g., navigating back to a
    // page in session history could have resulted in a new client redirect.
    // The given entry might also provide a new title (typically an empty title
    // to overwrite the existing title).
    existing_entry->set_url(entry->url());
    existing_entry->set_title(entry->title());
    existing_entry->favicon() = entry->favicon();
    existing_entry->set_content_state(entry->content_state());

    // TODO(brettw) why only copy the security style and no other SSL stuff?
    existing_entry->ssl().set_security_style(entry->ssl().security_style());

    const int prev_entry_index = last_committed_entry_index_;
    if (existing_entry == pending_entry_) {
      DCHECK(pending_entry_index_ != -1);
      last_committed_entry_index_ = pending_entry_index_;
      // TODO(pkasting): http://b/1113085 Should this use DiscardPendingEntry()?
      DiscardPendingEntryInternal();
    } else {
      // NOTE: Do not update the unique ID here, as we don't want infobars etc.
      // to dismiss.

      // The navigation could have been issued by the renderer, so be sure that
      // we update our current index.
      last_committed_entry_index_ = existing_entry_index;
    }
    IndexOfActiveEntryChanged(prev_entry_index);
  }

  delete entry;
  NotifyNavigationEntryCommitted(details);

  if (alternate_nav_url_fetcher_.get()) {
    // Because this call may synchronously show an infobar, we do it last, to
    // make sure all other state is stable and the infobar won't get blown away
    // by some transition.
    alternate_nav_url_fetcher_->OnNavigatedToEntry();
  }

  // It is now a safe time to schedule collection for any tab contents of a
  // different type, because a navigation is necessary to get back to them.
  ScheduleTabContentsCollectionForInactiveTabs();
}


int NavigationController::GetIndexOfEntry(
    const NavigationEntry* entry) const {
  const NavigationEntries::const_iterator i(std::find(
      entries_.begin(),
      entries_.end(),
      entry));
  return (i == entries_.end()) ? -1 : static_cast<int>(i - entries_.begin());
}

void NavigationController::RemoveLastEntry() {
  int current_size = static_cast<int>(entries_.size());

  if (current_size > 0) {
    if (pending_entry_ == entries_[current_size - 1] ||
        pending_entry_index_ == current_size - 1)
      DiscardPendingEntryInternal();

    entries_.pop_back();

    if (last_committed_entry_index_ >= current_size - 1)
      last_committed_entry_index_ = current_size - 2;

    NotifyPrunedEntries();
  }
}

void NavigationController::DiscardPendingEntry() {
  DiscardPendingEntryInternal();

  // Synchronize the active_contents_ to the last committed entry.
  NavigationEntry* last_entry = GetLastCommittedEntry();
  if (last_entry && last_entry->tab_type() != active_contents_->type()) {
    TabContents* from_contents = active_contents_;
    from_contents->SetActive(false);

    // Switch back to the previous tab contents.
    active_contents_ = GetTabContents(last_entry->tab_type());
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
  DCHECK(entry->transition_type() != PageTransition::AUTO_SUBFRAME);

  // Copy the pending entry's unique ID to the committed entry.
  // I don't know if pending_entry_index_ can be other than -1 here.
  const NavigationEntry* const pending_entry = (pending_entry_index_ == -1) ?
      pending_entry_ : entries_[pending_entry_index_].get();
  if (pending_entry)
    entry->set_unique_id(pending_entry->unique_id());

  DiscardPendingEntryInternal();

  int current_size = static_cast<int>(entries_.size());

  // Prune any entries which are in front of the current entry.
  if (current_size > 0) {
    bool pruned = false;
    while (last_committed_entry_index_ < (current_size - 1)) {
      pruned = true;
      entries_.pop_back();
      current_size--;
    }
    if (pruned)  // Only notify if we did prune something.
      NotifyPrunedEntries();
  }

  if (entries_.size() >= max_entry_count_)
    RemoveEntryAtIndex(0);

  entries_.push_back(linked_ptr<NavigationEntry>(entry));
  last_committed_entry_index_ = static_cast<int>(entries_.size()) - 1;
  
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
    pending_entry_ = entries_[pending_entry_index_].get();
  }

  // Reset the security states as any SSL error may have been resolved since we
  // last visited that page.
  pending_entry_->ssl() = NavigationEntry::SSLStatus();

  if (from_contents && from_contents->type() != pending_entry_->tab_type())
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

void NavigationController::NotifyNavigationEntryCommitted(
    LoadCommittedDetails* details) {
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

  details->entry = GetActiveEntry();
  NotificationService::current()->Notify(
      NOTIFY_NAV_ENTRY_COMMITTED,
      Source<NavigationController>(this),
      Details<LoadCommittedDetails>(details));
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
  TabContents* contents = GetTabContents(entry.tab_type());
  if (!contents) {
    contents = TabContents::CreateWithType(entry.tab_type(), parent, profile_,
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
        if (entries_[i]->tab_type() == entry.tab_type())
          max_page_id = std::max(max_page_id, entries_[i]->page_id());
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
    NotifyEntryChanged(entries_[index].get(), index);
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

void NavigationController::RemoveEntryAtIndex(int index) {
  // TODO(brettw) this is only called to remove the first one when we've got
  // too many entries. It should probably be more specific for this case.
  if (index >= static_cast<int>(entries_.size()) ||
      index == pending_entry_index_ || index == last_committed_entry_index_) {
    NOTREACHED();
    return;
  }

  entries_.erase(entries_.begin() + index);

  if (last_committed_entry_index_ >= index) {
    if (!entries_.empty())
      last_committed_entry_index_--;
    else
      last_committed_entry_index_ = -1;
  }

  // TODO(brettw) bug 1324021: we probably need some notification here so the
  // session service can stay in sync.
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
  for (int i = 0, c = GetEntryCount(); i < c; ++i) {
    nc->entries_.push_back(linked_ptr<NavigationEntry>(
        new NavigationEntry(*GetEntryAtIndex(i))));
  }

  nc->FinishRestore(parent_hwnd, last_committed_entry_index_);

  return nc;
}

void NavigationController::ScheduleTabContentsCollectionForInactiveTabs() {
  int index = GetCurrentEntryIndex();
  if (index < 0 || GetPendingEntryIndex() != -1)
    return;

  TabContentsType active_type = GetEntryAtIndex(index)->tab_type();
  for (TabContentsMap::iterator i = tab_contents_map_.begin();
       i != tab_contents_map_.end(); ++i) {
    if (i->first != active_type)
      ScheduleTabContentsCollection(i->first);
  }
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

void NavigationController::DiscardPendingEntryInternal() {
  if (pending_entry_index_ == -1)
    delete pending_entry_;
  pending_entry_ = NULL;
  pending_entry_index_ = -1;
}

int NavigationController::GetEntryIndexWithPageID(
    TabContentsType type, SiteInstance* instance, int32 page_id) const {
  // The instance should only be specified for contents displaying web pages.
  // TODO(evanm): checking against NEW_TAB_UI and HTML_DLG here is lame.
  // It'd be nice for DomUIHost to just use SiteInstances for keeping content
  // separated properly.
  if (type != TAB_CONTENTS_WEB &&
      type != TAB_CONTENTS_NEW_TAB_UI &&
      type != TAB_CONTENTS_ABOUT_UI &&
      type != TAB_CONTENTS_HTML_DIALOG &&
      type != TAB_CONTENTS_VIEW_SOURCE &&
      type != TAB_CONTENTS_DEBUGGER)
    DCHECK(instance == NULL);

  for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
    if ((entries_[i]->tab_type() == type) &&
        (entries_[i]->site_instance() == instance) &&
        (entries_[i]->page_id() == page_id))
      return i;
  }
  return -1;
}
