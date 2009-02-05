// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/navigation_controller.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/dom_ui_host.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/repost_form_warning.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/common/navigation_types.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/resource_bundle.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_WIN)
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/repost_form_warning_dialog.h"
#endif

namespace {

// Invoked when entries have been pruned, or removed. For example, if the
// current entries are [google, digg, yahoo], with the current entry google,
// and the user types in cnet, then digg and yahoo are pruned.
void NotifyPrunedEntries(NavigationController* nav_controller,
                         bool from_front,
                         int count) {
  NavigationController::PrunedDetails details;
  details.from_front = from_front;
  details.count = count;
  NotificationService::current()->Notify(
      NotificationType::NAV_LIST_PRUNED,
      Source<NavigationController>(nav_controller),
      Details<NavigationController::PrunedDetails>(&details));
}

// Ensure the given NavigationEntry has a valid state, so that WebKit does not
// get confused if we navigate back to it.
// 
// An empty state is treated as a new navigation by WebKit, which would mean
// losing the navigation entries and generating a new navigation entry after
// this one. We don't want that. To avoid this we create a valid state which
// WebKit will not treat as a new navigation.
void SetContentStateIfEmpty(NavigationEntry* entry) {
  if (entry->content_state().empty() &&
      (entry->tab_type() == TAB_CONTENTS_WEB ||
       entry->tab_type() == TAB_CONTENTS_NEW_TAB_UI ||
       entry->tab_type() == TAB_CONTENTS_ABOUT_UI ||
       entry->tab_type() == TAB_CONTENTS_HTML_DIALOG ||
       entry->tab_type() == TAB_CONTENTS_VIEW_SOURCE)) {
    entry->set_content_state(
        webkit_glue::CreateHistoryStateForURL(entry->url()));
  }
}

// Configure all the NavigationEntries in entries for restore. This resets
// the transition type to reload and makes sure the content state isn't empty.
void ConfigureEntriesForRestore(
    std::vector<linked_ptr<NavigationEntry> >* entries) {
  for (size_t i = 0; i < entries->size(); ++i) {
    // Use a transition type of reload so that we don't incorrectly increase
    // the typed count.
    (*entries)[i]->set_transition_type(PageTransition::RELOAD);
    (*entries)[i]->set_restored(true);
    // NOTE(darin): This code is only needed for backwards compat.
    SetContentStateIfEmpty((*entries)[i].get());
  }
}

// See NavigationController::IsURLInPageNavigation for how this works and why.
bool AreURLsInPageNavigation(const GURL& existing_url, const GURL& new_url) {
  if (existing_url == new_url || !new_url.has_ref())
    return false;

  url_canon::Replacements<char> replacements;
  replacements.ClearRef();
  return existing_url.ReplaceComponents(replacements) ==
      new_url.ReplaceComponents(replacements);
}

}  // namespace

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
size_t NavigationController::max_entry_count_ = 50;

// static
bool NavigationController::check_for_repost_ = true;

// Creates a new NavigationEntry for each TabNavigation in navigations, adding
// the NavigationEntry to entries. This is used during session restore.
static void CreateNavigationEntriesFromTabNavigations(
    const std::vector<TabNavigation>& navigations,
    std::vector<linked_ptr<NavigationEntry> >* entries) {
  // Create a NavigationEntry for each of the navigations.
  int page_id = 0;
  for (std::vector<TabNavigation>::const_iterator i =
           navigations.begin(); i != navigations.end(); ++i, ++page_id) {
    entries->push_back(
        linked_ptr<NavigationEntry>(i->ToNavigationEntry(page_id)));
  }
}

NavigationController::NavigationController(TabContents* contents,
                                           Profile* profile)
    : profile_(profile),
      pending_entry_(NULL),
      last_committed_entry_index_(-1),
      pending_entry_index_(-1),
      transient_entry_index_(-1),
      active_contents_(contents),
      max_restored_page_id_(-1),
      ssl_manager_(this, NULL),
      needs_reload_(false),
      load_pending_entry_when_active_(false) {
  if (contents)
    RegisterTabContents(contents);
  DCHECK(profile_);
}

NavigationController::NavigationController(
    Profile* profile,
    const std::vector<TabNavigation>& navigations,
    int selected_navigation)
    : profile_(profile),
      pending_entry_(NULL),
      last_committed_entry_index_(-1),
      pending_entry_index_(-1),
      transient_entry_index_(-1),
      active_contents_(NULL),
      max_restored_page_id_(-1),
      ssl_manager_(this, NULL),
      needs_reload_(true),
      load_pending_entry_when_active_(false) {
  DCHECK(profile_);
  DCHECK(selected_navigation >= 0 &&
         selected_navigation < static_cast<int>(navigations.size()));

  // Populate entries_ from the supplied TabNavigations.
  CreateNavigationEntriesFromTabNavigations(navigations, &entries_);

  // And finish the restore.
  FinishRestore(selected_navigation);
}

NavigationController::~NavigationController() {
  DCHECK(tab_contents_map_.empty());
  DCHECK(tab_contents_collector_map_.empty());

  DiscardNonCommittedEntriesInternal();

  NotificationService::current()->Notify(
      NotificationType::TAB_CLOSED,
      Source<NavigationController>(this),
      NotificationService::NoDetails());
}

TabContents* NavigationController::GetTabContents(TabContentsType t) {
  // Make sure the TabContents is no longer scheduled for collection.
  CancelTabContentsCollection(t);
  return tab_contents_map_[t];
}

void NavigationController::Reload(bool check_for_repost) {
  // Reloading a transient entry does nothing.
  if (transient_entry_index_ != -1)
    return;

  DiscardNonCommittedEntriesInternal();
  int current_index = GetCurrentEntryIndex();
  if (check_for_repost_ && check_for_repost && current_index != -1 &&
      GetEntryAtIndex(current_index)->has_post_data()) {
    // The user is asking to reload a page with POST data. Prompt to make sure
    // they really want to do this. If they do, the dialog will call us back
    // with check_for_repost = false.
    active_contents_->Activate();
    RunRepostFormWarningDialog(this);
  } else {
    // Base the navigation on where we are now...
    int current_index = GetCurrentEntryIndex();

    // If we are no where, then we can't reload.  TODO(darin): We should add a
    // CanReload method.
    if (current_index == -1)
      return;

    DiscardNonCommittedEntriesInternal();

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
  DiscardNonCommittedEntriesInternal();
  pending_entry_ = entry;
  NotificationService::current()->Notify(
      NotificationType::NAV_ENTRY_PENDING,
      Source<NavigationController>(this),
      NotificationService::NoDetails());
  NavigateToPendingEntry(false);
}

NavigationEntry* NavigationController::GetActiveEntry() const {
  if (transient_entry_index_ != -1)
    return entries_[transient_entry_index_].get();
  if (pending_entry_)
    return pending_entry_;
  return GetLastCommittedEntry();
}

int NavigationController::GetCurrentEntryIndex() const {
  if (transient_entry_index_ != -1)
    return transient_entry_index_;
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
  int index = (transient_entry_index_ != -1) ?
                  transient_entry_index_ + offset :
                  last_committed_entry_index_ + offset;
  if (index < 0 || index >= GetEntryCount())
    return NULL;

  return entries_[index].get();
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

  DiscardNonCommittedEntries();

  pending_entry_index_ = current_index - 1;
  NavigateToPendingEntry(false);
}

void NavigationController::GoForward() {
  if (!CanGoForward()) {
    NOTREACHED();
    return;
  }

  bool transient = (transient_entry_index_ != -1);

  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  DiscardNonCommittedEntries();

  pending_entry_index_ = current_index;
  // If there was a transient entry, we removed it making the current index
  // the next page.
  if (!transient)
    pending_entry_index_++;

  NavigateToPendingEntry(false);
}

void NavigationController::GoToIndex(int index) {
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    NOTREACHED();
    return;
  }

  if (transient_entry_index_ != -1) {
    if (index == transient_entry_index_) {
      // Nothing to do when navigating to the transient.
      return;
    }
    if (index > transient_entry_index_) {
      // Removing the transient is goint to shift all entries by 1.
      index--;
    }
  }

  DiscardNonCommittedEntries();

  pending_entry_index_ = index;
  NavigateToPendingEntry(false);
}

void NavigationController::GoToOffset(int offset) {
  int index = (transient_entry_index_ != -1) ?
                  transient_entry_index_ + offset :
                  last_committed_entry_index_ + offset;
  if (index < 0 || index >= GetEntryCount())
    return;

  GoToIndex(index);
}

void NavigationController::RemoveEntryAtIndex(int index,
                                              const GURL& default_url) {
  int size = static_cast<int>(entries_.size());
  DCHECK(index < size);

  DiscardNonCommittedEntries();

  entries_.erase(entries_.begin() + index);

  if (last_committed_entry_index_ == index) {
    last_committed_entry_index_--;
    // We removed the currently shown entry, so we have to load something else.
    if (last_committed_entry_index_ != -1) {
      pending_entry_index_ = last_committed_entry_index_;
      NavigateToPendingEntry(false);
    } else {
      // If there is nothing to show, show a default page.
      LoadURL(default_url.is_empty() ? GURL("about:blank") : default_url,
              GURL(), PageTransition::START_PAGE);
    }
  } else if (last_committed_entry_index_ > index) {
    last_committed_entry_index_--;
  }
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
    const GURL& url, const GURL& referrer, PageTransition::Type transition) {
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
                                               referrer,
                                               std::wstring(), transition);
  entry->set_display_url(url);
  entry->set_user_typed_url(url);
  if (url.SchemeIsFile()) {
    entry->set_title(file_util::GetFilenameFromPath(UTF8ToWide(url.host() +
                                                               url.path())));
  }
  return entry;
}

void NavigationController::AddTransientEntry(NavigationEntry* entry) {
  // Discard any current transient entry, we can only have one at a time.
  int index = 0;
  if (last_committed_entry_index_ != -1)
    index = last_committed_entry_index_ + 1;
  DiscardTransientEntry();
  entries_.insert(entries_.begin() + index, linked_ptr<NavigationEntry>(entry));
  transient_entry_index_  = index;
  active_contents_->NotifyNavigationStateChanged(
      TabContents::INVALIDATE_EVERYTHING);
}

void NavigationController::LoadURL(const GURL& url, const GURL& referrer,
                                   PageTransition::Type transition) {
  // The user initiated a load, we don't need to reload anymore.
  needs_reload_ = false;

  NavigationEntry* entry = CreateNavigationEntry(url, referrer, transition);

  LoadEntry(entry);
}

void NavigationController::LoadURLLazily(const GURL& url,
                                         const GURL& referrer,
                                         PageTransition::Type type,
                                         const std::wstring& title,
                                         SkBitmap* icon) {
  NavigationEntry* entry = CreateNavigationEntry(url, referrer, type);
  entry->set_title(title);
  if (icon)
    entry->favicon().set_bitmap(*icon);

  DiscardNonCommittedEntriesInternal();
  pending_entry_ = entry;
  load_pending_entry_when_active_ = true;
}

bool NavigationController::LoadingURLLazily() {
  return load_pending_entry_when_active_;
}

const std::wstring& NavigationController::GetLazyTitle() const {
  if (pending_entry_)
    return pending_entry_->GetTitleForDisplay();
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

bool NavigationController::RendererDidNavigate(
    const ViewHostMsg_FrameNavigate_Params& params,
    LoadCommittedDetails* details) {
  // Save the previous state before we clobber it.
  if (GetLastCommittedEntry()) {
    details->previous_url = GetLastCommittedEntry()->url();
    details->previous_entry_index = GetLastCommittedEntryIndex();
  } else {
    details->previous_url = GURL();
    details->previous_entry_index = -1;
  }

  // Assign the current site instance to any pending entry, so we can find it
  // later by calling GetEntryIndexWithPageID. We only care about this if the
  // pending entry is an existing navigation and not a new one (or else we
  // wouldn't care about finding it with GetEntryIndexWithPageID).
  //
  // TODO(brettw) this seems slightly bogus as we don't really know if the
  // pending entry is what this navigation is for. There is a similar TODO
  // w.r.t. the pending entry in RendererDidNavigateToNewPage.
  if (pending_entry_index_ >= 0)
    pending_entry_->set_site_instance(active_contents_->GetSiteInstance());

  // Do navigation-type specific actions. These will make and commit an entry.
  details->type = ClassifyNavigation(params);
  switch (details->type) {
    case NavigationType::NEW_PAGE:
      RendererDidNavigateToNewPage(params);
      break;
    case NavigationType::EXISTING_PAGE:
      RendererDidNavigateToExistingPage(params);
      break;
    case NavigationType::SAME_PAGE:
      RendererDidNavigateToSamePage(params);
      break;
    case NavigationType::IN_PAGE:
      RendererDidNavigateInPage(params);
      break;
    case NavigationType::NEW_SUBFRAME:
      RendererDidNavigateNewSubframe(params);
      break;
    case NavigationType::AUTO_SUBFRAME:
      if (!RendererDidNavigateAutoSubframe(params))
        return false;
      break;
    case NavigationType::NAV_IGNORE:
      // There is nothing we can do with this navigation, so we just return to
      // the caller that nothing has happened.
      return false;
    default:
      NOTREACHED();
  }

  // All committed entries should have nonempty content state so WebKit doesn't
  // get confused when we go back to them (see the function for details).
  SetContentStateIfEmpty(GetActiveEntry());

  // WebKit doesn't set the "auto" transition on meta refreshes properly (bug
  // 1051891) so we manually set it for redirects which we normally treat as
  // "non-user-gestures" where we want to update stuff after navigations.
  //
  // Note that the redirect check also checks for a pending entry to
  // differentiate real redirects from browser initiated navigations to a
  // redirected entry. This happens when you hit back to go to a page that was
  // the destination of a redirect, we don't want to treat it as a redirect
  // even though that's what its transition will be. See bug 1117048.
  //
  // TODO(brettw) write a test for this complicated logic.
  details->is_auto = (PageTransition::IsRedirect(params.transition) &&
                      !GetPendingEntry()) ||
      params.gesture == NavigationGestureAuto;

  // Now prep the rest of the details for the notification and broadcast.
  details->entry = GetActiveEntry();
  details->is_in_page = IsURLInPageNavigation(params.url);
  details->is_main_frame = PageTransition::IsMainFrame(params.transition);
  details->serialized_security_info = params.security_info;
  details->is_content_filtered = params.is_content_filtered;
  NotifyNavigationEntryCommitted(details);

  // It is now a safe time to schedule collection for any tab contents of a
  // different type, because a navigation is necessary to get back to them.
  ScheduleTabContentsCollectionForInactiveTabs();
  return true;
}

NavigationType::Type NavigationController::ClassifyNavigation(
    const ViewHostMsg_FrameNavigate_Params& params) const {
  // If a page makes a popup navigated to about blank, and then writes stuff
  // like a subframe navigated to a real site, we'll get a notification with an
  // invalid page ID. There's nothing we can do with these, so just ignore them.
  if (params.page_id == -1) {
    DCHECK(!GetActiveEntry()) << "Got an invalid page ID but we seem to be "
      " navigated to a valid page. This should be impossible.";
    return NavigationType::NAV_IGNORE;
  }

  if (params.page_id > active_contents_->GetMaxPageID()) {
    // Greater page IDs than we've ever seen before are new pages. We may or may
    // not have a pending entry for the page, and this may or may not be the
    // main frame.
    if (PageTransition::IsMainFrame(params.transition))
      return NavigationType::NEW_PAGE;

    // When this is a new subframe navigation, we should have a committed page
    // for which it's a suframe in. This may not be the case when an iframe is
    // navigated on a popup navigated to about:blank (the iframe would be
    // written into the popup by script on the main page). For these cases,
    // there isn't any navigation stuff we can do, so just ignore it.
    if (!GetLastCommittedEntry())
      return NavigationType::NAV_IGNORE;

    // Valid subframe navigation.
    return NavigationType::NEW_SUBFRAME;
  }

  // Now we know that the notification is for an existing page. Find that entry.
  int existing_entry_index = GetEntryIndexWithPageID(
      active_contents_->type(),
      active_contents_->GetSiteInstance(),
      params.page_id);
  if (existing_entry_index == -1) {
    // The page was not found. It could have been pruned because of the limit on
    // back/forward entries (not likely since we'll usually tell it to navigate
    // to such entries). It could also mean that the renderer is smoking crack.
    NOTREACHED();
    return NavigationType::NAV_IGNORE;
  }
  NavigationEntry* existing_entry = entries_[existing_entry_index].get();

  if (pending_entry_ &&
      pending_entry_->url() == params.url &&
      existing_entry != pending_entry_ &&
      pending_entry_->page_id() == -1 &&
      pending_entry_->url() == existing_entry->url()) {
    // In this case, we have a pending entry for a URL but WebCore didn't do a
    // new navigation. This happens when you press enter in the URL bar to
    // reload. We will create a pending entry, but WebKit will convert it to
    // a reload since it's the same page and not create a new entry for it
    // (the user doesn't want to have a new back/forward entry when they do
    // this). In this case, we want to just ignore the pending entry and go
    // back to where we were (the "existing entry").
    return NavigationType::SAME_PAGE;
  }

  if (!PageTransition::IsMainFrame(params.transition)) {
    // All manual subframes would get new IDs and were handled above, so we
    // know this is auto. Since the current page was found in the navigation
    // entry list, we're guaranteed to have a last committed entry.
    DCHECK(GetLastCommittedEntry());
    return NavigationType::AUTO_SUBFRAME;
  }

  // Any toplevel navigations with the same base (minus the reference fragment)
  // are in-page navigations. We weeded out subframe navigations above. Most of
  // the time this doesn't matter since WebKit doesn't tell us about subframe
  // navigations that don't actually navigate, but it can happen when there is
  // an encoding override (it always sends a navigation request).
  if (AreURLsInPageNavigation(existing_entry->url(), params.url))
    return NavigationType::IN_PAGE;

  // Since we weeded out "new" navigations above, we know this is an existing
  // (back/forward) navigation.
  return NavigationType::EXISTING_PAGE;
}

void NavigationController::RendererDidNavigateToNewPage(
    const ViewHostMsg_FrameNavigate_Params& params) {
  NavigationEntry* new_entry;
  if (pending_entry_) {
    // TODO(brettw) this assumes that the pending entry is appropriate for the
    // new page that was just loaded. I don't think this is necessarily the
    // case! We should have some more tracking to know for sure. This goes along
    // with a similar TODO at the top of RendererDidNavigate where we blindly
    // set the site instance on the pending entry.
    new_entry = new NavigationEntry(*pending_entry_);

    // Don't use the page type from the pending entry. Some interstitial page
    // may have set the type to interstitial. Once we commit, however, the page
    // type must always be normal.
    new_entry->set_page_type(NavigationEntry::NORMAL_PAGE);
  } else {
    new_entry = new NavigationEntry(active_contents_->type());
  }

  new_entry->set_url(params.url);
  new_entry->set_page_id(params.page_id);
  new_entry->set_transition_type(params.transition);
  new_entry->set_site_instance(active_contents_->GetSiteInstance());
  new_entry->set_has_post_data(params.is_post);

  InsertEntry(new_entry);
}

void NavigationController::RendererDidNavigateToExistingPage(
    const ViewHostMsg_FrameNavigate_Params& params) {
  // We should only get here for main frame navigations.
  DCHECK(PageTransition::IsMainFrame(params.transition));

  // This is a back/forward navigation. The existing page for the ID is
  // guaranteed to exist by ClassifyNavigation, and we just need to update it
  // with new information from the renderer.
  int entry_index = GetEntryIndexWithPageID(
      active_contents_->type(),
      active_contents_->GetSiteInstance(),
      params.page_id);
  DCHECK(entry_index >= 0 &&
         entry_index < static_cast<int>(entries_.size()));
  NavigationEntry* entry = entries_[entry_index].get();

  // The URL may have changed due to redirects. The site instance will normally
  // be the same except during session restore, when no site instance will be
  // assigned.
  entry->set_url(params.url);
  DCHECK(entry->site_instance() == NULL ||
         entry->site_instance() == active_contents_->GetSiteInstance());
  entry->set_site_instance(active_contents_->GetSiteInstance());

  // The entry we found in the list might be pending if the user hit
  // back/forward/reload. This load should commit it (since it's already in the
  // list, we can just discard the pending pointer).
  //
  // Note that we need to use the "internal" version since we don't want to
  // actually change any other state, just kill the pointer.
  if (entry == pending_entry_)
    DiscardNonCommittedEntriesInternal();
  
  last_committed_entry_index_ = entry_index;
}

void NavigationController::RendererDidNavigateToSamePage(
    const ViewHostMsg_FrameNavigate_Params& params) {
  // This mode implies we have a pending entry that's the same as an existing
  // entry for this page ID. This entry is guaranteed to exist by
  // ClassifyNavigation. All we need to do is update the existing entry.
  NavigationEntry* existing_entry = GetEntryWithPageID(
      active_contents_->type(),
      active_contents_->GetSiteInstance(),
      params.page_id);

  // We assign the entry's unique ID to be that of the new one. Since this is
  // always the result of a user action, we want to dismiss infobars, etc. like
  // a regular user-initiated navigation.
  existing_entry->set_unique_id(pending_entry_->unique_id());

  DiscardNonCommittedEntries();
}

void NavigationController::RendererDidNavigateInPage(
    const ViewHostMsg_FrameNavigate_Params& params) {
  DCHECK(PageTransition::IsMainFrame(params.transition)) <<
      "WebKit should only tell us about in-page navs for the main frame.";
  // We're guaranteed to have an entry for this one.
  NavigationEntry* existing_entry = GetEntryWithPageID(
      active_contents_->type(),
      active_contents_->GetSiteInstance(),
      params.page_id);

  // Reference fragment navigation. We're guaranteed to have the last_committed
  // entry and it will be the same page as the new navigation (minus the
  // reference fragments, of course).
  NavigationEntry* new_entry = new NavigationEntry(*existing_entry);
  new_entry->set_page_id(params.page_id);
  new_entry->set_url(params.url);
  InsertEntry(new_entry);
}

void NavigationController::RendererDidNavigateNewSubframe(
    const ViewHostMsg_FrameNavigate_Params& params) {
  // Manual subframe navigations just get the current entry cloned so the user
  // can go back or forward to it. The actual subframe information will be
  // stored in the page state for each of those entries. This happens out of
  // band with the actual navigations.
  DCHECK(GetLastCommittedEntry()) << "ClassifyNavigation should guarantee "
                                  << "that a last committed entry exists.";
  NavigationEntry* new_entry = new NavigationEntry(*GetLastCommittedEntry());
  new_entry->set_page_id(params.page_id);
  InsertEntry(new_entry);
}

bool NavigationController::RendererDidNavigateAutoSubframe(
    const ViewHostMsg_FrameNavigate_Params& params) {
  // We're guaranteed to have a previously committed entry, and we now need to
  // handle navigation inside of a subframe in it without creating a new entry.
  DCHECK(GetLastCommittedEntry());

  // Handle the case where we're navigating back/forward to a previous subframe
  // navigation entry. This is case "2." in NAV_AUTO_SUBFRAME comment in the
  // header file. In case "1." this will be a NOP.
  int entry_index = GetEntryIndexWithPageID(
      active_contents_->type(),
      active_contents_->GetSiteInstance(),
      params.page_id);
  if (entry_index < 0 ||
      entry_index >= static_cast<int>(entries_.size())) {
    NOTREACHED();
    return false;
  }

  // Update the current navigation entry in case we're going back/forward.
  if (entry_index != last_committed_entry_index_) {
    last_committed_entry_index_ = entry_index;
    return true;
  }
  return false;
}

void NavigationController::CommitPendingEntry() {
  DiscardTransientEntry();

  if (!GetPendingEntry())
    return;  // Nothing to do.

  // Need to save the previous URL for the notification.
  LoadCommittedDetails details;
  if (GetLastCommittedEntry()) {
    details.previous_url = GetLastCommittedEntry()->url();
    details.previous_entry_index = GetLastCommittedEntryIndex();
  } else {
    details.previous_entry_index = -1;
  }

  if (pending_entry_index_ >= 0) {
    // This is a previous navigation (back/forward) that we're just now
    // committing. Just mark it as committed.
    details.type = NavigationType::EXISTING_PAGE;
    int new_entry_index = pending_entry_index_;
    DiscardNonCommittedEntriesInternal();

    // Mark that entry as committed.
    last_committed_entry_index_ = new_entry_index;
  } else {
    // This is a new navigation. It's easiest to just copy the entry and insert
    // it new again, since InsertEntry expects to take ownership and also
    // discard the pending entry. We also need to synthesize a page ID. We can
    // only do this because this function will only be called by our custom
    // TabContents types. For WebContents, the IDs are generated by the
    // renderer, so we can't do this.
    details.type = NavigationType::NEW_PAGE;
    pending_entry_->set_page_id(active_contents_->GetMaxPageID() + 1);
    active_contents_->UpdateMaxPageID(pending_entry_->page_id());
    InsertEntry(new NavigationEntry(*pending_entry_));
  }

  // Broadcast the notification of the navigation.
  details.entry = GetActiveEntry();
  details.is_auto = false;
  details.is_in_page = AreURLsInPageNavigation(details.previous_url,
                                               details.entry->url());
  details.is_main_frame = true;
  NotifyNavigationEntryCommitted(&details);
}

int NavigationController::GetIndexOfEntry(
    const NavigationEntry* entry) const {
  const NavigationEntries::const_iterator i(std::find(
      entries_.begin(),
      entries_.end(),
      entry));
  return (i == entries_.end()) ? -1 : static_cast<int>(i - entries_.begin());
}

bool NavigationController::IsURLInPageNavigation(const GURL& url) const {
  NavigationEntry* last_committed = GetLastCommittedEntry();
  if (!last_committed)
    return false;
  return AreURLsInPageNavigation(last_committed->url(), url);
}

void NavigationController::DiscardNonCommittedEntries() {
  bool transient = transient_entry_index_ != -1;
  DiscardNonCommittedEntriesInternal();

  // Synchronize the active_contents_ to the last committed entry.
  NavigationEntry* last_entry = GetLastCommittedEntry();
  if (last_entry && last_entry->tab_type() != active_contents_->type()) {
    TabContents* from_contents = active_contents_;
    from_contents->set_is_active(false);

    // Switch back to the previous tab contents.
    active_contents_ = GetTabContents(last_entry->tab_type());
    DCHECK(active_contents_);

    active_contents_->set_is_active(true);

    // If we are transitioning from two types of WebContents, we need to migrate
    // the download shelf if it is visible. The download shelf may have been
    // created before the error that caused us to discard the entry.
    TabContents::MigrateShelfView(from_contents, active_contents_);

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

  // If there was a transient entry, invalidate everything so the new active
  // entry state is shown.
  if (transient) {
    active_contents_->NotifyNavigationStateChanged(
        TabContents::INVALIDATE_EVERYTHING);
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

  DiscardNonCommittedEntriesInternal();

  int current_size = static_cast<int>(entries_.size());

  // Prune any entries which are in front of the current entry.
  if (current_size > 0) {
    int num_pruned = 0;
    while (last_committed_entry_index_ < (current_size - 1)) {
      num_pruned++;
      entries_.pop_back();
      current_size--;
    }
    if (num_pruned > 0)  // Only notify if we did prune something.
      NotifyPrunedEntries(this, false, num_pruned);
  }

  if (entries_.size() >= max_entry_count_) {
    RemoveEntryAtIndex(0, GURL());
    NotifyPrunedEntries(this, true, 1);
  }

  entries_.push_back(linked_ptr<NavigationEntry>(entry));
  last_committed_entry_index_ = static_cast<int>(entries_.size()) - 1;

  // This is a new page ID, so we need everybody to know about it.
  active_contents_->UpdateMaxPageID(entry->page_id());
}

void NavigationController::SetWindowID(const SessionID& id) {
  window_id_ = id;
  NotificationService::current()->Notify(NotificationType::TAB_PARENTED,
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
    from_contents->set_is_active(false);

  TabContents* contents = GetTabContentsCreateIfNecessary(*pending_entry_);

  contents->set_is_active(true);
  active_contents_ = contents;

  if (from_contents && from_contents != contents && from_contents->delegate())
    from_contents->delegate()->ReplaceContents(from_contents, contents);

  NavigationEntry temp_entry(*pending_entry_);
  if (!contents->NavigateToPendingEntry(reload))
    DiscardNonCommittedEntries();
}

void NavigationController::NotifyNavigationEntryCommitted(
    LoadCommittedDetails* details) {
  // TODO(pkasting): http://b/1113079 Probably these explicit notification paths
  // should be removed, and interested parties should just listen for the
  // notification below instead.
  ssl_manager_.NavigationStateChanged();
  active_contents_->NotifyNavigationStateChanged(
      TabContents::INVALIDATE_EVERYTHING);

  details->entry = GetActiveEntry();
  NotificationService::current()->Notify(
      NotificationType::NAV_ENTRY_COMMITTED,
      Source<NavigationController>(this),
      Details<LoadCommittedDetails>(details));
}

TabContents* NavigationController::GetTabContentsCreateIfNecessary(
    const NavigationEntry& entry) {
  TabContents* contents = GetTabContents(entry.tab_type());
  if (!contents) {
    contents = TabContents::CreateWithType(entry.tab_type(), profile_,
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
  NotificationService::current()->Notify(NotificationType::NAV_ENTRY_CHANGED,
                                         Source<NavigationController>(this),
                                         Details<EntryChangedDetails>(&det));
}

NavigationController* NavigationController::Clone() {
  NavigationController* nc = new NavigationController(NULL, profile_);

  if (GetEntryCount() == 0)
    return nc;

  nc->needs_reload_ = true;

  nc->entries_.reserve(entries_.size());
  for (int i = 0, c = GetEntryCount(); i < c; ++i) {
    nc->entries_.push_back(linked_ptr<NavigationEntry>(
        new NavigationEntry(*GetEntryAtIndex(i))));
  }

  nc->FinishRestore(last_committed_entry_index_);

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

void NavigationController::FinishRestore(int selected_index) {
  DCHECK(selected_index >= 0 && selected_index < GetEntryCount());
  ConfigureEntriesForRestore(&entries_);

  set_max_restored_page_id(GetEntryCount());

  last_committed_entry_index_ = selected_index;

  // Callers assume we have an active_contents after restoring, so set it now.
  active_contents_ = GetTabContentsCreateIfNecessary(*entries_[selected_index]);
}

void NavigationController::DiscardNonCommittedEntriesInternal() {
  if (pending_entry_index_ == -1)
    delete pending_entry_;
  pending_entry_ = NULL;
  pending_entry_index_ = -1;

  DiscardTransientEntry();
}

void NavigationController::DiscardTransientEntry() {
  if (transient_entry_index_ == -1)
    return;
  entries_.erase(entries_.begin() + transient_entry_index_ );
  transient_entry_index_ = -1;
}

int NavigationController::GetEntryIndexWithPageID(
    TabContentsType type, SiteInstance* instance, int32 page_id) const {
  for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
    if ((entries_[i]->tab_type() == type) &&
        (entries_[i]->site_instance() == instance) &&
        (entries_[i]->page_id() == page_id))
      return i;
  }
  return -1;
}

NavigationEntry* NavigationController::GetTransientEntry() const {
  if (transient_entry_index_ == -1)
    return NULL;
  return entries_[transient_entry_index_].get();
}
