// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_INTERSTITIAL_PAGE_H_
#define CHROME_BROWSER_TAB_CONTENTS_INTERSTITIAL_PAGE_H_

#include <string>

#include "base/gfx/size.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

class MessageLoop;
class NavigationEntry;
class WebContents;

// This class is a base class for interstitial pages, pages that show some
// informative message asking for user validation before reaching the target
// page. (Navigating to a page served over bad HTTPS or a page containing
// malware are typical cases where an interstitial is required.)
//
// If specified in its constructor, this class creates a navigation entry so
// that when the interstitial shows, the current entry is the target URL.
//
// InterstitialPage instances take care of deleting themselves when closed
// through a navigation, the WebContents closing them or the tab containing them
// being closed.

enum ResourceRequestAction {
  BLOCK,
  RESUME,
  CANCEL
};

class InterstitialPage : public NotificationObserver,
                         public RenderViewHostDelegate {
 public:
  // Creates an interstitial page to show in |tab|. |new_navigation| should be
  // set to true when the interstitial is caused by loading a new page, in which
  // case a temporary navigation entry is created with the URL |url| and
  // added to the navigation controller (so the interstitial page appears as a
  // new navigation entry). |new_navigation| should be false when the
  // interstitial was triggered by a loading a sub-resource in a page.
  InterstitialPage(WebContents* tab, bool new_navigation, const GURL& url);
  virtual ~InterstitialPage();

  // Shows the interstitial page in the tab.
  virtual void Show();

  // Hides the interstitial page. Warning: this deletes the InterstitialPage.
  void Hide();

  // Retrieves the InterstitialPage if any associated with the specified
  // |tab_contents| (used by ui tests).
  static InterstitialPage* GetInterstitialPage(WebContents* web_contents);

  // Sub-classes should return the HTML that should be displayed in the page.
  virtual std::string GetHTMLContents() { return std::string(); }

  // Reverts to the page showing before the interstitial.
  // Sub-classes should call this method when the user has chosen NOT to proceed
  // to the target URL.
  // Warning: 'this' has been deleted when this method returns.
  virtual void DontProceed();

  // Sub-classes should call this method when the user has chosen to proceed to
  // the target URL.
  // Warning: 'this' has been deleted when this method returns.
  virtual void Proceed();

  // Sizes the RenderViewHost showing the actual interstitial page contents.
  void SetSize(const gfx::Size& size);

 protected:
  // NotificationObserver method:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // RenderViewHostDelegate implementation:
  virtual Profile* GetProfile() const;
  virtual WebPreferences GetWebkitPrefs() {
    return WebPreferences();
  }
  virtual void DidNavigate(RenderViewHost* render_view_host,
                           const ViewHostMsg_FrameNavigate_Params& params);
  virtual void RenderViewGone(RenderViewHost* render_view_host);
  virtual void DomOperationResponse(const std::string& json_string,
                                    int automation_id);
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::wstring& title);
  virtual View* GetViewDelegate() const;

  // Invoked when the page sent a command through DOMAutomation.
  virtual void CommandReceived(const std::string& command) { }

  // Invoked with the NavigationEntry that is going to be added to the
  // navigation controller.
  // Gives an opportunity to sub-classes to set states on the |entry|.
  // Note that this is only called if the InterstitialPage was constructed with
  // |create_navigation_entry| set to true.
  virtual void UpdateEntry(NavigationEntry* entry) { }

  WebContents* tab() const { return tab_; }
  const GURL& url() const { return url_; }
  RenderViewHost* render_view_host() const { return render_view_host_; }

  // Creates and shows the RenderViewHost containing the interstitial content.
  // Overriden in unit tests.
  virtual RenderViewHost* CreateRenderViewHost();

 private:
  // AutomationProvider needs access to Proceed and DontProceed to simulate
  // user actions.
  friend class AutomationProvider;

  class InterstitialPageRVHViewDelegate;

  // Initializes tab_to_interstitial_page_ in a thread-safe manner.
  // Should be called before accessing tab_to_interstitial_page_.
  static void InitInterstitialPageMap();

  // Disable the interstitial:
  // - if it is not yet showing, then it won't be shown.
  // - any command sent by the RenderViewHost will be ignored.
  void Disable();

  // Executes the passed action on the ResourceDispatcher (on the IO thread).
  // Used to block/resume/cancel requests for the RenderViewHost hidden by this
  // interstitial.
  void TakeActionOnResourceDispatcher(ResourceRequestAction action);

  // The tab in which we are displayed.
  WebContents* tab_;

  // The URL that is shown when the interstitial is showing.
  GURL url_;

  // Whether this interstitial is shown as a result of a new navigation (in
  // which case a transient navigation entry is created).
  bool new_navigation_;

  // Whether this interstitial is enabled.  See Disable() for more info.
  bool enabled_;

  // Whether the Proceed or DontProceed have been called yet.
  bool action_taken_;

  // Notification magic.
  NotificationRegistrar notification_registrar_;

  // The RenderViewHost displaying the interstitial contents.
  RenderViewHost* render_view_host_;

  // The IDs for the RenderViewHost hidden by this interstitial.
  int original_rvh_process_id_;
  int original_rvh_id_;

  // Whether or not we should change the title of the tab when hidden (to revert
  // it to its original value).
  bool should_revert_tab_title_;

  // Whether the ResourceDispatcherHost has been notified to cancel/resume the
  // resource requests blocked for the RenderViewHost.
  bool resource_dispatcher_host_notified_;

  // The original title of the tab that should be reverted to when the
  // interstitial is hidden.
  std::wstring original_tab_title_;

  MessageLoop* ui_loop_;

  // Our RenderViewHostViewDelegate, necessary for accelerators to work.
  scoped_ptr<InterstitialPageRVHViewDelegate> rvh_view_delegate_;

  // We keep a map of the various blocking pages shown as the UI tests need to
  // be able to retrieve them.
  typedef std::map<WebContents*,InterstitialPage*> InterstitialPageMap;
  static InterstitialPageMap* tab_to_interstitial_page_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialPage);
};

#endif  // #ifndef CHROME_BROWSER_TAB_CONTENTS_INTERSTITIAL_PAGE_H_

