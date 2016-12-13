// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/interstitial_page.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/thread.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/notification_service.h"
#include "grit/browser_resources.h"
#include "net/base/escape.h"
#include "views/window/window_delegate.h"

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
        resource_dispatcher_host_->BlockRequestsForRoute(
            process_id_, render_view_host_id_);
        break;
      case RESUME:
        resource_dispatcher_host_->ResumeBlockedRequestsForRoute(
            process_id_, render_view_host_id_);
        break;
      case CANCEL:
        resource_dispatcher_host_->CancelBlockedRequestsForRoute(
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

class InterstitialPage::InterstitialPageRVHViewDelegate
    : public RenderViewHostDelegate::View {
 public:
  explicit InterstitialPageRVHViewDelegate(InterstitialPage* page);

  // RenderViewHostDelegate::View implementation:
  virtual void CreateNewWindow(int route_id,
                               base::WaitableEvent* modal_dialog_event);
  virtual void CreateNewWidget(int route_id, bool activatable);
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture,
                                 const GURL& creator_url);
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos);
  virtual void ShowContextMenu(const ContextMenuParams& params);
  virtual void StartDragging(const WebDropData& drop_data);
  virtual void UpdateDragCursor(bool is_drop_target);
  virtual void GotFocus();
  virtual void TakeFocus(bool reverse);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);
  virtual void HandleMouseEvent();
  virtual void HandleMouseLeave();
  virtual void OnFindReply(int request_id,
                           int number_of_matches,
                           const gfx::Rect& selection_rect,
                           int active_match_ordinal,
                           bool final_update);
  virtual void UpdatePreferredWidth(int pref_width);

 private:
  InterstitialPage* interstitial_page_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialPageRVHViewDelegate);
};

// static
InterstitialPage::InterstitialPageMap*
    InterstitialPage::tab_to_interstitial_page_ =  NULL;

InterstitialPage::InterstitialPage(TabContents* tab,
                                   bool new_navigation,
                                   const GURL& url)
    : tab_(tab),
      url_(url),
      new_navigation_(new_navigation),
      should_discard_pending_nav_entry_(new_navigation),
      enabled_(true),
      action_taken_(false),
      render_view_host_(NULL),
      original_rvh_process_id_(tab->render_view_host()->process()->pid()),
      original_rvh_id_(tab->render_view_host()->routing_id()),
      should_revert_tab_title_(false),
      resource_dispatcher_host_notified_(false),
      ui_loop_(MessageLoop::current()),
      ALLOW_THIS_IN_INITIALIZER_LIST(rvh_view_delegate_(
          new InterstitialPageRVHViewDelegate(this))) {
  InitInterstitialPageMap();
  // It would be inconsistent to create an interstitial with no new navigation
  // (which is the case when the interstitial was triggered by a sub-resource on
  // a page) when we have a pending entry (in the process of loading a new top
  // frame).
  DCHECK(new_navigation || !tab->controller().pending_entry());
}

InterstitialPage::~InterstitialPage() {
  InterstitialPageMap::iterator iter = tab_to_interstitial_page_->find(tab_);
  DCHECK(iter != tab_to_interstitial_page_->end()) <<
      "InterstitialPage missing from map. Please add a comment to the bug "
      "http://crbug.com/9442 with the URL you were visiting";
  if (iter != tab_to_interstitial_page_->end())
    tab_to_interstitial_page_->erase(iter);
  DCHECK(!render_view_host_);
}

void InterstitialPage::Show() {
  // If an interstitial is already showing, close it before showing the new one.
  // Be careful not to take an action on the old interstitial more than once.
  if (tab_->interstitial_page()) {
    if (tab_->interstitial_page()->action_taken()) {
      tab_->interstitial_page()->Hide();
    } else {
      // If we are currently showing an interstitial page for which we created
      // a transient entry and a new interstitial is shown as the result of a
      // new browser initiated navigation, then that transient entry has already
      // been discarded and a new pending navigation entry created.
      // So we should not discard that new pending navigation entry.
      // See http://crbug.com/9791
      if (new_navigation_ && tab_->interstitial_page()->new_navigation_)
        tab_->interstitial_page()->should_discard_pending_nav_entry_= false;
      tab_->interstitial_page()->DontProceed();
    }
  }

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
    NavigationEntry* entry = new NavigationEntry;
    entry->set_url(url_);
    entry->set_display_url(url_);
    entry->set_page_type(NavigationEntry::INTERSTITIAL_PAGE);

    // Give sub-classes a chance to set some states on the navigation entry.
    UpdateEntry(entry);

    tab_->controller().AddTransientEntry(entry);
  }

  DCHECK(!render_view_host_);
  render_view_host_ = CreateRenderViewHost();
  CreateTabContentsView();

  std::string data_url = "data:text/html;charset=utf-8," +
                         EscapePath(GetHTMLContents());
  render_view_host_->NavigateToURL(GURL(data_url));

  notification_registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                              Source<TabContents>(tab_));
  notification_registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
      Source<NavigationController>(&tab_->controller()));
  notification_registrar_.Add(this, NotificationType::NAV_ENTRY_PENDING,
      Source<NavigationController>(&tab_->controller()));
}

void InterstitialPage::Hide() {
  RenderWidgetHostView* old_view = tab_->render_view_host()->view();
  if (old_view) {
    // Show the original RVH since we're going away.  Note it might not exist if
    // the renderer crashed while the interstitial was showing.
    old_view->Show();
  }

  // If the focus was on the interstitial, let's keep it to the page.
  // (Note that in unit-tests the RVH may not have a view).
  if (render_view_host_->view() && render_view_host_->view()->HasFocus() &&
      tab_->render_view_host()->view()) {
    tab_->render_view_host()->view()->Focus();
  }

  render_view_host_->Shutdown();
  render_view_host_ = NULL;
  if (tab_->interstitial_page())
    tab_->remove_interstitial_page();
  // Let's revert to the original title if necessary.
  NavigationEntry* entry = tab_->controller().GetActiveEntry();
  if (!new_navigation_ && should_revert_tab_title_) {
    entry->set_title(WideToUTF16Hack(original_tab_title_));
    tab_->NotifyNavigationStateChanged(TabContents::INVALIDATE_TAB);
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
        DCHECK(rvh->process()->pid() == original_rvh_process_id_ &&
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

RenderViewHostDelegate::View* InterstitialPage::GetViewDelegate() {
  return rvh_view_delegate_.get();
}

const GURL& InterstitialPage::GetURL() const {
  return url_;
}

void InterstitialPage::RenderViewGone(RenderViewHost* render_view_host) {
  // Our renderer died. This should not happen in normal cases.
  // Just dismiss the interstitial.
  DontProceed();
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

  RenderWidgetHostView* rwh_view = tab_->render_view_host()->view();

  // The RenderViewHost may already have crashed before we even get here.
  if (rwh_view) {
    // If the page has focus, focus the interstitial.
    if (rwh_view->HasFocus())
      Focus();

    // Hide the original RVH since we're showing the interstitial instead.
    rwh_view->Hide();
  }

  // Notify the tab we are not loading so the throbber is stopped. It also
  // causes a NOTIFY_LOAD_STOP notification, that the AutomationProvider (used
  // by the UI tests) expects to consider a navigation as complete. Without
  // this, navigating in a UI test to a URL that triggers an interstitial would
  // hang.
  tab_->SetIsLoading(false, NULL);
}

void InterstitialPage::UpdateTitle(RenderViewHost* render_view_host,
                                   int32 page_id,
                                   const std::wstring& title) {
  DCHECK(render_view_host == render_view_host_);
  NavigationEntry* entry = tab_->controller().GetActiveEntry();
  if (!entry) {
    // Crash reports from the field indicate this can be NULL.
    // This is unexpected as InterstitialPages constructed with the
    // new_navigation flag set to true create a transient navigation entry
    // (that is returned as the active entry). And the only case so far of
    // interstitial created with that flag set to false is with the
    // SafeBrowsingBlockingPage, when the resource triggering the interstitial
    // is a sub-resource, meaning the main page has already been loaded and a
    // navigation entry should have been created.
    NOTREACHED();
    return;
  }

  // If this interstitial is shown on an existing navigation entry, we'll need
  // to remember its title so we can revert to it when hidden.
  if (!new_navigation_ && !should_revert_tab_title_) {
    original_tab_title_ = UTF16ToWideHack(entry->title());
    should_revert_tab_title_ = true;
  }
  entry->set_title(WideToUTF16Hack(title));
  tab_->NotifyNavigationStateChanged(TabContents::INVALIDATE_TAB);
}

void InterstitialPage::DomOperationResponse(const std::string& json_string,
                                            int automation_id) {
  if (enabled_)
    CommandReceived(json_string);
}

RenderViewHost* InterstitialPage::CreateRenderViewHost() {
  RenderViewHost* render_view_host = new RenderViewHost(
      SiteInstance::CreateSiteInstance(tab()->profile()),
      this, MSG_ROUTING_NONE, NULL);
  return render_view_host;
}

TabContentsView* InterstitialPage::CreateTabContentsView() {
  TabContentsView* tab_contents_view = tab()->view();
  RenderWidgetHostView* view =
      tab_contents_view->CreateViewForWidget(render_view_host_);
  render_view_host_->set_view(view);
  render_view_host_->AllowBindings(BindingsPolicy::DOM_AUTOMATION);
  render_view_host_->CreateRenderView();
  view->SetSize(tab_contents_view->GetContainerSize());
  // Don't show the interstitial until we have navigated to it.
  view->Hide();
  return tab_contents_view;
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

  if (should_discard_pending_nav_entry_) {
    // Since no navigation happens we have to discard the transient entry
    // explicitely.  Note that by calling DiscardNonCommittedEntries() we also
    // discard the pending entry, which is what we want, since the navigation is
    // cancelled.
    tab_->controller().DiscardNonCommittedEntries();
  }

  Hide();
  // WARNING: we are now deleted!
}

void InterstitialPage::SetSize(const gfx::Size& size) {
#if defined(OS_WIN)
  // When a tab is closed, we might be resized after our view was NULLed
  // (typically if there was an info-bar).
  if (render_view_host_->view())
    render_view_host_->view()->SetSize(size);
#else
  // TODO(port): do Mac or Linux need to SetSize?
  NOTIMPLEMENTED();
#endif
}

void InterstitialPage::Focus() {
  // Focus the native window.
  render_view_host_->view()->Focus();
}

void InterstitialPage::FocusThroughTabTraversal(bool reverse) {
  render_view_host_->SetInitialFocus(reverse);
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
    TabContents* tab_contents) {
  InitInterstitialPageMap();
  InterstitialPageMap::const_iterator iter =
      tab_to_interstitial_page_->find(tab_contents);
  if (iter == tab_to_interstitial_page_->end())
    return NULL;

  return iter->second;
}

InterstitialPage::InterstitialPageRVHViewDelegate::
    InterstitialPageRVHViewDelegate(InterstitialPage* page)
    : interstitial_page_(page) {
}

void InterstitialPage::InterstitialPageRVHViewDelegate::CreateNewWindow(
    int route_id, base::WaitableEvent* modal_dialog_event) {
  NOTREACHED() << "InterstitialPage does not support showing popups yet.";
}

void InterstitialPage::InterstitialPageRVHViewDelegate::CreateNewWidget(
    int route_id, bool activatable) {
  NOTREACHED() << "InterstitialPage does not support showing drop-downs yet.";
}

void InterstitialPage::InterstitialPageRVHViewDelegate::ShowCreatedWindow(
    int route_id, WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos, bool user_gesture, const GURL& creator_url) {
  NOTREACHED() << "InterstitialPage does not support showing popups yet.";
}

void InterstitialPage::InterstitialPageRVHViewDelegate::ShowCreatedWidget(
    int route_id, const gfx::Rect& initial_pos) {
  NOTREACHED() << "InterstitialPage does not support showing drop-downs yet.";
}

void InterstitialPage::InterstitialPageRVHViewDelegate::ShowContextMenu(
    const ContextMenuParams& params) {
}

void InterstitialPage::InterstitialPageRVHViewDelegate::StartDragging(
    const WebDropData& drop_data) {
  NOTREACHED() << "InterstitialPage does not support dragging yet.";
}

void InterstitialPage::InterstitialPageRVHViewDelegate::UpdateDragCursor(
    bool is_drop_target) {
  NOTREACHED() << "InterstitialPage does not support dragging yet.";
}

void InterstitialPage::InterstitialPageRVHViewDelegate::GotFocus() {
}

void InterstitialPage::InterstitialPageRVHViewDelegate::UpdatePreferredWidth(
    int pref_width) {
}

void InterstitialPage::InterstitialPageRVHViewDelegate::TakeFocus(
    bool reverse) {
  if (interstitial_page_->tab() && interstitial_page_->tab()->GetViewDelegate())
    interstitial_page_->tab()->GetViewDelegate()->TakeFocus(reverse);
}

void InterstitialPage::InterstitialPageRVHViewDelegate::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if (interstitial_page_->tab() && interstitial_page_->tab()->GetViewDelegate())
    interstitial_page_->tab()->GetViewDelegate()->HandleKeyboardEvent(event);
}

void InterstitialPage::InterstitialPageRVHViewDelegate::HandleMouseEvent() {
  if (interstitial_page_->tab() && interstitial_page_->tab()->GetViewDelegate())
    interstitial_page_->tab()->GetViewDelegate()->HandleMouseEvent();
}

void InterstitialPage::InterstitialPageRVHViewDelegate::HandleMouseLeave() {
  if (interstitial_page_->tab() && interstitial_page_->tab()->GetViewDelegate())
    interstitial_page_->tab()->GetViewDelegate()->HandleMouseLeave();
}

void InterstitialPage::InterstitialPageRVHViewDelegate::OnFindReply(
    int request_id, int number_of_matches, const gfx::Rect& selection_rect,
    int active_match_ordinal, bool final_update) {
}
