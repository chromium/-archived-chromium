// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/render_view_host_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/render_widget_host_view.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"

RenderViewHostManager::RenderViewHostManager(
    RenderViewHostFactory* render_view_factory,
    RenderViewHostDelegate* render_view_delegate,
    Delegate* delegate)
    : delegate_(delegate),
      cross_navigation_pending_(false),
      render_view_factory_(render_view_factory),
      render_view_delegate_(render_view_delegate),
      render_view_host_(NULL),
      pending_render_view_host_(NULL),
      interstitial_page_(NULL) {
}

RenderViewHostManager::~RenderViewHostManager() {
  // Shutdown should have been called which should have cleaned these up.
  DCHECK(!render_view_host_);
  DCHECK(!pending_render_view_host_);
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
  if (pending_render_view_host_)
    CancelPendingRenderView();

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

  return dest_render_view_host;
}

void RenderViewHostManager::Stop() {
  render_view_host_->Stop();

  // If we are cross-navigating, we should stop the pending renderers.  This
  // will lead to a DidFailProvisionalLoad, which will properly destroy them.
  if (cross_navigation_pending_) {
    pending_render_view_host_->Stop();

  }
}

void RenderViewHostManager::SetIsLoading(bool is_loading) {
  render_view_host_->SetIsLoading(is_loading);
  if (pending_render_view_host_)
    pending_render_view_host_->SetIsLoading(is_loading);
}

bool RenderViewHostManager::ShouldCloseTabOnUnresponsiveRenderer() {
  if (!cross_navigation_pending_)
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
  if (!cross_navigation_pending_) {
    // We should only hear this from our current renderer.
    DCHECK(render_view_host == render_view_host_);
    return;
  }

  if (render_view_host == pending_render_view_host_) {
    // The pending cross-site navigation completed, so show the renderer.
    SwapToRenderView(&pending_render_view_host_, true);
    cross_navigation_pending_ = false;
  } else if (render_view_host == render_view_host_) {
    // A navigation in the original page has taken place.  Cancel the pending
    // one.
    CancelPendingRenderView();
    cross_navigation_pending_ = false;
  } else {
    // No one else should be sending us DidNavigate in this state.
    DCHECK(false);
  }
}

void RenderViewHostManager::OnCrossSiteResponse(int new_render_process_host_id,
                                                int new_request_id) {
  // Should only see this while we have a pending renderer.
  if (!cross_navigation_pending_)
    return;
  DCHECK(pending_render_view_host_);

  // Tell the old renderer to run its onunload handler.  When it finishes, it
  // will send a ClosePage_ACK to the ResourceDispatcherHost with the given
  // IDs (of the pending RVH's request), allowing the pending RVH's response to
  // resume.
  render_view_host_->ClosePage(new_render_process_host_id, new_request_id);

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

  if (proceed) {
    // Ok to unload the current page, so proceed with the cross-site navigate.
    pending_render_view_host_->SetNavigationsSuspended(false);
  } else {
    // Current page says to cancel.
    CancelPendingRenderView();
    cross_navigation_pending_ = false;
  }
}

void RenderViewHostManager::OnJavaScriptMessageBoxClosed(
    IPC::Message* reply_msg,
    bool success,
    const std::wstring& prompt) {
  render_view_host_->JavaScriptMessageBoxClosed(reply_msg, success, prompt);
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
  if (interstitial_page_) {
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
    CancelPendingRenderView();
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
  // If we are cross-navigating, then we want to get back to normal and navigate
  // as usual.
  if (cross_navigation_pending_) {
    if (pending_render_view_host_)
      CancelPendingRenderView();
    cross_navigation_pending_ = false;
  }

  // render_view_host_ will not be deleted before the end of this method, so we
  // don't have to worry about this SiteInstance's ref count dropping to zero.
  SiteInstance* curr_instance = render_view_host_->site_instance();

  // Determine if we need a new SiteInstance for this entry.
  // Again, new_instance won't be deleted before the end of this method, so it
  // is safe to use a normal pointer here.
  SiteInstance* new_instance = curr_instance;
  if (ShouldTransitionCrossSite())
    new_instance = GetSiteInstanceForEntry(entry, curr_instance);

  if (new_instance != curr_instance) {
    // New SiteInstance.
    DCHECK(!cross_navigation_pending_);

    // Create a pending RVH and navigate it.
    bool success = CreatePendingRenderView(new_instance);
    if (!success)
      return NULL;

    // Check if our current RVH is live before we set up a transition.
    if (!render_view_host_->IsRenderViewLive()) {
      if (!cross_navigation_pending_) {
        // The current RVH is not live.  There's no reason to sit around with a
        // sad tab or a newly created RVH while we wait for the pending RVH to
        // navigate.  Just switch to the pending RVH now and go back to non
        // cross-navigating (Note that we don't care about on{before}unload
        // handlers if the current RVH isn't live.)
        SwapToRenderView(&pending_render_view_host_, true);
        return render_view_host_;
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

    // We now have a pending RVH.
    DCHECK(!cross_navigation_pending_);
    cross_navigation_pending_ = true;

    // Tell the old render view to run its onbeforeunload handler, since it
    // doesn't otherwise know that the cross-site request is happening.  This
    // will trigger a call to ShouldClosePage with the reply.
    render_view_host_->FirePageBeforeUnload();

    return pending_render_view_host_;
  }

  // Same SiteInstance can be used.  Navigate render_view_host_ if we are not
  // cross navigating.
  DCHECK(!cross_navigation_pending_);
  return render_view_host_;
}

void RenderViewHostManager::CancelPendingRenderView() {
  pending_render_view_host_->Shutdown();
  pending_render_view_host_ = NULL;
}

void RenderViewHostManager::CrossSiteNavigationCanceled() {
  DCHECK(cross_navigation_pending_);
  cross_navigation_pending_ = false;
  if (pending_render_view_host_)
    CancelPendingRenderView();
}

