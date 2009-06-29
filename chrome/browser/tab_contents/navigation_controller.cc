// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/navigation_controller.h"

#include "app/resource_bundle.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_url_handler.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/repost_form_warning.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/navigation_types.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "grit/app_resources.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_WIN)
#include "chrome/browser/tab_contents/repost_form_warning.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
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
  if (entry->content_state().empty()) {
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

// Navigation within this limit since the last document load is considered to
// be automatic (i.e., machine-initiated) rather than user-initiated unless
// a user gesture has been observed.
const base::TimeDelta kMaxAutoNavigationTimeDelta =
    base::TimeDelta::FromSeconds(5);

}  // namespace

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
      tab_contents_(contents),
      max_restored_page_id_(-1),
      ALLOW_THIS_IN_INITIALIZER_LIST(ssl_manager_(this)),
      needs_reload_(false),
      load_pending_entry_when_active_(false) {
  DCHECK(profile_);
}

NavigationController::~NavigationController() {
  DiscardNonCommittedEntriesInternal();

  NotificationService::current()->Notify(
      NotificationType::TAB_CLOSED,
      Source<NavigationController>(this),
      NotificationService::NoDetails());
}

void NavigationController::RestoreFromState(
    const std::vector<TabNavigation>& navigations,
    int selected_navigation) {
  // Verify that this controller is unused and that the input is valid.
  DCHECK(entry_count() == 0 && !pending_entry());
  DCHECK(selected_navigation >= 0 &&
         selected_navigation < static_cast<int>(navigations.size()));

  // Populate entries_ from the supplied TabNavigations.
  needs_reload_ = true;
  CreateNavigationEntriesFromTabNavigations(navigations, &entries_);

  // And finish the restore.
  FinishRestore(selected_navigation);
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
    tab_contents_->Activate();
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
    SiteInstance* instance, int32 page_id) const {
  int index = GetEntryIndexWithPageID(instance, page_id);
  return (index != -1) ? entries_[index].get() : NULL;
}

void NavigationController::LoadEntry(NavigationEntry* entry) {
  // Handle non-navigational URLs that popup dialogs and such, these should not
  // actually navigate.
  if (HandleNonNavigationAboutURL(entry->url()))
    return;

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
  if (index < 0 || index >= entry_count())
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
  if (index < 0 || index >= entry_count())
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

NavigationEntry* NavigationController::CreateNavigationEntry(
    const GURL& url, const GURL& referrer, PageTransition::Type transition) {
  // Allow the browser URL handler to rewrite the URL. This will, for example,
  // remove "view-source:" from the beginning of the URL to get the URL that
  // will actually be loaded. This real URL won't be shown to the user, just
  // used internally.
  GURL loaded_url(url);
  BrowserURLHandler::RewriteURLIfNecessary(&loaded_url);

  NavigationEntry* entry = new NavigationEntry(NULL, -1, loaded_url, referrer,
                                               string16(), transition);
  entry->set_display_url(url);
  entry->set_user_typed_url(url);
  if (url.SchemeIsFile()) {
    std::wstring languages = profile()->GetPrefs()->GetString(
        prefs::kAcceptLanguages);
    entry->set_title(WideToUTF16Hack(
        file_util::GetFilenameFromPath(net::FormatUrl(url, languages))));
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
  tab_contents_->NotifyNavigationStateChanged(
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
  entry->set_title(WideToUTF16Hack(title));
  if (icon)
    entry->favicon().set_bitmap(*icon);

  DiscardNonCommittedEntriesInternal();
  pending_entry_ = entry;
  load_pending_entry_when_active_ = true;
}

bool NavigationController::LoadingURLLazily() const {
  return load_pending_entry_when_active_;
}

const string16& NavigationController::GetLazyTitle() const {
  if (pending_entry_)
    return pending_entry_->GetTitleForDisplay(this);
  else
    return EmptyString16();
}

const SkBitmap& NavigationController::GetLazyFavIcon() const {
  if (pending_entry_) {
    return pending_entry_->favicon().bitmap();
  } else {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    return *rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
  }
}

void NavigationController::DocumentLoadedInFrame() {
  last_document_loaded_ = base::TimeTicks::Now();
}

void NavigationController::OnUserGesture() {
  user_gesture_observed_ = true;
}

bool NavigationController::RendererDidNavigate(
    const ViewHostMsg_FrameNavigate_Params& params,
    LoadCommittedDetails* details) {
  // Save the previous state before we clobber it.
  if (GetLastCommittedEntry()) {
    details->previous_url = GetLastCommittedEntry()->url();
    details->previous_entry_index = last_committed_entry_index();
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
    pending_entry_->set_site_instance(tab_contents_->GetSiteInstance());

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
                      !pending_entry()) ||
      params.gesture == NavigationGestureAuto;

  // Now prep the rest of the details for the notification and broadcast.
  details->entry = GetActiveEntry();
  details->is_in_page = IsURLInPageNavigation(params.url);
  details->is_main_frame = PageTransition::IsMainFrame(params.transition);
  details->serialized_security_info = params.security_info;
  details->is_content_filtered = params.is_content_filtered;
  details->http_status_code = params.http_status_code;
  NotifyNavigationEntryCommitted(details);

  user_gesture_observed_ = false;

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

  if (params.page_id > tab_contents_->GetMaxPageID()) {
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
      tab_contents_->GetSiteInstance(),
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
      existing_entry != pending_entry_ &&
      pending_entry_->page_id() == -1) {
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

bool NavigationController::IsRedirect(
  const ViewHostMsg_FrameNavigate_Params& params) {
  // For main frame transition, we judge by params.transition.
  // Otherwise, by params.redirects.
  if (PageTransition::IsMainFrame(params.transition)) {
    return PageTransition::IsRedirect(params.transition);
  }
  return params.redirects.size() > 1;
}

bool NavigationController::IsLikelyAutoNavigation(base::TimeTicks now) {
  return !user_gesture_observed_ &&
         (now - last_document_loaded_) < kMaxAutoNavigationTimeDelta;
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
    new_entry = new NavigationEntry;
  }

  new_entry->set_url(params.url);
  new_entry->set_referrer(params.referrer);
  new_entry->set_page_id(params.page_id);
  new_entry->set_transition_type(params.transition);
  new_entry->set_site_instance(tab_contents_->GetSiteInstance());
  new_entry->set_has_post_data(params.is_post);

  // If the current entry is a redirection source and the redirection has
  // occurred within kMaxAutoNavigationTimeDelta since the last document load,
  // this is likely to be machine-initiated redirect and the entry needs to be
  // replaced with the new entry to avoid unwanted redirections in navigating
  // backward/forward.
  // Otherwise, just insert the new entry.
  InsertOrReplaceEntry(new_entry,
      IsRedirect(params) && IsLikelyAutoNavigation(base::TimeTicks::Now()));
}

void NavigationController::RendererDidNavigateToExistingPage(
    const ViewHostMsg_FrameNavigate_Params& params) {
  // We should only get here for main frame navigations.
  DCHECK(PageTransition::IsMainFrame(params.transition));

  // This is a back/forward navigation. The existing page for the ID is
  // guaranteed to exist by ClassifyNavigation, and we just need to update it
  // with new information from the renderer.
  int entry_index = GetEntryIndexWithPageID(tab_contents_->GetSiteInstance(),
                                            params.page_id);
  DCHECK(entry_index >= 0 &&
         entry_index < static_cast<int>(entries_.size()));
  NavigationEntry* entry = entries_[entry_index].get();

  // The URL may have changed due to redirects. The site instance will normally
  // be the same except during session restore, when no site instance will be
  // assigned.
  entry->set_url(params.url);
  DCHECK(entry->site_instance() == NULL ||
         entry->site_instance() == tab_contents_->GetSiteInstance());
  entry->set_site_instance(tab_contents_->GetSiteInstance());

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
      tab_contents_->GetSiteInstance(),
      params.page_id);

  // We assign the entry's unique ID to be that of the new one. Since this is
  // always the result of a user action, we want to dismiss infobars, etc. like
  // a regular user-initiated navigation.
  existing_entry->set_unique_id(pending_entry_->unique_id());

  // The URL may have changed due to redirects.
  existing_entry->set_url(params.url);

  DiscardNonCommittedEntries();
}

void NavigationController::RendererDidNavigateInPage(
    const ViewHostMsg_FrameNavigate_Params& params) {
  DCHECK(PageTransition::IsMainFrame(params.transition)) <<
      "WebKit should only tell us about in-page navs for the main frame.";
  // We're guaranteed to have an entry for this one.
  NavigationEntry* existing_entry = GetEntryWithPageID(
      tab_contents_->GetSiteInstance(),
      params.page_id);

  // Reference fragment navigation. We're guaranteed to have the last_committed
  // entry and it will be the same page as the new navigation (minus the
  // reference fragments, of course).
  NavigationEntry* new_entry = new NavigationEntry(*existing_entry);
  new_entry->set_page_id(params.page_id);
  new_entry->set_url(params.url);
  InsertOrReplaceEntry(new_entry,
      IsRedirect(params) && IsLikelyAutoNavigation(base::TimeTicks::Now()));
}

void NavigationController::RendererDidNavigateNewSubframe(
    const ViewHostMsg_FrameNavigate_Params& params) {
  if (PageTransition::StripQualifier(params.transition) ==
      PageTransition::AUTO_SUBFRAME) {
    // This is not user-initiated. Ignore.
    return;
  }
  if (IsRedirect(params)) {
    // This is redirect. Ignore.
    return;
  }

  // Manual subframe navigations just get the current entry cloned so the user
  // can go back or forward to it. The actual subframe information will be
  // stored in the page state for each of those entries. This happens out of
  // band with the actual navigations.
  DCHECK(GetLastCommittedEntry()) << "ClassifyNavigation should guarantee "
                                  << "that a last committed entry exists.";
  NavigationEntry* new_entry = new NavigationEntry(*GetLastCommittedEntry());
  new_entry->set_page_id(params.page_id);
  InsertOrReplaceEntry(new_entry, false);
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
      tab_contents_->GetSiteInstance(),
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

// TODO(brettw) I think this function is unnecessary.
void NavigationController::CommitPendingEntry() {
  DiscardTransientEntry();

  if (!pending_entry())
    return;  // Nothing to do.

  // Need to save the previous URL for the notification.
  LoadCommittedDetails details;
  if (GetLastCommittedEntry()) {
    details.previous_url = GetLastCommittedEntry()->url();
    details.previous_entry_index = last_committed_entry_index();
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
    // it new again, since InsertOrReplaceEntry expects to take ownership and
    // also discard the pending entry. We also need to synthesize a page ID. We
    // can only do this because this function will only be called by our custom
    // TabContents types. For TabContents, the IDs are generated by the
    // renderer, so we can't do this.
    details.type = NavigationType::NEW_PAGE;
    pending_entry_->set_page_id(tab_contents_->GetMaxPageID() + 1);
    tab_contents_->UpdateMaxPageID(pending_entry_->page_id());
    InsertOrReplaceEntry(new NavigationEntry(*pending_entry_), false);
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

void NavigationController::CopyStateFrom(const NavigationController& source) {
  // Verify that we look new.
  DCHECK(entry_count() == 0 && !pending_entry());

  if (source.entry_count() == 0)
    return;  // Nothing new to do.

  needs_reload_ = true;
  for (int i = 0; i < source.entry_count(); i++) {
    entries_.push_back(linked_ptr<NavigationEntry>(
        new NavigationEntry(*source.entries_[i])));
  }

  FinishRestore(source.last_committed_entry_index_);
}

void NavigationController::DiscardNonCommittedEntries() {
  bool transient = transient_entry_index_ != -1;
  DiscardNonCommittedEntriesInternal();

  // If there was a transient entry, invalidate everything so the new active
  // entry state is shown.
  if (transient) {
    tab_contents_->NotifyNavigationStateChanged(
        TabContents::INVALIDATE_EVERYTHING);
  }
}

void NavigationController::InsertOrReplaceEntry(NavigationEntry* entry,
                                                bool replace) {
  DCHECK(entry->transition_type() != PageTransition::AUTO_SUBFRAME);

  // Copy the pending entry's unique ID to the committed entry.
  // I don't know if pending_entry_index_ can be other than -1 here.
  const NavigationEntry* const pending_entry = (pending_entry_index_ == -1) ?
      pending_entry_ : entries_[pending_entry_index_].get();
  if (pending_entry)
    entry->set_unique_id(pending_entry->unique_id());

  DiscardNonCommittedEntriesInternal();

  int current_size = static_cast<int>(entries_.size());

  if (current_size > 0) {
    // Prune any entries which are in front of the current entry.
    // Also prune the current entry if we are to replace the current entry.
    int prune_up_to = replace ? last_committed_entry_index_ - 1
                              : last_committed_entry_index_;
    int num_pruned = 0;
    while (prune_up_to < (current_size - 1)) {
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
  tab_contents_->UpdateMaxPageID(entry->page_id());
}

void NavigationController::SetWindowID(const SessionID& id) {
  window_id_ = id;
  NotificationService::current()->Notify(NotificationType::TAB_PARENTED,
                                         Source<NavigationController>(this),
                                         NotificationService::NoDetails());
}

void NavigationController::NavigateToPendingEntry(bool reload) {
  // For session history navigations only the pending_entry_index_ is set.
  if (!pending_entry_) {
    DCHECK(pending_entry_index_ != -1);
    pending_entry_ = entries_[pending_entry_index_].get();
  }

  if (!tab_contents_->NavigateToPendingEntry(reload))
    DiscardNonCommittedEntries();
}

void NavigationController::NotifyNavigationEntryCommitted(
    LoadCommittedDetails* details) {
  details->entry = GetActiveEntry();
  NotificationDetails notification_details =
      Details<LoadCommittedDetails>(details);

  // We need to notify the ssl_manager_ before the tab_contents_ so the
  // location bar will have up-to-date information about the security style
  // when it wants to draw.  See http://crbug.com/11157
  ssl_manager_.DidCommitProvisionalLoad(notification_details);

  // TODO(pkasting): http://b/1113079 Probably these explicit notification paths
  // should be removed, and interested parties should just listen for the
  // notification below instead.
  tab_contents_->NotifyNavigationStateChanged(
      TabContents::INVALIDATE_EVERYTHING);

  NotificationService::current()->Notify(
      NotificationType::NAV_ENTRY_COMMITTED,
      Source<NavigationController>(this),
      notification_details);
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

void NavigationController::FinishRestore(int selected_index) {
  DCHECK(selected_index >= 0 && selected_index < entry_count());
  ConfigureEntriesForRestore(&entries_);

  set_max_restored_page_id(entry_count());

  last_committed_entry_index_ = selected_index;
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
  entries_.erase(entries_.begin() + transient_entry_index_);
  transient_entry_index_ = -1;
}

int NavigationController::GetEntryIndexWithPageID(
    SiteInstance* instance, int32 page_id) const {
  for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
    if ((entries_[i]->site_instance() == instance) &&
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
