// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_MANAGER_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_MANAGER_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_observer.h"

class DOMUI;
class InterstitialPage;
class NavigationController;
class NavigationEntry;
class Profile;
class RenderViewHostDelegate;
class RenderViewHostFactory;
class RenderWidgetHostView;
class SiteInstance;

// Manages RenderViewHosts for a WebContents. Normally there is only one and
// it is easy to do. But we can also have transitions of processes (and hence
// RenderViewHosts) that can get complex.
class RenderViewHostManager : public NotificationObserver {
 public:
  // Functions implemented by our owner that we need.
  //
  // TODO(brettw) Clean this up! These are all the functions in WebContents that
  // are required to run this class. The design should probably be better such
  // that these are more clear.
  //
  // There is additional complexity that some of the functions we need in
  // WebContents are inherited and non-virtual. These are named with
  // "RenderManager" so that the duplicate implementation of them will be clear.
  class Delegate {
   public:
    // See web_contents.h's implementation for more.
    virtual bool CreateRenderViewForRenderManager(
        RenderViewHost* render_view_host) = 0;
    virtual void BeforeUnloadFiredFromRenderManager(
        bool proceed, bool* proceed_to_fire_unload) = 0;
    virtual void DidStartLoadingFromRenderManager(
        RenderViewHost* render_view_host, int32 page_id) = 0;
    virtual void RenderViewGoneFromRenderManager(
        RenderViewHost* render_view_host) = 0;
    virtual void UpdateRenderViewSizeForRenderManager() = 0;
    virtual void NotifySwappedFromRenderManager() = 0;
    virtual NavigationController* GetControllerForRenderManager() = 0;

    // Creates a DOMUI object for the given URL if one applies. Ownership of the
    // returned pointer will be passed to the caller. If no DOMUI applies,
    // returns NULL.
    virtual DOMUI* CreateDOMUIForRenderManager(const GURL& url) = 0;

    // Returns the navigation entry of the current navigation, or NULL if there
    // is none.
    virtual NavigationEntry*
        GetLastCommittedNavigationEntryForRenderManager() = 0;
  };

  // The factory is optional. It is used by unit tests to supply custom render
  // view hosts. When NULL, the regular RenderViewHost will be created.
  //
  // Both delegate pointers must be non-NULL and are not owned by this class.
  // They must outlive this class. The RenderViewHostDelegate is what will be
  // installed into all RenderViewHosts that are created.
  //
  // You must call Init() before using this class and Shutdown() before
  // deleting it.
  RenderViewHostManager(RenderViewHostFactory* render_view_factory,
                        RenderViewHostDelegate* render_view_delegate,
                        Delegate* delegate);
  ~RenderViewHostManager();

  // For arguments, see WebContents constructor.
  void Init(Profile* profile,
            SiteInstance* site_instance,
            int routing_id,
            base::WaitableEvent* modal_dialog_event);

  // Schedules all RenderViewHosts for destruction.
  void Shutdown();

  // Returns the currently actuive RenderViewHost.
  //
  // This will be non-NULL between Init() and Shutdown(). You may want to NULL
  // check it in many cases, however. Windows can send us messages during the
  // destruction process after it has been shut down.
  RenderViewHost* current_host() const {
    return render_view_host_;
  }

  // Returns the view associated with the current RenderViewHost, or NULL if
  // there is no current one.
  RenderWidgetHostView* current_view() const {
    if (!render_view_host_)
      return NULL;
    return render_view_host_->view();
  }

  // Returns the pending render view host, or NULL if there is no pending one.
  RenderViewHost* pending_render_view_host() const {
    return pending_render_view_host_;
  }

  // Returns the current committed DOM UI or NULL if none applies.
  DOMUI* dom_ui() const { return dom_ui_.get(); }

  // Returns the DOM UI for the pending navigation, or NULL of none applies.
  DOMUI* pending_dom_ui() const { return pending_dom_ui_.get(); }

  // Called when we want to instruct the renderer to navigate to the given
  // navigation entry. It may create a new RenderViewHost or re-use an existing
  // one. The RenderViewHost to navigate will be returned. Returns NULL if one
  // could not be created.
  RenderViewHost* Navigate(const NavigationEntry& entry);

  // Instructs the various live views to stop. Called when the user directed the
  // page to stop loading.
  void Stop();

  // Notifies the regular and pending RenderViewHosts that a load is or is not
  // happening. Even though the message is only for one of them, we don't know
  // which one so we tell both.
  void SetIsLoading(bool is_loading);

  // Whether to close the tab or not when there is a hang during an unload
  // handler. If we are mid-crosssite navigation, then we should proceed
  // with the navigation instead of closing the tab.
  bool ShouldCloseTabOnUnresponsiveRenderer();

  // Called when a renderer's main frame navigates.
  void DidNavigateMainFrame(RenderViewHost* render_view_host);

  // Allows the WebContents to react when a cross-site response is ready to be
  // delivered to a pending RenderViewHost.  We must first run the onunload
  // handler of the old RenderViewHost before we can allow it to proceed.
  void OnCrossSiteResponse(int new_render_process_host_id,
                           int new_request_id);

  // Notifies that the navigation that initiated a cross-site transition has
  // been canceled.
  void CrossSiteNavigationCanceled();

  // Called when a provisional load on the given renderer is aborted.
  void RendererAbortedProvisionalLoad(RenderViewHost* render_view_host);

  // Actually implements this RenderViewHostDelegate function for the
  // WebContents.
  void ShouldClosePage(bool proceed);

  // Forwards the message to the RenderViewHost, which is the original one.
  void OnJavaScriptMessageBoxClosed(IPC::Message* reply_msg,
                                    bool success,
                                    const std::wstring& prompt);

  // Sets the passed passed interstitial as the currently showing interstitial.
  // |interstitial_page| should be non NULL (use the remove_interstitial_page
  // method to unset the interstitial) and no interstitial page should be set
  // when there is already a non NULL interstitial page set.
  void set_interstitial_page(InterstitialPage* interstitial_page) {
    DCHECK(!interstitial_page_ && interstitial_page);
    interstitial_page_ = interstitial_page;
  }

  // Unsets the currently showing interstitial.
  void remove_interstitial_page() {
    DCHECK(interstitial_page_);
    interstitial_page_ = NULL;
  }

  // Returns the currently showing interstitial, NULL if no interstitial is
  // showing.
  InterstitialPage* interstitial_page() const {
    return interstitial_page_;
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  friend class TestWebContents;

  // Returns whether this tab should transition to a new renderer for
  // cross-site URLs.  Enabled unless we see the --process-per-tab command line
  // switch.  Can be overridden in unit tests.
  bool ShouldTransitionCrossSite();

  // Returns true if the two navigation entries are incompatible in some way
  // other than site instances. This will cause us to swap RenderViewHosts even
  // if the site instances are the same. Either of the entries may be NULL.
  bool ShouldSwapRenderViewsForNavigation(
      const NavigationEntry* cur_entry,
      const NavigationEntry* new_entry) const;

  // Returns an appropriate SiteInstance object for the given NavigationEntry,
  // possibly reusing the current SiteInstance.
  // Never called if --process-per-tab is used.
  SiteInstance* GetSiteInstanceForEntry(const NavigationEntry& entry,
                                        SiteInstance* curr_instance);

  // Helper method to create a pending RenderViewHost for a cross-site
  // navigation.
  bool CreatePendingRenderView(SiteInstance* instance);

  // Creates a RenderViewHost using render_view_factory_ (or directly, if the
  // factory is NULL).
  RenderViewHost* CreateRenderViewHost(SiteInstance* instance,
                                       int routing_id,
                                       base::WaitableEvent* modal_dialog_event);

  // Sets the pending RenderViewHost/DOMUI to be the active one. Note that this
  // doesn't require the pending render_view_host_ pointer to be non-NULL, since
  // there could be DOM UI switching as well. Call this for every commit.
  void CommitPending();

  // Helper method to terminate the pending RenderViewHost.
  void CancelPending();

  RenderViewHost* UpdateRendererStateForNavigate(const NavigationEntry& entry);

  // Our delegate, not owned by us. Guaranteed non-NULL.
  Delegate* delegate_;

  // Whether a navigation requiring different RenderView's is pending. This is
  // either cross-site request is (in the new process model), or when required
  // for the view type (like view source versus not).
  bool cross_navigation_pending_;

  // Allows tests to create their own render view host types.
  RenderViewHostFactory* render_view_factory_;

  // Implemented by the owner of this class, this delegate is installed into all
  // the RenderViewHosts that we create.
  RenderViewHostDelegate* render_view_delegate_;

  // Our RenderView host and its associated DOM UI (if any, will be NULL for
  // non-DOM-UI pages). This object is responsible for all communication with
  // a child RenderView instance.
  RenderViewHost* render_view_host_;
  scoped_ptr<DOMUI> dom_ui_;

  // A RenderViewHost used to load a cross-site page. This remains hidden
  // while a cross-site request is pending until it calls DidNavigate. It may
  // have an associated DOM UI, in which case the DOM UI pointer will be non-
  // NULL.
  //
  // The pending_dom_ui may be non-NULL even when the pending_render_view_host_
  // is. This will happen when we're transitioning between two DOM UI pages:
  // the RVH won't be swapped, so the pending pointer will be unused, but there
  // will be a pending DOM UI associated with the navigation.
  RenderViewHost* pending_render_view_host_;
  scoped_ptr<DOMUI> pending_dom_ui_;

  // The intersitial page currently shown if any, not own by this class
  // (the InterstitialPage is self-owned, it deletes itself when hidden).
  InterstitialPage* interstitial_page_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostManager);
};

// The "details" for a NOTIFY_RENDER_VIEW_HOST_CHANGED notification. The old
// host can be NULL when the first RenderViewHost is set.
struct RenderViewHostSwitchedDetails {
  RenderViewHost* old_host;
  RenderViewHost* new_host;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_MANAGER_H_
