// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/interstitial_page.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_resources.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/renderer_host/render_widget_host_view_win.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/tab_contents/web_contents_view_win.h"
#include "chrome/common/notification_service.h"
#include "chrome/views/window.h"
#include "chrome/views/window_delegate.h"
#include "net/base/escape.h"

namespace {

class ResourceRequestTask : public Task {
 public:
  ResourceRequestTask(int process_id,
                      int render_view_host_id,
                      ResourceRequestAction action)
      : action_(action),
        process_id_(process_id),
        render_view_host_id_(render_view_host_id),
        resource_dispatcher_host_(
            g_browser_process->resource_dispatcher_host()) {
  }

  virtual void Run() {
    switch (action_) {
      case BLOCK:
        resource_dispatcher_host_->BlockRequestsForRenderView(
            process_id_, render_view_host_id_);
        break;
      case RESUME:
        resource_dispatcher_host_->ResumeBlockedRequestsForRenderView(
            process_id_, render_view_host_id_);
        break;
      case CANCEL:
        resource_dispatcher_host_->CancelBlockedRequestsForRenderView(
            process_id_, render_view_host_id_);
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  ResourceRequestAction action_;
  int process_id_;
  int render_view_host_id_;
  ResourceDispatcherHost* resource_dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestTask);
};

}  // namespace

// static
InterstitialPage::InterstitialPageMap*
    InterstitialPage::tab_to_interstitial_page_ =  NULL;

InterstitialPage::InterstitialPage(WebContents* tab,
                                   bool new_navigation,
                                   const GURL& url)
    : tab_(tab),
      url_(url),
      action_taken_(false),
      enabled_(true),
      new_navigation_(new_navigation),
      original_rvh_process_id_(tab->render_view_host()->process()->host_id()),
      original_rvh_id_(tab->render_view_host()->routing_id()),
      render_view_host_(NULL),
      resource_dispatcher_host_notified_(false),
      should_revert_tab_title_(false),
      ui_loop_(MessageLoop::current()) {
  InitInterstitialPageMap();
  // It would be inconsistent to create an interstitial with no new navigation
  // (which is the case when the interstitial was triggered by a sub-resource on
  // a page) when we have a pending entry (in the process of loading a new top
  // frame).
  DCHECK(new_navigation || !tab->controller()->GetPendingEntry());
}

InterstitialPage::~InterstitialPage() {
  InterstitialPageMap::iterator iter = tab_to_interstitial_page_->find(tab_);
  DCHECK(iter != tab_to_interstitial_page_->end());
  tab_to_interstitial_page_->erase(iter);
  DCHECK(!render_view_host_);
}

void InterstitialPage::Show() {
  // If an interstitial is already showing, close it before showing the new one.
  if (tab_->interstitial_page())
    tab_->interstitial_page()->DontProceed();

  // Block the resource requests for the render view host while it is hidden.
  TakeActionOnResourceDispatcher(BLOCK);
  // We need to be notified when the RenderViewHost is destroyed so we can
  // cancel the blocked requests.  We cannot do that on
  // NOTIFY_TAB_CONTENTS_DESTROYED as at that point the RenderViewHost has
  // already been destroyed.
  notification_registrar_.Add(
      this, NotificationType::RENDER_WIDGET_HOST_DESTROYED,
      Source<RenderWidgetHost>(tab_->render_view_host()));

  // Update the tab_to_interstitial_page_ map.
  InterstitialPageMap::const_iterator iter =
      tab_to_interstitial_page_->find(tab_);
  DCHECK(iter == tab_to_interstitial_page_->end());
  (*tab_to_interstitial_page_)[tab_] = this;

  if (new_navigation_) {
    NavigationEntry* entry = new NavigationEntry(TAB_CONTENTS_WEB);
    entry->set_url(url_);
    entry->set_display_url(url_);
    entry->set_page_type(NavigationEntry::INTERSTITIAL_PAGE);

    // Give sub-classes a chance to set some states on the navigation entry.
    UpdateEntry(entry);

    tab_->controller()->AddTransientEntry(entry);
  }

  DCHECK(!render_view_host_);
  render_view_host_ = CreateRenderViewHost();

  std::string data_url = "data:text/html;charset=utf-8," +
                         EscapePath(GetHTMLContents());
  render_view_host_->NavigateToURL(GURL(data_url));

  notification_registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                              Source<TabContents>(tab_));
  notification_registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                              Source<NavigationController>(tab_->controller()));
  notification_registrar_.Add(this, NotificationType::NAV_ENTRY_PENDING,
                              Source<NavigationController>(tab_->controller()));
}

void InterstitialPage::Hide() {
  render_view_host_->Shutdown();
  render_view_host_ = NULL;
  if (tab_->interstitial_page())
    tab_->remove_interstitial_page();
  // Let's revert to the original title if necessary.
  NavigationEntry* entry = tab_->controller()->GetActiveEntry();
  if (!new_navigation_ && should_revert_tab_title_) {
    entry->set_title(original_tab_title_);
    tab_->NotifyNavigationStateChanged(TabContents::INVALIDATE_TITLE);
  }
  delete this;
}

void InterstitialPage::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::NAV_ENTRY_PENDING:
      // We are navigating away from the interstitial (the user has typed a URL
      // in the location bar or clicked a bookmark).  Make sure clicking on the
      // interstitial will have no effect.  Also cancel any blocked requests
      // on the ResourceDispatcherHost.  Note that when we get this notification
      // the RenderViewHost has not yet navigated so we'll unblock the
      // RenderViewHost before the resource request for the new page we are
      // navigating arrives in the ResourceDispatcherHost.  This ensures that
      // request won't be blocked if the same RenderViewHost was used for the
      // new navigation.
      Disable();
      DCHECK(!resource_dispatcher_host_notified_);
      TakeActionOnResourceDispatcher(CANCEL);
      break;
    case NotificationType::RENDER_WIDGET_HOST_DESTROYED:
      if (!action_taken_) {
        // The RenderViewHost is being destroyed (as part of the tab being
        // closed), make sure we clear the blocked requests.
        RenderViewHost* rvh = Source<RenderViewHost>(source).ptr();
        DCHECK(rvh->process()->host_id() == original_rvh_process_id_ &&
               rvh->routing_id() == original_rvh_id_);
        TakeActionOnResourceDispatcher(CANCEL);
      }
      break;
    case NotificationType::TAB_CONTENTS_DESTROYED:
    case NotificationType::NAV_ENTRY_COMMITTED:
      if (!action_taken_) {
        // We are navigating away from the interstitial or closing a tab with an
        // interstitial.  Default to DontProceed(). We don't just call Hide as
        // subclasses will almost certainly override DontProceed to do some work
        // (ex: close pending connections).
        DontProceed();
      } else {
        // User decided to proceed and either the navigation was committed or
        // the tab was closed before that.
        Hide();
        // WARNING: we are now deleted!
      }
      break;
    default:
      NOTREACHED();
  }
}

RenderViewHost* InterstitialPage::CreateRenderViewHost() {
  RenderViewHost* render_view_host = new RenderViewHost(
      SiteInstance::CreateSiteInstance(tab()->profile()),
      this, MSG_ROUTING_NONE, NULL);
  RenderWidgetHostViewWin* view =
      new RenderWidgetHostViewWin(render_view_host);
  render_view_host->set_view(view);
  view->Create(tab_->GetContentHWND());
  view->set_parent_hwnd(tab_->GetContentHWND());
  WebContentsViewWin* web_contents_view =
      static_cast<WebContentsViewWin*>(tab_->view());
  render_view_host->CreateRenderView();
  // SetSize must be called after CreateRenderView or the HWND won't show.
  view->SetSize(web_contents_view->GetContainerSize());

  render_view_host->AllowDomAutomationBindings();
  return render_view_host;
}

void InterstitialPage::Proceed() {
  if (action_taken_) {
    NOTREACHED();
    return;
  }
  Disable();
  action_taken_ = true;

  // Resumes the throbber.
  tab_->SetIsLoading(true, NULL);

  // If this is a new navigation, the old page is going away, so we cancel any
  // blocked requests for it.  If it is not a new navigation, then it means the
  // interstitial was shown as a result of a resource loading in the page.
  // Since the user wants to proceed, we'll let any blocked request go through.
  if (new_navigation_)
    TakeActionOnResourceDispatcher(CANCEL);
  else
    TakeActionOnResourceDispatcher(RESUME);

  // No need to hide if we are a new navigation, we'll get hidden when the
  // navigation is committed.
  if (!new_navigation_) {
    Hide();
    // WARNING: we are now deleted!
  }
}

void InterstitialPage::DontProceed() {
  if (action_taken_) {
    NOTREACHED();
    return;
  }
  Disable();
  action_taken_ = true;

  // If this is a new navigation, we are returning to the original page, so we
  // resume blocked requests for it.  If it is not a new navigation, then it
  // means the interstitial was shown as a result of a resource loading in the
  // page and we won't return to the original page, so we cancel blocked
  // requests in that case.
  if (new_navigation_)
    TakeActionOnResourceDispatcher(RESUME);
  else
    TakeActionOnResourceDispatcher(CANCEL);

  if (new_navigation_) {
    // Since no navigation happens we have to discard the transient entry
    // explicitely.  Note that by calling DiscardNonCommittedEntries() we also
    // discard the pending entry, which is what we want, since the navigation is
    // cancelled.
    tab_->controller()->DiscardNonCommittedEntries();
  }

  Hide();
  // WARNING: we are now deleted!
}

void InterstitialPage::SetSize(const gfx::Size& size) {
  render_view_host_->view()->SetSize(size);
}

Profile* InterstitialPage::GetProfile() const {
  return tab_->profile();
}

void InterstitialPage::DidNavigate(
    RenderViewHost* render_view_host,
    const ViewHostMsg_FrameNavigate_Params& params) {
  // A fast user could have navigated away from the page that triggered the
  // interstitial while the interstitial was loading, that would have disabled
  // us. In that case we can dismiss ourselves.
  if (!enabled_){
    DontProceed();
    return;
  }

  // The RenderViewHost has loaded its contents, we can show it now.
  render_view_host_->view()->Show();
  tab_->set_interstitial_page(this); 

  // Notify the tab we are not loading so the throbber is stopped. It also
  // causes a NOTIFY_LOAD_STOP notification, that the AutomationProvider (used
  // by the UI tests) expects to consider a navigation as complete. Without this,
  // navigating in a UI test to a URL that triggers an interstitial would hang.
  tab_->SetIsLoading(false, NULL);
}

void InterstitialPage::RendererGone(RenderViewHost* render_view_host) {
  // Our renderer died. This should not happen in normal cases.
  // Just dismiss the interstitial.
  DontProceed();
}

void InterstitialPage::DomOperationResponse(const std::string& json_string,
                                            int automation_id) {
  if (enabled_)
    CommandReceived(json_string);
}

void InterstitialPage::UpdateTitle(RenderViewHost* render_view_host,
                                   int32 page_id,
                                   const std::wstring& title) {
  DCHECK(render_view_host == render_view_host_);
  NavigationEntry* entry = tab_->controller()->GetActiveEntry();
  // If this interstitial is shown on an existing navigation entry, we'll need
  // to remember its title so we can revert to it when hidden.
  if (!new_navigation_ && !should_revert_tab_title_) {
    original_tab_title_ = entry->title();
    should_revert_tab_title_ = true;
  }
  entry->set_title(title);
  tab_->NotifyNavigationStateChanged(TabContents::INVALIDATE_TITLE);
}

void InterstitialPage::Disable() {
  enabled_ = false;
}

void InterstitialPage::TakeActionOnResourceDispatcher(
    ResourceRequestAction action) {
  DCHECK(MessageLoop::current() == ui_loop_) << 
      "TakeActionOnResourceDispatcher should be called on the main thread.";

  if (action == CANCEL || action == RESUME) {
    if (resource_dispatcher_host_notified_)
      return;
    resource_dispatcher_host_notified_ = true;
  }

  // The tab might not have a render_view_host if it was closed (in which case,
  // we have taken care of the blocked requests when processing
  // NOTIFY_RENDER_WIDGET_HOST_DESTROYED.
  // Also we need to test there is an IO thread, as when unit-tests we don't
  // have one.
  RenderViewHost* rvh = RenderViewHost::FromID(original_rvh_process_id_,
                                               original_rvh_id_);
  if (rvh && g_browser_process->io_thread()) {
    g_browser_process->io_thread()->message_loop()->PostTask(
        FROM_HERE, new ResourceRequestTask(original_rvh_process_id_,
                                           original_rvh_id_,
                                           action));
  }
}

// static
void InterstitialPage::InitInterstitialPageMap() {
  if (!tab_to_interstitial_page_)
    tab_to_interstitial_page_ = new InterstitialPageMap;
}

// static
InterstitialPage* InterstitialPage::GetInterstitialPage(
    WebContents* web_contents) {
  InitInterstitialPageMap();
  InterstitialPageMap::const_iterator iter =
      tab_to_interstitial_page_->find(web_contents);
  if (iter == tab_to_interstitial_page_->end())
    return NULL;

  return iter->second;
}

