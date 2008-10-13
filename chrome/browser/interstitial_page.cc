// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitial_page.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_resources.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/web_contents.h"

// static
InterstitialPage::InterstitialPageMap*
    InterstitialPage::tab_to_interstitial_page_ =  NULL;

InterstitialPage::InterstitialPage(TabContents* tab,
                                   bool create_navigation_entry,
                                   const GURL& url)
    : tab_(tab),
      url_(url),
      delegate_has_been_notified_(false),
      create_navigation_entry_(create_navigation_entry) {
  InitInterstitialPageMap();

  // If there's already an interstitial in this tab, then we're about to
  // replace it.  We should be ok with just deleting the previous
  // InterstitialPage (not hiding it first), since we're about to be shown.
  InterstitialPageMap::const_iterator iter =
      tab_to_interstitial_page_->find(tab_);
  if (iter != tab_to_interstitial_page_->end()) {
    // Deleting the InterstitialPage will also remove it from the map.
    delete iter->second;
  }
  (*tab_to_interstitial_page_)[tab_] = this;

  // Register for DOM operations, this is how the  page notifies us of the user
  // selection.
  notification_registrar_.Add(this, NOTIFY_DOM_OPERATION_RESPONSE,
                              Source<TabContents>(tab_));
}

InterstitialPage::~InterstitialPage() {
  InterstitialPageMap::iterator iter = tab_to_interstitial_page_->find(tab_);
  DCHECK(iter != tab_to_interstitial_page_->end());
  tab_to_interstitial_page_->erase(iter);
}

void InterstitialPage::Show() {
  DCHECK(tab_->type() == TAB_CONTENTS_WEB);
  WebContents* tab = tab_->AsWebContents();

  if (create_navigation_entry_) {
    NavigationEntry* entry = new NavigationEntry(TAB_CONTENTS_WEB);
    entry->set_url(url_);
    entry->set_display_url(url_);
    entry->set_page_type(NavigationEntry::INTERSTITIAL_PAGE);

    // Give sub-classes a chance to set some states on the navigation entry.
    UpdateEntry(entry);

    tab_->controller()->AddTransientEntry(entry);
  }

  tab->ShowInterstitialPage(this);
}

void InterstitialPage::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  DCHECK(type == NOTIFY_DOM_OPERATION_RESPONSE);
  std::string json = Details<DomOperationNotificationDetails>(details)->json();
  CommandReceived(json);
}

void InterstitialPage::InterstitialClosed() {
  delete this;
}

void InterstitialPage::Proceed() {
  DCHECK(tab_->type() == TAB_CONTENTS_WEB);
  tab_->AsWebContents()->HideInterstitialPage(true, true);
}

void InterstitialPage::DontProceed() {
  if (create_navigation_entry_) {
    // Since no navigation happens we have to discard the transient entry
    // explicitely.  Note that by calling DiscardNonCommittedEntries() we also
    // discard the pending entry, which is what we want, since the navigation is
    // cancelled.
    tab_->controller()->DiscardNonCommittedEntries();
  }
  tab_->AsWebContents()->HideInterstitialPage(false, false);

  // WARNING: we are now deleted!
}

// static
void InterstitialPage::InitInterstitialPageMap() {
  if (!tab_to_interstitial_page_)
    tab_to_interstitial_page_ = new InterstitialPageMap;
}

// static
InterstitialPage* InterstitialPage::GetInterstitialPage(
    TabContents* tab_contents) {
  InitInterstitialPageMap();
  InterstitialPageMap::const_iterator iter =
      tab_to_interstitial_page_->find(tab_contents);
  if (iter == tab_to_interstitial_page_->end())
    return NULL;

  return iter->second;
}
