// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERSTITIAL_PAGE_H_
#define CHROME_BROWSER_INTERSTITIAL_PAGE_H_

#include <string>

#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"

class NavigationEntry;
class TabContents;

// This class is a base class for interstitial pages, pages that show some
// informative message asking for user validation before reaching the target
// page. (Navigating to a page served over bad HTTPS or a page contining
// malware are typical cases where an interstitial is required.)
//
// If specified in its constructor, this class creates a navigation entry so
// that when the interstitial shows, the current entry is the target URL.
//
// InterstitialPage instances take care of deleting themselves when closed
// through a navigation, the WebContents closing them or the tab containing them
// being closed.

class InterstitialPage : public NotificationObserver {
 public:
  // Creates an interstitial page to show in |tab|. If |create_navigation_entry|
  // is true, a temporary navigation entry is created with the URL |url| and
  // added to the navigation controller (so the interstitial page appears as a
  // new navigation entry).
  InterstitialPage(TabContents* tab,
                   bool create_navigation_entry,
                   const GURL& url);
  virtual ~InterstitialPage();

  // Shows the interstitial page in the tab.
  void Show();

  // Invoked by the tab showing the interstitial to notify that the interstitial
  // page was closed.
  virtual void InterstitialClosed();

  // Retrieves the InterstitialPage if any associated with the specified
  // |tab_contents| (used by ui tests).
  static InterstitialPage* GetInterstitialPage(TabContents* tab_contents);

  // Sub-classes should return the HTML that should be displayed in the page.
  virtual std::string GetHTMLContents() { return std::string(); }

  // Reverts to the page showing before the interstitial.
  // Sub-classes should call this method when the user has chosen NOT to proceed
  // to the target URL.
  // Warning: 'this' has been deleted when this method returns.
  virtual void DontProceed();

 protected:
  // Invoked when the page sent a command through DOMAutomation.
  virtual void CommandReceived(const std::string& command) { }

  // Invoked with the NavigationEntry that is going to be added to the
  // navigation controller.
  // Gives an opportunity to sub-classes to set states on the |entry|.
  // Note that this is only called if the InterstitialPage was constructed with
  // |create_navigation_entry| set to true.
  virtual void UpdateEntry(NavigationEntry* entry) { }

  // Sub-classes should call this method when the user has chosen to proceed to
  // the target URL.
  // Warning: 'this' has been deleted when this method returns.
  virtual void Proceed();

  TabContents* tab() const { return tab_; }
  const GURL& url() const { return url_; }

 private:
  // AutomationProvider needs access to Proceed and DontProceed to simulate
  // user actions.
  friend class AutomationProvider;

  // NotificationObserver method.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Initializes tab_to_interstitial_page_ in a thread-safe manner.
  // Should be called before accessing tab_to_interstitial_page_.
  static void InitInterstitialPageMap();

  // A flag to indicate if we've notified |delegate_| of the user's decision.
  bool delegate_has_been_notified_;

  // The tab in which we are displayed.
  TabContents* tab_;

  // The URL that is shown when the interstitial is showing.
  GURL url_;

  // Whether a transient navigation entry should be created when the page is
  // shown.
  bool create_navigation_entry_;

  // Notification magic.
  NotificationRegistrar notification_registrar_;

  // We keep a map of the various blocking pages shown as the UI tests need to
  // be able to retrieve them.
  typedef std::map<TabContents*,InterstitialPage*> InterstitialPageMap;
  static InterstitialPageMap* tab_to_interstitial_page_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialPage);
};

#endif  // #ifndef CHROME_BROWSER_INTERSTITIAL_PAGE_H_
 
