// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/render_view_host_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/interstitial_page.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/render_widget_host_view.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/render_view_host_delegate.h"
#include "chrome/browser/site_instance.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"

// Destroys the given |**render_view_host| and NULLs out |*render_view_host|
// and then NULLs the field. Callers should only pass pointers to the
// pending_render_view_host_, interstitial_render_view_host_, or
// original_render_view_host_ fields of this object.
static void CancelRenderView(RenderViewHost** render_view_host) {
  (*render_view_host)->Shutdown();
  (*render_view_host) = NULL;
}

RenderViewHostManager::RenderViewHostManager(
    RenderViewHostFactory* render_view_factory,
    RenderViewHostDelegate* render_view_delegate,
    Delegate* delegate)
    : delegate_(delegate),
      renderer_state_(NORMAL),
      render_view_factory_(render_view_factory),
      render_view_delegate_(render_view_delegate),
      render_view_host_(NULL),
      original_render_view_host_(NULL),
      interstitial_render_view_host_(NULL),
      pending_render_view_host_(NULL),
      interstitial_page_(NULL),
      showing_repost_interstitial_(false) {
}

RenderViewHostManager::~RenderViewHostManager() {
  // Shutdown should have been called which should have cleaned these up.
  DCHECK(!render_view_host_);
  DCHECK(!pending_render_view_host_);
  DCHECK(!original_render_view_host_);
  DCHECK(!interstitial_render_view_host_);
}

void RenderViewHostManager::Init(Profile* profile,
                                 SiteInstance* site_instance,
                                 int routing_id,
                                 HANDLE modal_dialog_event) {
  // Create a RenderViewHost, once we have an instance.  It is important to
  // immediately give this SiteInstance to a RenderViewHost so that it is
  // ref counted.
  if (!site_instance)
    site_instance = SiteInstance::CreateSiteInstance(profile);
  render_view_host_ = CreateRenderViewHost(
      site_instance, routing_id, modal_dialog_event);
}

void RenderViewHostManager::Shutdown() {
  if (showing_interstitial_page()) {
    // The tab is closed while the interstitial page is showing, hide and
    // destroy it.
    HideInterstitialPage(false, false);
  }
  DCHECK(!interstitial_render_view_host_) << "Should have been deleted by Hide";

  if (pending_render_view_host_) {
    pending_render_view_host_->Shutdown();
    pending_render_view_host_ = NULL;
  }
  if (original_render_view_host_) {
    original_render_view_host_->Shutdown();
    original_render_view_host_ = NULL;
  }

  // We should always have a main RenderViewHost.
  render_view_host_->Shutdown();
  render_view_host_ = NULL;
}
  
RenderViewHost* RenderViewHostManager::Navigate(const NavigationEntry& entry) {
  RenderViewHost* dest_render_view_host = UpdateRendererStateNavigate(entry);
  if (!dest_render_view_host)
    return NULL;  // We weren't able to create a pending render view host.

  // If the current render_view_host_ isn't live, we should create it so
  // that we don't show a sad tab while the dest_render_view_host fetches
  // its first page.  (Bug 1145340)
  if (dest_render_view_host != render_view_host_ &&
      !render_view_host_->IsRenderViewLive()) {
    delegate_->CreateRenderViewForRenderManager(render_view_host_);
  }

  // If the renderer crashed, then try to create a new one to satisfy this
  // navigation request.
  if (!dest_render_view_host->IsRenderViewLive()) {
    if (!delegate_->CreateRenderViewForRenderManager(dest_render_view_host))
      return NULL;

    // Now that we've created a new renderer, be sure to hide it if it isn't
    // our primary one.  Otherwise, we might crash if we try to call Show()
    // on it later.
    if (dest_render_view_host != render_view_host_ &&
        dest_render_view_host->view()) {
      dest_render_view_host->view()->Hide();
    } else {
      // This is our primary renderer, notify here as we won't be calling
      // SwapToRenderView (which does the notify).
      RenderViewHostSwitchedDetails details;
      details.new_host = render_view_host_;
      details.old_host = NULL;
      NotificationService::current()->Notify(
          NOTIFY_RENDER_VIEW_HOST_CHANGED,
          Source<NavigationController>(
              delegate_->GetControllerForRenderManager()),
          Details<RenderViewHostSwitchedDetails>(&details));
    }
  }

  showing_repost_interstitial_ = false;
  return dest_render_view_host;
}

void RenderViewHostManager::Stop() {
  render_view_host_->Stop();

  // If we aren't in the NORMAL renderer state, we should stop the pending
  // renderers.  This will lead to a DidFailProvisionalLoad, which will
  // properly destroy them.
  if (renderer_state_ == PENDING) {
    pending_render_view_host_->Stop();

  } else if (renderer_state_ == ENTERING_INTERSTITIAL) {
    interstitial_render_view_host_->Stop();
    if (pending_render_view_host_) {
      pending_render_view_host_->Stop();
    }

  } else if (renderer_state_ == LEAVING_INTERSTITIAL) {
    if (pending_render_view_host_) {
      pending_render_view_host_->Stop();
    }
  }
}

void RenderViewHostManager::SetIsLoading(bool is_loading) {
  render_view_host_->SetIsLoading(is_loading);
  if (pending_render_view_host_)
    pending_render_view_host_->SetIsLoading(is_loading);
  if (original_render_view_host_)
    original_render_view_host_->SetIsLoading(is_loading);
}

bool RenderViewHostManager::ShouldCloseTabOnUnresponsiveRenderer() {
  if (renderer_state_ != PENDING)
    return true;

  // If the tab becomes unresponsive during unload while doing a
  // crosssite navigation, proceed with the navigation.
  int pending_request_id = pending_render_view_host_->GetPendingRequestId();
  if (pending_request_id == -1) {
    // Haven't gotten around to starting the request. 
    pending_render_view_host_->SetNavigationsSuspended(false);
  } else {
    current_host()->process()->CrossSiteClosePageACK(
        pending_render_view_host_->site_instance()->process_host_id(), 
        pending_request_id);
    DidNavigateMainFrame(pending_render_view_host_);
  }
  return false;
}

void RenderViewHostManager::DidNavigateMainFrame(
    RenderViewHost* render_view_host) {
  if (renderer_state_ == NORMAL) {
    // We should only hear this from our current renderer.
    DCHECK(render_view_host == render_view_host_);
    return;
  } else if (renderer_state_ == PENDING) {
    if (render_view_host == pending_render_view_host_) {
      // The pending cross-site navigation completed, so show the renderer.
      SwapToRenderView(&pending_render_view_host_, true);
      renderer_state_ = NORMAL;
    } else if (render_view_host == render_view_host_) {
      // A navigation in the original page has taken place.  Cancel the pending
      // one.
      CancelRenderView(&pending_render_view_host_);
      renderer_state_ = NORMAL;
    } else {
      // No one else should be sending us DidNavigate in this state.
      DCHECK(false);
      return;
    }

  } else if (renderer_state_ == ENTERING_INTERSTITIAL) {
    if (render_view_host == interstitial_render_view_host_) {
      // The interstitial renderer is ready, so show it, and keep the old
      // RenderViewHost around.
      original_render_view_host_ = render_view_host_;
      SwapToRenderView(&interstitial_render_view_host_, false);
      renderer_state_ = INTERSTITIAL;
    } else if (render_view_host == render_view_host_) {
      // We shouldn't get here, because the original render view was the one
      // that caused the ShowInterstitial.
      // However, until we intercept navigation events from JavaScript, it is
      // possible to get here, if another tab tells render_view_host_ to
      // navigate.  To be safe, we'll cancel the interstitial and show the
      // page that caused the DidNavigate.
      CancelRenderView(&interstitial_render_view_host_);
      if (pending_render_view_host_)
        CancelRenderView(&pending_render_view_host_);
      renderer_state_ = NORMAL;
    } else if (render_view_host == pending_render_view_host_) {
      // We shouldn't get here, because the original render view was the one
      // that caused the ShowInterstitial.
      // However, until we intercept navigation events from JavaScript, it is
      // possible to get here, if another tab tells pending_render_view_host_
      // to navigate.  To be safe, we'll cancel the interstitial and show the
      // page that caused the DidNavigate.
      CancelRenderView(&interstitial_render_view_host_);
      SwapToRenderView(&pending_render_view_host_, true);
      renderer_state_ = NORMAL;
    } else {
      // No one else should be sending us DidNavigate in this state.
      DCHECK(false);
      return;
    }

  } else if (renderer_state_ == INTERSTITIAL) {
    if (render_view_host == original_render_view_host_) {
      // We shouldn't get here, because the original render view was the one
      // that caused the ShowInterstitial.
      // However, until we intercept navigation events from JavaScript, it is
      // possible to get here, if another tab tells render_view_host_ to
      // navigate.  To be safe, we'll cancel the interstitial and show the
      // page that caused the DidNavigate.
      SwapToRenderView(&original_render_view_host_, true);
      if (pending_render_view_host_)
        CancelRenderView(&pending_render_view_host_);
      renderer_state_ = NORMAL;
    } else if (render_view_host == pending_render_view_host_) {
      // No one else should be sending us DidNavigate in this state.
      // However, until we intercept navigation events from JavaScript, it is
      // possible to get here, if another tab tells pending_render_view_host_
      // to navigate.  To be safe, we'll cancel the interstitial and show the
      // page that caused the DidNavigate.
      SwapToRenderView(&pending_render_view_host_, true);
      CancelRenderView(&original_render_view_host_);
      renderer_state_ = NORMAL;
    } else {
      // No one else should be sending us DidNavigate in this state.
      DCHECK(false);
      return;
    }
    InterstitialPageGone();

  } else if (renderer_state_ == LEAVING_INTERSTITIAL) {
    if (render_view_host == original_render_view_host_) {
      // We navigated to something in the original renderer, so show it.
      if (pending_render_view_host_)
        CancelRenderView(&pending_render_view_host_);
      SwapToRenderView(&original_render_view_host_, true);
      renderer_state_ = NORMAL;
    } else if (render_view_host == pending_render_view_host_) {
      // We navigated to something in the pending renderer.
      CancelRenderView(&original_render_view_host_);
      SwapToRenderView(&pending_render_view_host_, true);
      renderer_state_ = NORMAL;
    } else {
      // No one else should be sending us DidNavigate in this state.
      DCHECK(false);
      return;
    }
    InterstitialPageGone();

  } else {
    // No such state.
    DCHECK(false);
    return;
  }
}

void RenderViewHostManager::OnCrossSiteResponse(int new_render_process_host_id,
                                                int new_request_id) {
  // Should only see this while we have a pending renderer, possibly during an
  // interstitial.  Otherwise, we should ignore.
  if (renderer_state_ != PENDING && renderer_state_ != LEAVING_INTERSTITIAL)
    return;
  DCHECK(pending_render_view_host_);

  // Tell the old renderer to run its onunload handler.  When it finishes, it
  // will send a ClosePage_ACK to the ResourceDispatcherHost with the given
  // IDs (of the pending RVH's request), allowing the pending RVH's response to
  // resume.
  if (showing_interstitial_page()) {
    DCHECK(original_render_view_host_);
    original_render_view_host_->ClosePage(new_render_process_host_id,
                                          new_request_id);
  } else {
    render_view_host_->ClosePage(new_render_process_host_id,
                                 new_request_id);
  }

  // ResourceDispatcherHost has told us to run the onunload handler, which
  // means it is not a download or unsafe page, and we are going to perform the
  // navigation.  Thus, we no longer need to remember that the RenderViewHost
  // is part of a pending cross-site request.
  pending_render_view_host_->SetHasPendingCrossSiteRequest(false, 
                                                           new_request_id);
}

void RenderViewHostManager::RendererAbortedProvisionalLoad(
    RenderViewHost* render_view_host) {
  // We used to cancel the pending renderer here for cross-site downloads.
  // However, it's not safe to do that because the download logic repeatedly
  // looks for this TabContents based on a render view ID.  Instead, we just
  // leave the pending renderer around until the next navigation event
  // (Navigate, DidNavigate, etc), which will clean it up properly.
  // TODO(creis): All of this will go away when we move the cross-site logic
  // to ResourceDispatcherHost, so that we intercept responses rather than
  // navigation events.  (That's necessary to support onunload anyway.)  Once
  // we've made that change, we won't create a pending renderer until we know
  // the response is not a download.

  if (renderer_state_ == ENTERING_INTERSTITIAL) {
    if ((pending_render_view_host_ &&
         (pending_render_view_host_ == render_view_host)) ||
        (!pending_render_view_host_ &&
         (render_view_host_ == render_view_host))) {
      // The abort came from the RenderViewHost that triggered the
      // interstitial.  (e.g., User clicked stop after ShowInterstitial but
      // before the interstitial was visible.)  We should go back to NORMAL.
      // Note that this is an uncommon case, because we are only in the
      // ENTERING_INTERSTITIAL state in the small time window while the
      // interstitial's RenderViewHost is being created.
      if (pending_render_view_host_)
        CancelRenderView(&pending_render_view_host_);
      CancelRenderView(&interstitial_render_view_host_);
      renderer_state_ = NORMAL;
    }

    // We can get here, at least in the following case.
    // We show an interstitial, then navigate to a URL that leads to another
    // interstitial.  Now there's a race.  The new interstitial will be
    // created and we will go to ENTERING_INTERSTITIAL, but the old one will
    // meanwhile destroy itself and fire DidFailProvisionalLoad.  That puts
    // us here.  Should be safe to ignore the DidFailProvisionalLoad, from
    // the perspective of the renderer state.
  } else if (renderer_state_ == LEAVING_INTERSTITIAL) {
    // If we've left the interstitial by seeing a download (or otherwise
    // aborting a load), we should get back to the original page, because
    // interstitial page doesn't make sense anymore.  (For example, we may
    // have clicked Proceed on a download URL.)

    // TODO(creis):  This causes problems in the old process model when
    // visiting a new URL from an interstitial page.
    // This is because we receive a DidFailProvisionalLoad from cancelling
    // the first request, which is indistinguishable from a
    // DidFailProvisionalLoad from the second request (if it is a download).
    // We need to find a way to distinguish these cases, because it doesn't
    // make sense to keep showing the interstitial after a download.
    // if (pending_render_view_host_)
    //   CancelRenderView(&pending_render_view_host_);
    // SwapToRenderView(&original_render_view_host_, true);
    // renderer_state_ = NORMAL;
    // delegate_->InterstitialPageGoneFromRenderManager();
  }
}

void RenderViewHostManager::ShouldClosePage(bool proceed) {
  // Should only see this while we have a pending renderer.  Otherwise, we
  // should ignore.
  if (!pending_render_view_host_) {
    bool proceed_to_fire_unload;
    delegate_->BeforeUnloadFiredFromRenderManager(proceed,
                                                  &proceed_to_fire_unload);

    if (proceed_to_fire_unload) {
      // This is not a cross-site navigation, the tab is being closed.
      render_view_host_->FirePageUnload();
    }
    return;
  }

  DCHECK(renderer_state_ != ENTERING_INTERSTITIAL);
  DCHECK(renderer_state_ != INTERSTITIAL);
  if (proceed) {
    // Ok to unload the current page, so proceed with the cross-site navigate.
    pending_render_view_host_->SetNavigationsSuspended(false);
  } else {
    // Current page says to cancel.
    CancelRenderView(&pending_render_view_host_);
    renderer_state_ = NORMAL;
  }
}

void RenderViewHostManager::ShowInterstitialPage(
    InterstitialPage* interstitial_page) {
  // Note that it is important that the interstitial page render view host is
  // in the same process as the normal render view host for the tab, so they
  // use page ids from the same pool.  If they came from different processes,
  // page ids may collide causing confusion in the controller (existing
  // navigation entries in the controller history could get overridden with the
  // interstitial entry).
  SiteInstance* interstitial_instance = NULL;

  if (renderer_state_ == NORMAL) {
    // render_view_host_ will not be deleted before the end of this method, so
    // we don't have to worry about this SiteInstance's ref count dropping to
    // zero.
    interstitial_instance = render_view_host_->site_instance();

  } else if (renderer_state_ == PENDING) {
    // pending_render_view_host_ will not be deleted before the end of this
    // method (when we are in this state), so we don't have to worry about this
    // SiteInstance's ref count dropping to zero.
    interstitial_instance = pending_render_view_host_->site_instance();

  } else if (renderer_state_ == ENTERING_INTERSTITIAL) {
    // We should never get here if we're in the process of showing an
    // interstitial.
    // However, until we intercept navigation events from JavaScript, it is
    // possible to get here, if another tab tells render_view_host_ to
    // navigate to a URL that causes an interstitial.  To be safe, we'll cancel
    // the first interstitial.
    CancelRenderView(&interstitial_render_view_host_);
    renderer_state_ = NORMAL;

    // We'd like to now show the new interstitial, but if there's a
    // pending_render_view_host_, we can't tell if this JavaScript navigation
    // occurred in the original or the pending renderer.  That means we won't
    // know where to proceed, so we can't show the interstitial.  This is
    // really just meant to avoid a crash until we can intercept JavaScript
    // navigation events, so for now we'll kill the interstitial and go back
    // to the last known good page.
    if (pending_render_view_host_) {
      CancelRenderView(&pending_render_view_host_);
      return;
    }
    // Should be safe to show the interstitial for the new page.
    // render_view_host_ will not be deleted before the end of this method, so
    // we don't have to worry about this SiteInstance's ref count dropping to
    // zero.
    interstitial_instance = render_view_host_->site_instance();

  } else if (renderer_state_ == INTERSTITIAL) {
    // We should never get here if we're already showing an interstitial.
    // However, until we intercept navigation events from JavaScript, it is
    // possible to get here, if another tab tells render_view_host_ to
    // navigate to a URL that causes an interstitial.  To be safe, we'll go
    // back to normal first.
    if (pending_render_view_host_ != NULL) {
      // There was a pending RVH.  We don't know which RVH caused this call
      // to ShowInterstitial, so we can't really proceed.  We'll have to stay
      // in the NORMAL state, showing the last good page.  This is only a
      // temporary fix anyway, to stave off a crash.
      HideInterstitialPage(false, false);
      return;
    }
    // Should be safe to show the interstitial for the new page.
    // render_view_host_ will not be deleted before the end of this method, so
    // we don't have to worry about this SiteInstance's ref count dropping to
    // zero.
    SwapToRenderView(&original_render_view_host_, true);
    interstitial_instance = render_view_host_->site_instance();

  } else if (renderer_state_ == LEAVING_INTERSTITIAL) {
    SwapToRenderView(&original_render_view_host_, true);
    interstitial_instance = NULL;
    if (pending_render_view_host_) {
      // We're now effectively in PENDING.
      // pending_render_view_host_ will not be deleted before the end of this
      // method, so we don't have to worry about this SiteInstance's ref count
      // dropping to zero.
      interstitial_instance = pending_render_view_host_->site_instance();
    } else {
      // We're now effectively in NORMAL.
      // render_view_host_ will not be deleted before the end of this method,
      // so we don't have to worry about this SiteInstance's ref count dropping
      // to zero.
      interstitial_instance = render_view_host_->site_instance();
    }

  } else {
    // No such state.
    DCHECK(false);
    return;
  }

  // Create a pending renderer and move to ENTERING_INTERSTITIAL.
  interstitial_render_view_host_ =
      CreateRenderViewHost(interstitial_instance, MSG_ROUTING_NONE, NULL);
  interstitial_page_ = interstitial_page;
  bool success = delegate_->CreateRenderViewForRenderManager(
      interstitial_render_view_host_);
  if (!success) {
    // TODO(creis): If this fails, should we load the interstitial in
    // render_view_host_?  We shouldn't just skip the interstitial...
    CancelRenderView(&interstitial_render_view_host_);
    return;
  }

  // Don't show the view yet.
  interstitial_render_view_host_->view()->Hide();

  renderer_state_ = ENTERING_INTERSTITIAL;

  // We allow the DOM bindings as a way to get the page to talk back to us.
  interstitial_render_view_host_->AllowDomAutomationBindings();

  interstitial_render_view_host_->LoadAlternateHTMLString(
      interstitial_page->GetHTMLContents(), false,
      GURL::EmptyGURL(),
      std::string());
}

void RenderViewHostManager::HideInterstitialPage(bool wait_for_navigation,
                                                 bool proceed) {
  if (renderer_state_ == NORMAL || renderer_state_ == PENDING) {
    // Shouldn't get here, since there's no interstitial showing.
    DCHECK(false);
    return;

  } else if (renderer_state_ == ENTERING_INTERSTITIAL) {
    // Unclear if it is possible to get here.  (Can you hide the interstitial
    // before it is shown?)  If so, we should go back to NORMAL.
    CancelRenderView(&interstitial_render_view_host_);
    if (pending_render_view_host_)
      CancelRenderView(&pending_render_view_host_);
    renderer_state_ = NORMAL;
    return;
  }

  DCHECK(showing_interstitial_page());
  DCHECK(render_view_host_ && original_render_view_host_ &&
         !interstitial_render_view_host_);

  if (renderer_state_ == INTERSTITIAL) {
    // Disable the Proceed button on the interstitial, because the destination
    // renderer might get replaced.
    DisableInterstitialProceed(false);

  } else if (renderer_state_ == LEAVING_INTERSTITIAL) {
    // We have already given up the ability to proceed by starting a new
    // navigation.  If this is a request to proceed, we must ignore it.
    // (Hopefully we will have disabled the Proceed button by now, but it's
    // possible to get here before that happens.)
    if (proceed)
      return;
  }

  if (wait_for_navigation) {
    // We are resuming the loading.  We need to set the state to loading again
    // as it was set to false when the interstitial stopped loading (so the
    // throbber runs).
    delegate_->DidStartLoadingFromRenderManager(render_view_host_, NULL);
  }

  if (proceed) {
    // Now we will resume loading automatically, either in
    // original_render_view_host_ or in pending_render_view_host_.  When it
    // completes, we will display the renderer in DidNavigate.
    renderer_state_ = LEAVING_INTERSTITIAL;

  } else {
    // Don't proceed.  Go back to the previously showing page.
    if (renderer_state_ == LEAVING_INTERSTITIAL) {
      // We said DontProceed after starting to leave the interstitial.
      // Abandon whatever we were in the process of doing.
      original_render_view_host_->Stop();
    }
    SwapToRenderView(&original_render_view_host_, true);
    if (pending_render_view_host_)
      CancelRenderView(&pending_render_view_host_);
    renderer_state_ = NORMAL;
    InterstitialPageGone();
  }
}

bool RenderViewHostManager::IsRenderViewInterstitial(
    const RenderViewHost* render_view_host) const {
  if (showing_interstitial_page())
    return render_view_host_ == render_view_host;
  if (renderer_state_ == ENTERING_INTERSTITIAL)
    return interstitial_render_view_host_ == render_view_host;
  return false;
}

void RenderViewHostManager::OnJavaScriptMessageBoxClosed(
    IPC::Message* reply_msg,
    bool success,
    const std::wstring& prompt) {
  RenderViewHost* rvh = render_view_host_;
  if (showing_interstitial_page()) {
    // No JavaScript message boxes are ever shown by interstitial pages, but
    // they can be shown by the original RVH while an interstitial page is
    // showing (e.g., from an onunload event handler).  We should send this to
    // the original RVH and not the interstitial's RVH.
    // TODO(creis): Perhaps the JavascriptMessageBoxHandler should store which
    // RVH created it, so that it can tell this method which RVH to reply to.
    DCHECK(original_render_view_host_);
    rvh = original_render_view_host_;
  }
  rvh->JavaScriptMessageBoxClosed(reply_msg, success, prompt);
}


bool RenderViewHostManager::ShouldTransitionCrossSite() {
  // True if we are using process-per-site-instance (default) or
  // process-per-site (kProcessPerSite).
  return !CommandLine().HasSwitch(switches::kProcessPerTab);
}

SiteInstance* RenderViewHostManager::GetSiteInstanceForEntry(
    const NavigationEntry& entry,
    SiteInstance* curr_instance) {
  // NOTE: This is only called when ShouldTransitionCrossSite is true.

  // If the entry has an instance already, we should use it.
  if (entry.site_instance())
    return entry.site_instance();

  // (UGLY) HEURISTIC, process-per-site only:
  //
  // If this navigation is generated, then it probably corresponds to a search
  // query.  Given that search results typically lead to users navigating to
  // other sites, we don't really want to use the search engine hostname to
  // determine the site instance for this navigation.
  //
  // NOTE: This can be removed once we have a way to transition between
  //       RenderViews in response to a link click.
  //
  if (CommandLine().HasSwitch(switches::kProcessPerSite) &&
      entry.transition_type() == PageTransition::GENERATED)
    return curr_instance;

  const GURL& dest_url = entry.url();

  // If we haven't used our SiteInstance (and thus RVH) yet, then we can use it
  // for this entry.  We won't commit the SiteInstance to this site until the
  // navigation commits (in DidNavigate), unless the navigation entry was
  // restored. As session restore loads all the pages immediately we need to set
  // the site first, otherwise after a restore none of the pages would share
  // renderers.
  if (!curr_instance->has_site()) {
    // If we've already created a SiteInstance for our destination, we don't
    // want to use this unused SiteInstance; use the existing one.  (We don't
    // do this check if the curr_instance has a site, because for now, we want
    // to compare against the current URL and not the SiteInstance's site.  In
    // this case, there is no current URL, so comparing against the site is ok.
    // See additional comments below.)
    if (curr_instance->HasRelatedSiteInstance(dest_url)) {
      return curr_instance->GetRelatedSiteInstance(dest_url);
    } else {
      if (entry.restored())
        curr_instance->SetSite(dest_url);
      return curr_instance;
    }
  }

  // Otherwise, only create a new SiteInstance for cross-site navigation.

  // TODO(creis): Once we intercept links and script-based navigations, we
  // will be able to enforce that all entries in a SiteInstance actually have
  // the same site, and it will be safe to compare the URL against the
  // SiteInstance's site, as follows:
  // const GURL& current_url = curr_instance->site();
  // For now, though, we're in a hybrid model where you only switch
  // SiteInstances if you type in a cross-site URL.  This means we have to
  // compare the entry's URL to the last committed entry's URL.
  NavigationController* controller = delegate_->GetControllerForRenderManager();
  NavigationEntry* curr_entry = controller->GetLastCommittedEntry();
  if (showing_interstitial_page()) {
    // The interstitial is currently the last committed entry, but we want to
    // compare against the last non-interstitial entry.
    curr_entry = controller->GetEntryAtOffset(-1);
  }
  // If there is no last non-interstitial entry (and curr_instance already
  // has a site), then we must have been opened from another tab.  We want
  // to compare against the URL of the page that opened us, but we can't
  // get to it directly.  The best we can do is check against the site of
  // the SiteInstance.  This will be correct when we intercept links and
  // script-based navigations, but for now, it could place some pages in a
  // new process unnecessarily.  We should only hit this case if a page tries
  // to open a new tab to an interstitial-inducing URL, and then navigates
  // the page to a different same-site URL.  (This seems very unlikely in
  // practice.)
  const GURL& current_url = (curr_entry) ? curr_entry->url() :
      curr_instance->site();

  if (SiteInstance::IsSameWebSite(current_url, dest_url)) {
    return curr_instance;
  } else {
    // Start the new renderer in a new SiteInstance, but in the current
    // BrowsingInstance.  It is important to immediately give this new
    // SiteInstance to a RenderViewHost (if it is different than our current
    // SiteInstance), so that it is ref counted.  This will happen in
    // CreatePendingRenderView.
    return curr_instance->GetRelatedSiteInstance(dest_url);
  }
}

bool RenderViewHostManager::CreatePendingRenderView(SiteInstance* instance) {
  NavigationEntry* curr_entry =
      delegate_->GetControllerForRenderManager()->GetLastCommittedEntry();
  if (curr_entry && curr_entry->tab_type() == TAB_CONTENTS_WEB) {
    DCHECK(!curr_entry->content_state().empty());

    // TODO(creis): Should send a message to the RenderView to let it know
    // we're about to switch away, so that it sends an UpdateState message.
  }

  pending_render_view_host_ =
      CreateRenderViewHost(instance, MSG_ROUTING_NONE, NULL);

  bool success = delegate_->CreateRenderViewForRenderManager(
      pending_render_view_host_);
  if (success) {
    // Don't show the view until we get a DidNavigate from it.
    pending_render_view_host_->view()->Hide();
  } else {
    CancelRenderView(&pending_render_view_host_);
  }
  return success;
}

RenderViewHost* RenderViewHostManager::CreateRenderViewHost(
    SiteInstance* instance,
    int routing_id,
    HANDLE modal_dialog_event) {
  if (render_view_factory_) {
    return render_view_factory_->CreateRenderViewHost(
        instance, render_view_delegate_, routing_id, modal_dialog_event);
  } else {
    return new RenderViewHost(instance, render_view_delegate_, routing_id,
                              modal_dialog_event);
  }
}

void RenderViewHostManager::SwapToRenderView(
    RenderViewHost** new_render_view_host,
    bool destroy_after) {
  // Remember if the page was focused so we can focus the new renderer in
  // that case.
  bool focus_render_view = render_view_host_->view() &&
      render_view_host_->view()->HasFocus();

  // Hide the current view and prepare to destroy it.
  // TODO(creis): Get the old RenderViewHost to send us an UpdateState message
  // before we destroy it.
  if (render_view_host_->view())
    render_view_host_->view()->Hide();
  RenderViewHost* old_render_view_host = render_view_host_;

  // Swap in the pending view and make it active.
  render_view_host_ = (*new_render_view_host);
  (*new_render_view_host) = NULL;

  // If the view is gone, then this RenderViewHost died while it was hidden.
  // We ignored the RendererGone call at the time, so we should send it now
  // to make sure the sad tab shows up, etc.
  if (render_view_host_->view())
    render_view_host_->view()->Show();
  else
    delegate_->RendererGoneFromRenderManager(render_view_host_);

  // Make sure the size is up to date.  (Fix for bug 1079768.)
  delegate_->UpdateRenderViewSizeForRenderManager();

  if (focus_render_view && render_view_host_->view())
    render_view_host_->view()->Focus();

  RenderViewHostSwitchedDetails details;
  details.new_host = render_view_host_;
  details.old_host = old_render_view_host;
  NotificationService::current()->Notify(
      NOTIFY_RENDER_VIEW_HOST_CHANGED,
      Source<NavigationController>(delegate_->GetControllerForRenderManager()),
      Details<RenderViewHostSwitchedDetails>(&details));

  if (destroy_after)
    old_render_view_host->Shutdown();

  // Let the task manager know that we've swapped RenderViewHosts, since it
  // might need to update its process groupings.
  delegate_->NotifySwappedFromRenderManager();
}

RenderViewHost* RenderViewHostManager::UpdateRendererStateNavigate(
    const NavigationEntry& entry) {
  // If we are in PENDING or ENTERING_INTERSTITIAL, then we want to get back
  // to NORMAL and navigate as usual.
  if (renderer_state_ == PENDING || renderer_state_ == ENTERING_INTERSTITIAL) {
    if (pending_render_view_host_)
      CancelRenderView(&pending_render_view_host_);
    if (interstitial_render_view_host_)
      CancelRenderView(&interstitial_render_view_host_);
    renderer_state_ = NORMAL;
  }

  // render_view_host_ will not be deleted before the end of this method, so we
  // don't have to worry about this SiteInstance's ref count dropping to zero.
  SiteInstance* curr_instance = render_view_host_->site_instance();

  if (showing_interstitial_page()) {
    // Must disable any ability to proceed from the interstitial, because we're
    // about to navigate somewhere else.
    DisableInterstitialProceed(true);

    if (pending_render_view_host_)
      CancelRenderView(&pending_render_view_host_);

    renderer_state_ = LEAVING_INTERSTITIAL;

    // We want to compare against where we were, because we just cancelled
    // where we were going.  The original_render_view_host_ won't be deleted
    // before the end of this method, so we don't have to worry about this
    // SiteInstance's ref count dropping to zero.
    curr_instance = original_render_view_host_->site_instance();
  }

  // Determine if we need a new SiteInstance for this entry.
  // Again, new_instance won't be deleted before the end of this method, so it
  // is safe to use a normal pointer here.
  SiteInstance* new_instance = curr_instance;
  if (ShouldTransitionCrossSite())
    new_instance = GetSiteInstanceForEntry(entry, curr_instance);

  if (new_instance != curr_instance) {
    // New SiteInstance.
    DCHECK(renderer_state_ == NORMAL ||
           renderer_state_ == LEAVING_INTERSTITIAL);

    // Create a pending RVH and navigate it.
    bool success = CreatePendingRenderView(new_instance);
    if (!success)
      return NULL;

    // Check if our current RVH is live before we set up a transition.
    if (!render_view_host_->IsRenderViewLive()) {
      if (renderer_state_ == NORMAL) {
        // The current RVH is not live.  There's no reason to sit around with a
        // sad tab or a newly created RVH while we wait for the pending RVH to
        // navigate.  Just switch to the pending RVH now and go back to NORMAL,
        // without requiring a cross-site transition.  (Note that we don't care
        // about on{before}unload handlers if the current RVH isn't live.)
        SwapToRenderView(&pending_render_view_host_, true);
        return render_view_host_;

      } else if (renderer_state_ == LEAVING_INTERSTITIAL) {
        // Cancel the interstitial, since it has died and we're navigating away
        // anyway.
        DCHECK(original_render_view_host_);
        if (original_render_view_host_->IsRenderViewLive()) {
          // Swap back to the original and act like a pending request (using
          // the logic below).
          SwapToRenderView(&original_render_view_host_, true);
          renderer_state_ = NORMAL;
          InterstitialPageGone();
          // Continue with the pending cross-site transition logic below.
        } else {
          // Both the interstitial and original are dead.  Just like the NORMAL
          // case, let's skip the cross-site transition entirely.  We also have
          // to clean up the interstitial state.
          SwapToRenderView(&pending_render_view_host_, true);
          CancelRenderView(&original_render_view_host_);
          renderer_state_ = NORMAL;
          InterstitialPageGone();
          return render_view_host_;
        }
      } else {
        NOTREACHED();
        return render_view_host_;
      }
    }
    // Otherwise, it's safe to treat this as a pending cross-site transition.

    // Make sure the old render view stops, in case a load is in progress.
    render_view_host_->Stop();

    // Suspend the new render view (i.e., don't let it send the cross-site
    // Navigate message) until we hear back from the old renderer's
    // onbeforeunload handler.  If it returns false, we'll have to cancel the
    // request.
    pending_render_view_host_->SetNavigationsSuspended(true);

    // Tell the CrossSiteRequestManager that this RVH has a pending cross-site
    // request, so that ResourceDispatcherHost will know to tell us to run the
    // old page's onunload handler before it sends the response.
    pending_render_view_host_->SetHasPendingCrossSiteRequest(true, -1);

    // We now have a pending RVH.  If we were in NORMAL, we should now be in
    // PENDING.  If we were in LEAVING_INTERSTITIAL, we should stay there.
    if (renderer_state_ == NORMAL)
      renderer_state_ = PENDING;
    else
      DCHECK(renderer_state_ == LEAVING_INTERSTITIAL);

    // Tell the old render view to run its onbeforeunload handler, since it
    // doesn't otherwise know that the cross-site request is happening.  This
    // will trigger a call to ShouldClosePage with the reply.
    render_view_host_->FirePageBeforeUnload();

    return pending_render_view_host_;
  }

  // Same SiteInstance can be used.  Navigate render_view_host_ if we are in
  // the NORMAL state, and original_render_view_host_ if an interstitial is
  // showing.
  if (renderer_state_ == NORMAL)
    return render_view_host_;

  DCHECK(renderer_state_ == LEAVING_INTERSTITIAL);
  return original_render_view_host_;
}

void RenderViewHostManager::DisableInterstitialProceed(bool stop_request) {
  // TODO(creis): Make sure the interstitial page disables any ability to
  // proceed at this point, because we're about to abort the original request.
  // This can be done by adding a new event to the NotificationService.
  // We should also disable the button on the page itself, but it's ok if that
  // doesn't happen immediately.

  // Stopping the request is necessary if we are navigating away, because the
  // user could be requesting the same URL again, causing the HttpCache to
  // ignore it.  (Fixes bug 1079784.)
  if (stop_request) {
    original_render_view_host_->Stop();
    if (pending_render_view_host_)
      pending_render_view_host_->Stop();
  }
}

void RenderViewHostManager::InterstitialPageGone() {
  DCHECK(!showing_interstitial_page());
  if (interstitial_page_) {
    interstitial_page_->InterstitialClosed();
    interstitial_page_ = NULL;
  }
}

