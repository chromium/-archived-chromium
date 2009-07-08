// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_view_host.h"

#include <string>
#include <vector>

#include "app/resource_bundle.h"
#include "base/gfx/native_widget_types.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/cross_site_request_manager.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/url_constants.h"
#include "net/base/net_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/api/public/WebFindOptions.h"
#include "webkit/glue/autofill_form.h"

#if defined(OS_WIN)
// TODO(port): accessibility not yet implemented. See http://crbug.com/8288.
#include "chrome/browser/browser_accessibility_manager.h"
#endif

using base::TimeDelta;
using webkit_glue::PasswordFormDomManager;
using WebKit::WebConsoleMessage;
using WebKit::WebFindOptions;
using WebKit::WebInputEvent;

namespace {

void FilterURL(ChildProcessSecurityPolicy* policy, int renderer_id, GURL* url) {
  if (!url->is_valid())
    return;  // We don't need to block invalid URLs.

  if (url->SchemeIs(chrome::kAboutScheme)) {
    // The renderer treats all URLs in the about: scheme as being about:blank.
    // Canonicalize about: URLs to about:blank.
    *url = GURL(chrome::kAboutBlankURL);
  }

  if (!policy->CanRequestURL(renderer_id, *url)) {
    // If this renderer is not permitted to request this URL, we invalidate the
    // URL.  This prevents us from storing the blocked URL and becoming confused
    // later.
    LOG(INFO) << "Blocked URL " << url->spec();
    *url = GURL();
  }
}

// Delay to wait on closing the tab for a beforeunload/unload handler to fire.
const int kUnloadTimeoutMS = 1000;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// RenderViewHost, public:

// static
RenderViewHost* RenderViewHost::FromID(int render_process_id,
                                       int render_view_id) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id);
  if (!process)
    return NULL;
  RenderWidgetHost* widget = static_cast<RenderWidgetHost*>(
      process->GetListenerByID(render_view_id));
  if (!widget || !widget->IsRenderView())
    return NULL;
  return static_cast<RenderViewHost*>(widget);
}

RenderViewHost::RenderViewHost(SiteInstance* instance,
                               RenderViewHostDelegate* delegate,
                               int routing_id,
                               base::WaitableEvent* modal_dialog_event)
    : RenderWidgetHost(instance->GetProcess(), routing_id),
      instance_(instance),
      delegate_(delegate),
      waiting_for_drag_context_response_(false),
      enabled_bindings_(0),
      pending_request_id_(0),
      modal_dialog_count_(0),
      navigations_suspended_(false),
      suspended_nav_message_(NULL),
      run_modal_reply_msg_(NULL),
      is_waiting_for_unload_ack_(false),
      are_javascript_messages_suppressed_(false),
      sudden_termination_allowed_(false) {
  DCHECK(instance_);
  DCHECK(delegate_);
  if (modal_dialog_event == NULL)
    modal_dialog_event = new base::WaitableEvent(true, false);

  modal_dialog_event_.reset(modal_dialog_event);

  // TODO(mpcomplete): remove this notification (and registrar) when we figure
  // out why we're crashing on process()->Init().
  // http://code.google.com/p/chromium/issues/detail?id=15607
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllSources());
}

RenderViewHost::~RenderViewHost() {
  DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
  if (devtools_manager)  // NULL in tests
    devtools_manager->UnregisterDevToolsClientHostFor(this);

  // Be sure to clean up any leftover state from cross-site requests.
  Singleton<CrossSiteRequestManager>()->SetHasPendingCrossSiteRequest(
      process()->pid(), routing_id(), false);

  NotificationService::current()->Notify(
      NotificationType::RENDER_VIEW_HOST_DELETED,
      Source<RenderViewHost>(this),
      NotificationService::NoDetails());
}

void RenderViewHost::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  DCHECK(type == NotificationType::RENDERER_PROCESS_TERMINATED);
  RenderProcessHost* rph = Source<RenderProcessHost>(source).ptr();
  if (rph == process()) {
    // Try to get some debugging information on the stack.
    size_t num_hosts = RenderProcessHost::size();
    bool no_listeners = rph->listeners_begin() == rph->listeners_end();
    bool live_instance = site_instance() != NULL;
    CHECK(live_instance);
    bool live_process = site_instance()->GetProcess() != NULL;
    bool same_process = site_instance()->GetProcess() == rph;
    CHECK(no_listeners);
    CHECK(live_process);
    CHECK(same_process);
    CHECK(num_hosts > 0);
    CHECK(false) << "RenderViewHost should outlive its RenderProcessHost.";
  }
}

bool RenderViewHost::CreateRenderView() {
  DCHECK(!IsRenderViewLive()) << "Creating view twice";
  CHECK(process());
  CHECK(process()->listeners_begin() != process()->listeners_end()) <<
      "Our process should have us as a listener.";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!process()->Init())
    return false;
  DCHECK(process()->HasConnection());
  DCHECK(process()->profile());

  if (BindingsPolicy::is_dom_ui_enabled(enabled_bindings_)) {
    ChildProcessSecurityPolicy::GetInstance()->GrantDOMUIBindings(
        process()->pid());
  }

  if (BindingsPolicy::is_extension_enabled(enabled_bindings_)) {
    ChildProcessSecurityPolicy::GetInstance()->GrantExtensionBindings(
        process()->pid());
  }

  renderer_initialized_ = true;

#if defined(OS_WIN)
  HANDLE modal_dialog_event_handle;
  HANDLE renderer_process_handle = process()->process().handle();
  if (renderer_process_handle == NULL)
    renderer_process_handle = GetCurrentProcess();

  BOOL result = DuplicateHandle(GetCurrentProcess(),
      modal_dialog_event_->handle(),
      renderer_process_handle,
      &modal_dialog_event_handle,
      SYNCHRONIZE,
      FALSE,
      0);
  DCHECK(result) <<
      "Couldn't duplicate the modal dialog handle for the renderer.";
#endif

  ModalDialogEvent modal_dialog_event;
#if defined(OS_WIN)
  modal_dialog_event.event = modal_dialog_event_handle;
#endif

  Send(new ViewMsg_New(GetNativeViewId(),
                       modal_dialog_event,
                       delegate_->GetRendererPrefs(),
                       delegate_->GetWebkitPrefs(),
                       routing_id()));

  // Set the alternate error page, which is profile specific, in the renderer.
  GURL url = delegate_->GetAlternateErrorPageURL();
  SetAlternateErrorPageURL(url);

  // If it's enabled, tell the renderer to set up the Javascript bindings for
  // sending messages back to the browser.
  Send(new ViewMsg_AllowBindings(routing_id(), enabled_bindings_));

  // Let our delegate know that we created a RenderView.
  delegate_->RenderViewCreated(this);

  return true;
}

bool RenderViewHost::IsRenderViewLive() const {
  return process()->HasConnection() && renderer_initialized_;
}

void RenderViewHost::SetRendererPrefs(
    const RendererPreferences& renderer_prefs) {
  Send(new ViewMsg_SetRendererPrefs(routing_id(), renderer_prefs));
}

void RenderViewHost::Navigate(const ViewMsg_Navigate_Params& params) {
  ChildProcessSecurityPolicy::GetInstance()->GrantRequestURL(
      process()->pid(), params.url);

  ViewMsg_Navigate* nav_message = new ViewMsg_Navigate(routing_id(), params);

  // Only send the message if we aren't suspended at the start of a cross-site
  // request.
  if (navigations_suspended_) {
    // Shouldn't be possible to have a second navigation while suspended, since
    // navigations will only be suspended during a cross-site request.  If a
    // second navigation occurs, TabContents will cancel this pending RVH
    // create a new pending RVH.
    DCHECK(!suspended_nav_message_.get());
    suspended_nav_message_.reset(nav_message);
  } else {
    Send(nav_message);

    // Force the throbber to start. We do this because WebKit's "started
    // loading" message will be received asynchronously from the UI of the
    // browser. But we want to keep the throbber in sync with what's happening
    // in the UI. For example, we want to start throbbing immediately when the
    // user naivgates even if the renderer is delayed. There is also an issue
    // with the throbber starting because the DOMUI (which controls whether the
    // favicon is displayed) happens synchronously. If the start loading
    // messages was asynchronous, then the default favicon would flash in.
    //
    // WebKit doesn't send throb notifications for JavaScript URLs, so we
    // don't want to either.
    if (!params.url.SchemeIs(chrome::kJavaScriptScheme))
      delegate_->DidStartLoading(this);
  }
}

void RenderViewHost::NavigateToURL(const GURL& url) {
  ViewMsg_Navigate_Params params;
  params.page_id = -1;
  params.url = url;
  params.transition = PageTransition::LINK;
  params.reload = false;
  Navigate(params);
}

void RenderViewHost::LoadAlternateHTMLString(const std::string& html_text,
                                             bool new_navigation,
                                             const GURL& display_url,
                                             const std::string& security_info) {
  Send(new ViewMsg_LoadAlternateHTMLText(routing_id(), html_text,
                                         new_navigation, display_url,
                                         security_info));
}

void RenderViewHost::SetNavigationsSuspended(bool suspend) {
  // This should only be called to toggle the state.
  DCHECK(navigations_suspended_ != suspend);

  navigations_suspended_ = suspend;
  if (!suspend && suspended_nav_message_.get()) {
    // There's a navigation message waiting to be sent.  Now that we're not
    // suspended anymore, resume navigation by sending it.
    Send(suspended_nav_message_.release());
  }
}

void RenderViewHost::FirePageBeforeUnload() {
  if (!IsRenderViewLive()) {
    // This RenderViewHost doesn't have a live renderer, so just skip running
    // the onbeforeunload handler.
    OnMsgShouldCloseACK(true);
    return;
  }

  // This may be called more than once (if the user clicks the tab close button
  // several times, or if she clicks the tab close button then the browser close
  // button), so this test makes sure we only send the message once.
  if (!is_waiting_for_unload_ack_) {
    // Start the hang monitor in case the renderer hangs in the beforeunload
    // handler.
    is_waiting_for_unload_ack_ = true;
    StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));
    Send(new ViewMsg_ShouldClose(routing_id()));
  }
}

void RenderViewHost::FirePageUnload() {
  ClosePage(process()->pid(), routing_id());
}

// static
void RenderViewHost::ClosePageIgnoringUnloadEvents(int render_process_host_id,
                                                   int request_id) {
  RenderViewHost* rvh = RenderViewHost::FromID(render_process_host_id,
                                               request_id);
  if (!rvh)
    return;

  rvh->StopHangMonitorTimeout();
  rvh->is_waiting_for_unload_ack_ = false;

  rvh->set_sudden_termination_allowed(true);
  rvh->delegate()->Close(rvh);
}

void RenderViewHost::ClosePage(int new_render_process_host_id,
                               int new_request_id) {
  // Start the hang monitor in case the renderer hangs in the unload handler.
  is_waiting_for_unload_ack_ = true;
  StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));

  if (IsRenderViewLive()) {
    Send(new ViewMsg_ClosePage(routing_id(),
                               new_render_process_host_id,
                               new_request_id));
  } else {
    // This RenderViewHost doesn't have a live renderer, so just skip closing
    // the page.  We must notify the ResourceDispatcherHost on the IO thread,
    // which we will do through the RenderProcessHost's widget helper.
    process()->CrossSiteClosePageACK(new_render_process_host_id,
                                     new_request_id);
  }
}

void RenderViewHost::SetHasPendingCrossSiteRequest(bool has_pending_request,
                                                   int request_id) {
  Singleton<CrossSiteRequestManager>()->SetHasPendingCrossSiteRequest(
      process()->pid(), routing_id(), has_pending_request);
  pending_request_id_ = request_id;
}

int RenderViewHost::GetPendingRequestId() {
  return pending_request_id_;
}

void RenderViewHost::OnCrossSiteResponse(int new_render_process_host_id,
                                         int new_request_id) {
  delegate_->OnCrossSiteResponse(new_render_process_host_id, new_request_id);
}

void RenderViewHost::Stop() {
  Send(new ViewMsg_Stop(routing_id()));
}

bool RenderViewHost::PrintPages() {
  return Send(new ViewMsg_PrintPages(routing_id()));
}

void RenderViewHost::PrintingDone(int document_cookie, bool success) {
  Send(new ViewMsg_PrintingDone(routing_id(), document_cookie, success));
}

void RenderViewHost::StartFinding(int request_id,
                                  const string16& search_text,
                                  bool forward,
                                  bool match_case,
                                  bool find_next) {
  if (search_text.empty())
    return;

  WebFindOptions options;
  options.forward = forward;
  options.matchCase = match_case;
  options.findNext = find_next;
  Send(new ViewMsg_Find(routing_id(), request_id, search_text, options));

  // This call is asynchronous and returns immediately.
  // The result of the search is sent as a notification message by the renderer.
}

void RenderViewHost::StopFinding(bool clear_selection) {
  Send(new ViewMsg_StopFinding(routing_id(), clear_selection));
}

void RenderViewHost::Zoom(PageZoom::Function function) {
  Send(new ViewMsg_Zoom(routing_id(), function));
}

void RenderViewHost::SetPageEncoding(const std::wstring& encoding_name) {
  Send(new ViewMsg_SetPageEncoding(routing_id(), encoding_name));
}

void RenderViewHost::SetAlternateErrorPageURL(const GURL& url) {
  Send(new ViewMsg_SetAltErrorPageURL(routing_id(), url));
}

void RenderViewHost::FillForm(const FormData& form_data) {
  Send(new ViewMsg_FormFill(routing_id(), form_data));
}

void RenderViewHost::FillPasswordForm(
    const PasswordFormDomManager::FillData& form_data) {
  Send(new ViewMsg_FillPasswordForm(routing_id(), form_data));
}

void RenderViewHost::DragTargetDragEnter(
    const WebDropData& drop_data,
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt) {
  // Grant the renderer the ability to load the drop_data.
  ChildProcessSecurityPolicy* policy = ChildProcessSecurityPolicy::GetInstance();
  policy->GrantRequestURL(process()->pid(), drop_data.url);
  for (std::vector<string16>::const_iterator iter(drop_data.filenames.begin());
       iter != drop_data.filenames.end(); ++iter) {
    FilePath path = FilePath::FromWStringHack(UTF16ToWideHack(*iter));
    policy->GrantRequestURL(process()->pid(), net::FilePathToFileURL(path));
    policy->GrantUploadFile(process()->pid(), path);
  }
  Send(new ViewMsg_DragTargetDragEnter(routing_id(), drop_data, client_pt,
                                       screen_pt));
}

void RenderViewHost::DragTargetDragOver(
    const gfx::Point& client_pt, const gfx::Point& screen_pt) {
  Send(new ViewMsg_DragTargetDragOver(routing_id(), client_pt, screen_pt));
}

void RenderViewHost::DragTargetDragLeave() {
  Send(new ViewMsg_DragTargetDragLeave(routing_id()));
}

void RenderViewHost::DragTargetDrop(
    const gfx::Point& client_pt, const gfx::Point& screen_pt) {
  Send(new ViewMsg_DragTargetDrop(routing_id(), client_pt, screen_pt));
}

void RenderViewHost::ReservePageIDRange(int size) {
  Send(new ViewMsg_ReservePageIDRange(routing_id(), size));
}

void RenderViewHost::ExecuteJavascriptInWebFrame(
    const std::wstring& frame_xpath, const std::wstring& jscript) {
  Send(new ViewMsg_ScriptEvalRequest(routing_id(), frame_xpath, jscript));
}

void RenderViewHost::InsertCSSInWebFrame(
    const std::wstring& frame_xpath, const std::string& css) {
  Send(new ViewMsg_CSSInsertRequest(routing_id(), frame_xpath, css));
}

void RenderViewHost::AddMessageToConsole(
    const string16& frame_xpath,
    const string16& message,
    const WebConsoleMessage::Level& level) {
  Send(new ViewMsg_AddMessageToConsole(
      routing_id(), frame_xpath, message, level));
}

void RenderViewHost::Undo() {
  Send(new ViewMsg_Undo(routing_id()));
}

void RenderViewHost::Redo() {
  Send(new ViewMsg_Redo(routing_id()));
}

void RenderViewHost::Cut() {
  Send(new ViewMsg_Cut(routing_id()));
}

void RenderViewHost::Copy() {
  Send(new ViewMsg_Copy(routing_id()));
}

void RenderViewHost::Paste() {
  Send(new ViewMsg_Paste(routing_id()));
}

void RenderViewHost::Replace(const std::wstring& text_to_replace) {
  Send(new ViewMsg_Replace(routing_id(), text_to_replace));
}

void RenderViewHost::ToggleSpellCheck() {
  Send(new ViewMsg_ToggleSpellCheck(routing_id()));
}

void RenderViewHost::AddToDictionary(const std::wstring& word) {
  process()->AddWord(word);
}

void RenderViewHost::Delete() {
  Send(new ViewMsg_Delete(routing_id()));
}

void RenderViewHost::SelectAll() {
  Send(new ViewMsg_SelectAll(routing_id()));
}

int RenderViewHost::DownloadImage(const GURL& url, int image_size) {
  if (!url.is_valid()) {
    NOTREACHED();
    return 0;
  }
  static int next_id = 1;
  int id = next_id++;
  Send(new ViewMsg_DownloadImage(routing_id(), id, url, image_size));
  return id;
}

void RenderViewHost::GetApplicationInfo(int32 page_id) {
  Send(new ViewMsg_GetApplicationInfo(routing_id(), page_id));
}

void RenderViewHost::CaptureThumbnail() {
  Send(new ViewMsg_CaptureThumbnail(routing_id()));
}

void RenderViewHost::JavaScriptMessageBoxClosed(IPC::Message* reply_msg,
                                                bool success,
                                                const std::wstring& prompt) {
  if (is_waiting_for_unload_ack_) {
    if (are_javascript_messages_suppressed_) {
      delegate_->RendererUnresponsive(this, is_waiting_for_unload_ack_);
      return;
    }

    StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));
  }

  ViewHostMsg_RunJavaScriptMessage::WriteReplyParams(reply_msg,
                                                     success, prompt);
  Send(reply_msg);
}

void RenderViewHost::JavaScriptMessageBoxWindowDestroyed() {
 if (--modal_dialog_count_ == 0)
   modal_dialog_event_->Reset();
}

void RenderViewHost::ModalHTMLDialogClosed(IPC::Message* reply_msg,
                                           const std::string& json_retval) {
  if (is_waiting_for_unload_ack_)
    StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));

  if (--modal_dialog_count_ == 0)
    modal_dialog_event_->Reset();

  ViewHostMsg_ShowModalHTMLDialog::WriteReplyParams(reply_msg, json_retval);
  Send(reply_msg);
}

void RenderViewHost::CopyImageAt(int x, int y) {
  Send(new ViewMsg_CopyImageAt(routing_id(), x, y));
}

void RenderViewHost::DragSourceCancelledAt(
    int client_x, int client_y, int screen_x, int screen_y) {
  Send(new ViewMsg_DragSourceEndedOrMoved(
      routing_id(),
      gfx::Point(client_x, client_y),
      gfx::Point(screen_x, screen_y),
      true, true));
}

void RenderViewHost::DragSourceEndedAt(
    int client_x, int client_y, int screen_x, int screen_y) {
  Send(new ViewMsg_DragSourceEndedOrMoved(
      routing_id(),
      gfx::Point(client_x, client_y),
      gfx::Point(screen_x, screen_y),
      true, false));
}

void RenderViewHost::DragSourceMovedTo(
    int client_x, int client_y, int screen_x, int screen_y) {
  Send(new ViewMsg_DragSourceEndedOrMoved(
      routing_id(),
      gfx::Point(client_x, client_y),
      gfx::Point(screen_x, screen_y),
      false, false));
}

void RenderViewHost::DragSourceSystemDragEnded() {
  Send(new ViewMsg_DragSourceSystemDragEnded(routing_id()));
}

void RenderViewHost::AllowBindings(int bindings_flags) {
  DCHECK(!renderer_initialized_);
  enabled_bindings_ |= bindings_flags;
}

void RenderViewHost::SetDOMUIProperty(const std::string& name,
                                      const std::string& value) {
  DCHECK(BindingsPolicy::is_dom_ui_enabled(enabled_bindings_));
  Send(new ViewMsg_SetDOMUIProperty(routing_id(), name, value));
}

void RenderViewHost::GotFocus() {
  RenderWidgetHost::GotFocus();  // Notifies the renderer it got focus.

  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->GotFocus();
}

bool RenderViewHost::CanBlur() const {
  return delegate_->CanBlur();
}

void RenderViewHost::SetInitialFocus(bool reverse) {
  Send(new ViewMsg_SetInitialFocus(routing_id(), reverse));
}

void RenderViewHost::ClearFocusedNode() {
  Send(new ViewMsg_ClearFocusedNode(routing_id()));
}

void RenderViewHost::UpdateWebPreferences(const WebPreferences& prefs) {
  Send(new ViewMsg_UpdateWebPreferences(routing_id(), prefs));
}

void RenderViewHost::InstallMissingPlugin() {
  Send(new ViewMsg_InstallMissingPlugin(routing_id()));
}

void RenderViewHost::FileSelected(const FilePath& path) {
  ChildProcessSecurityPolicy::GetInstance()->GrantUploadFile(process()->pid(),
                                                         path);
  std::vector<FilePath> files;
  files.push_back(path);
  Send(new ViewMsg_RunFileChooserResponse(routing_id(), files));
}

void RenderViewHost::MultiFilesSelected(
         const std::vector<FilePath>& files) {
  for (std::vector<FilePath>::const_iterator file = files.begin();
       file != files.end(); ++file) {
    ChildProcessSecurityPolicy::GetInstance()->GrantUploadFile(
      process()->pid(), *file);
  }
  Send(new ViewMsg_RunFileChooserResponse(routing_id(), files));
}

void RenderViewHost::LoadStateChanged(const GURL& url,
                                      net::LoadState load_state) {
  delegate_->LoadStateChanged(url, load_state);
}

bool RenderViewHost::SuddenTerminationAllowed() const {
  return sudden_termination_allowed_ || process()->sudden_termination_allowed();
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHost, IPC message handlers:

void RenderViewHost::OnMessageReceived(const IPC::Message& msg) {
#if defined(OS_WIN)
  // On Windows there's a potential deadlock with sync messsages going in
  // a circle from browser -> plugin -> renderer -> browser.
  // On Linux we can avoid this by avoiding sync messages from browser->plugin.
  // On Mac we avoid this by not supporting windowed plugins.
  if (msg.is_sync() && !msg.is_caller_pumping_messages()) {
    NOTREACHED() << "Can't send sync messages to UI thread without pumping "
        "messages in the renderer or else deadlocks can occur if the page "
        "has windowed plugins! (message type " << msg.type() << ")";
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
    reply->set_reply_error();
    Send(reply);
    return;
  }
#endif

  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderViewHost, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowView, OnMsgShowView)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnMsgShowWidget)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_RunModal, OnMsgRunModal)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewReady, OnMsgRenderViewReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewGone, OnMsgRenderViewGone)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_FrameNavigate, OnMsgNavigate(msg))
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateState, OnMsgUpdateState)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTitle, OnMsgUpdateTitle)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateEncoding, OnMsgUpdateEncoding)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTargetURL, OnMsgUpdateTargetURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Thumbnail, OnMsgThumbnail)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateInspectorSettings,
                        OnUpdateInspectorSettings);
    IPC_MESSAGE_HANDLER(ViewHostMsg_Close, OnMsgClose)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestMove, OnMsgRequestMove)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidStartLoading, OnMsgDidStartLoading)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidStopLoading, OnMsgDidStopLoading)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidLoadResourceFromMemoryCache,
                        OnMsgDidLoadResourceFromMemoryCache)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidRedirectProvisionalLoad,
                        OnMsgDidRedirectProvisionalLoad)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidStartProvisionalLoadForFrame,
                        OnMsgDidStartProvisionalLoadForFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidFailProvisionalLoadWithError,
                        OnMsgDidFailProvisionalLoadWithError)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Find_Reply, OnMsgFindReply)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateFavIconURL, OnMsgUpdateFavIconURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidDownloadImage, OnMsgDidDownloadImage)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ContextMenu, OnMsgContextMenu)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenURL, OnMsgOpenURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidContentsPreferredWidthChange,
                        OnMsgDidContentsPreferredWidthChange)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DomOperationResponse,
                        OnMsgDomOperationResponse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DOMUISend,
                        OnMsgDOMUISend)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardMessageToExternalHost,
                        OnMsgForwardMessageToExternalHost)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentLoadedInFrame,
                        OnMsgDocumentLoadedInFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GoToEntryAtOffset,
                        OnMsgGoToEntryAtOffset)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetTooltipText, OnMsgSetTooltipText)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RunFileChooser, OnMsgRunFileChooser)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_RunJavaScriptMessage,
                                    OnMsgRunJavaScriptMessage)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_RunBeforeUnloadConfirm,
                                    OnMsgRunBeforeUnloadConfirm)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ShowModalHTMLDialog,
                                    OnMsgShowModalHTMLDialog)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PasswordFormsSeen, OnMsgPasswordFormsSeen)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AutofillFormSubmitted,
                        OnMsgAutofillFormSubmitted)
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartDragging, OnMsgStartDragging)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateDragCursor, OnUpdateDragCursor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PageHasOSDD, OnMsgPageHasOSDD)
    IPC_MESSAGE_FORWARD(ViewHostMsg_DidGetPrintedPagesCount,
                        delegate_,
                        RenderViewHostDelegate::DidGetPrintedPagesCount)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidPrintPage, DidPrintPage)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AddMessageToConsole, OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardToDevToolsAgent,
                        OnForwardToDevToolsAgent);
    IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardToDevToolsClient,
                        OnForwardToDevToolsClient);
    IPC_MESSAGE_HANDLER(ViewHostMsg_ActivateDevToolsWindow,
                        OnActivateDevToolsWindow);
    IPC_MESSAGE_HANDLER(ViewHostMsg_CloseDevToolsWindow,
                        OnCloseDevToolsWindow);
    IPC_MESSAGE_HANDLER(ViewHostMsg_DockDevToolsWindow,
                        OnDockDevToolsWindow);
    IPC_MESSAGE_HANDLER(ViewHostMsg_UndockDevToolsWindow,
                        OnUndockDevToolsWindow);
    IPC_MESSAGE_HANDLER(ViewHostMsg_UserMetricsRecordAction,
                        OnUserMetricsRecordAction)
    IPC_MESSAGE_HANDLER(ViewHostMsg_MissingPluginStatus, OnMissingPluginStatus);
    IPC_MESSAGE_FORWARD(ViewHostMsg_CrashedPlugin, delegate_,
                        RenderViewHostDelegate::OnCrashedPlugin);
    IPC_MESSAGE_HANDLER(ViewHostMsg_SendCurrentPageAllSavableResourceLinks,
                        OnReceivedSavableResourceLinksForCurrentPage);
    IPC_MESSAGE_HANDLER(ViewHostMsg_SendSerializedHtmlData,
                        OnReceivedSerializedHtmlData);
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidGetApplicationInfo,
                        OnDidGetApplicationInfo);
    IPC_MESSAGE_FORWARD(ViewHostMsg_JSOutOfMemory, delegate_,
                        RenderViewHostDelegate::OnJSOutOfMemory);
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShouldClose_ACK, OnMsgShouldCloseACK);
    IPC_MESSAGE_HANDLER(ViewHostMsg_QueryFormFieldAutofill,
                        OnQueryFormFieldAutofill)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RemoveAutofillEntry,
                        OnRemoveAutofillEntry)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ExtensionRequest, OnExtensionRequest)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectionChanged, OnMsgSelectionChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PasteFromSelectionClipboard,
                        OnMsgPasteFromSelectionClipboard)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ExtensionPostMessage,
                        OnExtensionPostMessage)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AccessibilityFocusChange,
                        OnAccessibilityFocusChange)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OnCSSInserted, OnCSSInserted)
    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(RenderWidgetHost::OnMessageReceived(msg))
  IPC_END_MESSAGE_MAP_EX()

  if (!msg_is_ok) {
    // The message had a handler, but its de-serialization failed.
    // Kill the renderer.
    process()->ReceivedBadMessage(msg.type());
  }
}

void RenderViewHost::Shutdown() {
  // If we are being run modally (see RunModal), then we need to cleanup.
  if (run_modal_reply_msg_) {
    if (--modal_dialog_count_ == 0)
      modal_dialog_event_->Reset();
    Send(run_modal_reply_msg_);
    run_modal_reply_msg_ = NULL;
  }
  RenderWidgetHost::Shutdown();
}

void RenderViewHost::CreateNewWindow(int route_id,
                                     ModalDialogEvent modal_dialog_event) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (!view)
    return;

  base::WaitableEvent* waitable_event = new base::WaitableEvent(
#if defined(OS_WIN)
      modal_dialog_event.event);
#else
      true, false);
#endif
  view->CreateNewWindow(route_id, waitable_event);
}

void RenderViewHost::CreateNewWidget(int route_id, bool activatable) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->CreateNewWidget(route_id, activatable);
}

void RenderViewHost::OnMsgShowView(int route_id,
                                   WindowOpenDisposition disposition,
                                   const gfx::Rect& initial_pos,
                                   bool user_gesture,
                                   const GURL& creator_url) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view) {
    view->ShowCreatedWindow(route_id, disposition, initial_pos, user_gesture,
                            creator_url);
    Send(new ViewMsg_Move_ACK(route_id));
  }
}

void RenderViewHost::OnMsgShowWidget(int route_id,
                                     const gfx::Rect& initial_pos) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view) {
    view->ShowCreatedWidget(route_id, initial_pos);
    Send(new ViewMsg_Move_ACK(route_id));
  }
}

void RenderViewHost::OnMsgRunModal(IPC::Message* reply_msg) {
  DCHECK(!run_modal_reply_msg_);
  if (modal_dialog_count_++ == 0)
    modal_dialog_event_->Signal();
  run_modal_reply_msg_ = reply_msg;

  // TODO(darin): Bug 1107929: Need to inform our delegate to show this view in
  // an app-modal fashion.
}

void RenderViewHost::OnMsgRenderViewReady() {
  WasResized();
  delegate_->RenderViewReady(this);
}

void RenderViewHost::OnMsgRenderViewGone() {
  // Our base class RenderWidgetHost needs to reset some stuff.
  RendererExited();

  delegate_->RenderViewGone(this);
}

// Called when the renderer navigates.  For every frame loaded, we'll get this
// notification containing parameters identifying the navigation.
//
// Subframes are identified by the page transition type.  For subframes loaded
// as part of a wider page load, the page_id will be the same as for the top
// level frame.  If the user explicitly requests a subframe navigation, we will
// get a new page_id because we need to create a new navigation entry for that
// action.
void RenderViewHost::OnMsgNavigate(const IPC::Message& msg) {
  // Read the parameters out of the IPC message directly to avoid making another
  // copy when we filter the URLs.
  void* iter = NULL;
  ViewHostMsg_FrameNavigate_Params validated_params;
  if (!IPC::ParamTraits<ViewHostMsg_FrameNavigate_Params>::
      Read(&msg, &iter, &validated_params))
    return;

  const int renderer_id = process()->pid();
  ChildProcessSecurityPolicy* policy = ChildProcessSecurityPolicy::GetInstance();
  // Without this check, an evil renderer can trick the browser into creating
  // a navigation entry for a banned URL.  If the user clicks the back button
  // followed by the forward button (or clicks reload, or round-trips through
  // session restore, etc), we'll think that the browser commanded the
  // renderer to load the URL and grant the renderer the privileges to request
  // the URL.  To prevent this attack, we block the renderer from inserting
  // banned URLs into the navigation controller in the first place.
  FilterURL(policy, renderer_id, &validated_params.url);
  FilterURL(policy, renderer_id, &validated_params.referrer);
  for (std::vector<GURL>::iterator it(validated_params.redirects.begin());
      it != validated_params.redirects.end(); ++it) {
    FilterURL(policy, renderer_id, &(*it));
  }
  FilterURL(policy, renderer_id, &validated_params.searchable_form_url);
  FilterURL(policy, renderer_id, &validated_params.password_form.origin);
  FilterURL(policy, renderer_id, &validated_params.password_form.action);

  delegate_->DidNavigate(this, validated_params);

  UpdateBackForwardListCount();
}

void RenderViewHost::OnMsgUpdateState(int32 page_id,
                                      const std::string& state) {
  delegate_->UpdateState(this, page_id, state);
}

void RenderViewHost::OnMsgUpdateTitle(int32 page_id,
                                      const std::wstring& title) {
  if (title.length() > chrome::kMaxTitleChars) {
    NOTREACHED() << "Renderer sent too many characters in title.";
    return;
  }
  delegate_->UpdateTitle(this, page_id, title);
}

void RenderViewHost::OnMsgUpdateEncoding(const std::wstring& encoding_name) {
  delegate_->UpdateEncoding(this, encoding_name);
}

void RenderViewHost::OnMsgUpdateTargetURL(int32 page_id,
                                          const GURL& url) {
  delegate_->UpdateTargetURL(page_id, url);

  // Send a notification back to the renderer that we are ready to
  // receive more target urls.
  Send(new ViewMsg_UpdateTargetURL_ACK(routing_id()));
}

void RenderViewHost::OnMsgThumbnail(const GURL& url,
                                    const ThumbnailScore& score,
                                    const SkBitmap& bitmap) {
  delegate_->UpdateThumbnail(url, bitmap, score);
}

void RenderViewHost::OnUpdateInspectorSettings(
    const std::wstring& raw_settings) {
  delegate_->UpdateInspectorSettings(raw_settings);
}

void RenderViewHost::OnMsgClose() {
  delegate_->Close(this);
}

void RenderViewHost::OnMsgRequestMove(const gfx::Rect& pos) {
  delegate_->RequestMove(pos);
  Send(new ViewMsg_Move_ACK(routing_id()));
}

void RenderViewHost::OnMsgDidRedirectProvisionalLoad(int32 page_id,
                                                     const GURL& source_url,
                                                     const GURL& target_url) {
  delegate_->DidRedirectProvisionalLoad(page_id, source_url, target_url);
}

void RenderViewHost::OnMsgDidStartLoading() {
  delegate_->DidStartLoading(this);
}

void RenderViewHost::OnMsgDidStopLoading() {
  delegate_->DidStopLoading(this);
}

void RenderViewHost::OnMsgDidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& frame_origin,
    const std::string& main_frame_origin,
    const std::string& security_info) {
  delegate_->DidLoadResourceFromMemoryCache(
      url, frame_origin, main_frame_origin, security_info);
}

void RenderViewHost::OnMsgDidStartProvisionalLoadForFrame(bool is_main_frame,
                                                          const GURL& url) {
  GURL validated_url(url);
  FilterURL(ChildProcessSecurityPolicy::GetInstance(),
            process()->pid(), &validated_url);

  delegate_->DidStartProvisionalLoadForFrame(this, is_main_frame,
                                             validated_url);
}

void RenderViewHost::OnMsgDidFailProvisionalLoadWithError(
    bool is_main_frame,
    int error_code,
    const GURL& url,
    bool showing_repost_interstitial) {
  GURL validated_url(url);
  FilterURL(ChildProcessSecurityPolicy::GetInstance(),
            process()->pid(), &validated_url);

  delegate_->DidFailProvisionalLoadWithError(this, is_main_frame,
                                             error_code, validated_url,
                                             showing_repost_interstitial);
}

void RenderViewHost::OnMsgFindReply(int request_id,
                                    int number_of_matches,
                                    const gfx::Rect& selection_rect,
                                    int active_match_ordinal,
                                    bool final_update) {
  delegate_->OnFindReply(request_id, number_of_matches, selection_rect,
                         active_match_ordinal, final_update);

  // Send a notification to the renderer that we are ready to receive more
  // results from the scoping effort of the Find operation. The FindInPage
  // scoping is asynchronous and periodically sends results back up to the
  // browser using IPC. In an effort to not spam the browser we have the
  // browser send an ACK for each FindReply message and have the renderer
  // queue up the latest status message while waiting for this ACK.
  Send(new ViewMsg_FindReplyACK(routing_id()));
}

void RenderViewHost::OnMsgUpdateFavIconURL(int32 page_id,
                                           const GURL& icon_url) {
  delegate_->UpdateFavIconURL(this, page_id, icon_url);
}

void RenderViewHost::OnMsgDidDownloadImage(
    int id,
    const GURL& image_url,
    bool errored,
    const SkBitmap& image) {
  delegate_->DidDownloadImage(this, id, image_url, errored, image);
}

void RenderViewHost::OnMsgContextMenu(const ContextMenuParams& params) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (!view)
    return;

  // Validate the URLs in |params|.  If the renderer can't request the URLs
  // directly, don't show them in the context menu.
  ContextMenuParams validated_params(params);
  const int renderer_id = process()->pid();
  ChildProcessSecurityPolicy* policy = ChildProcessSecurityPolicy::GetInstance();

  // We don't validate |unfiltered_link_url| so that this field can be used
  // when users want to copy the original link URL.
  FilterURL(policy, renderer_id, &validated_params.link_url);
  FilterURL(policy, renderer_id, &validated_params.image_url);
  FilterURL(policy, renderer_id, &validated_params.page_url);
  FilterURL(policy, renderer_id, &validated_params.frame_url);

  view->ShowContextMenu(validated_params);
}

void RenderViewHost::OnMsgOpenURL(const GURL& url,
                                  const GURL& referrer,
                                  WindowOpenDisposition disposition) {
  GURL validated_url(url);
  FilterURL(ChildProcessSecurityPolicy::GetInstance(),
            process()->pid(), &validated_url);

  delegate_->RequestOpenURL(validated_url, referrer, disposition);
}

void RenderViewHost::OnMsgDidContentsPreferredWidthChange(int pref_width) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (!view)
    return;
  view->UpdatePreferredWidth(pref_width);
}

void RenderViewHost::OnMsgDomOperationResponse(
    const std::string& json_string, int automation_id) {
  delegate_->DomOperationResponse(json_string, automation_id);

  // We also fire a notification for more loosely-coupled use cases.
  DomOperationNotificationDetails details(json_string, automation_id);
  NotificationService::current()->Notify(
      NotificationType::DOM_OPERATION_RESPONSE, Source<RenderViewHost>(this),
      Details<DomOperationNotificationDetails>(&details));
}

void RenderViewHost::OnMsgDOMUISend(
    const std::string& message, const std::string& content) {
  if (!ChildProcessSecurityPolicy::GetInstance()->
          HasDOMUIBindings(process()->pid())) {
    NOTREACHED() << "Blocked unauthorized use of DOMUIBindings.";
    return;
  }

  // DOMUI doesn't use these values yet.
  // TODO(aa): When DOMUI is ported to ExtensionFunctionDispatcher, send real
  // values here.
  const int kRequestId = -1;
  const bool kHasCallback = false;

  delegate_->ProcessDOMUIMessage(message, content, kRequestId, kHasCallback);
}

void RenderViewHost::OnMsgForwardMessageToExternalHost(
    const std::string& message, const std::string& origin,
    const std::string& target) {
  delegate_->ProcessExternalHostMessage(message, origin, target);
}

void RenderViewHost::OnMsgDocumentLoadedInFrame() {
  delegate_->DocumentLoadedInFrame();
}

void RenderViewHost::DisassociateFromPopupCount() {
  Send(new ViewMsg_DisassociateFromPopupCount(routing_id()));
}

void RenderViewHost::PopupNotificationVisibilityChanged(bool visible) {
  Send(new ViewMsg_PopupNotificationVisibilityChanged(routing_id(), visible));
}

void RenderViewHost::OnMsgGoToEntryAtOffset(int offset) {
  delegate_->GoToEntryAtOffset(offset);
}

void RenderViewHost::OnMsgSetTooltipText(const std::wstring& tooltip_text) {
  if (view())
    view()->SetTooltipText(tooltip_text);
}

void RenderViewHost::OnMsgSelectionChanged(const std::string& text) {
  if (view())
    view()->SelectionChanged(text);
}

void RenderViewHost::OnMsgPasteFromSelectionClipboard() {
  if (view())
    view()->PasteFromSelectionClipboard();
}

void RenderViewHost::OnMsgRunFileChooser(bool multiple_files,
                                         const string16& title,
                                         const FilePath& default_file) {
  delegate_->RunFileChooser(multiple_files, title, default_file);
}

void RenderViewHost::OnMsgRunJavaScriptMessage(
    const std::wstring& message,
    const std::wstring& default_prompt,
    const GURL& frame_url,
    const int flags,
    IPC::Message* reply_msg) {
  StopHangMonitorTimeout();
  if (modal_dialog_count_++ == 0)
    modal_dialog_event_->Signal();
  delegate_->RunJavaScriptMessage(message, default_prompt, frame_url, flags,
                                  reply_msg,
                                  &are_javascript_messages_suppressed_);
}

void RenderViewHost::OnMsgRunBeforeUnloadConfirm(const GURL& frame_url,
                                                 const std::wstring& message,
                                                 IPC::Message* reply_msg) {
  StopHangMonitorTimeout();
  if (modal_dialog_count_++ == 0)
    modal_dialog_event_->Signal();
  delegate_->RunBeforeUnloadConfirm(message, reply_msg);
}

void RenderViewHost::OnMsgShowModalHTMLDialog(
    const GURL& url, int width, int height, const std::string& json_arguments,
    IPC::Message* reply_msg) {
  StopHangMonitorTimeout();
  if (modal_dialog_count_++ == 0)
    modal_dialog_event_->Signal();
  delegate_->ShowModalHTMLDialog(url, width, height, json_arguments, reply_msg);
}

void RenderViewHost::OnMsgPasswordFormsSeen(
    const std::vector<webkit_glue::PasswordForm>& forms) {
  delegate_->PasswordFormsSeen(forms);
}

void RenderViewHost::OnMsgAutofillFormSubmitted(
    const webkit_glue::AutofillForm& form) {
  delegate_->AutofillFormSubmitted(form);
}

void RenderViewHost::OnMsgStartDragging(
    const WebDropData& drop_data) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->StartDragging(drop_data);
}

void RenderViewHost::OnUpdateDragCursor(bool is_drop_target) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->UpdateDragCursor(is_drop_target);
}

void RenderViewHost::OnTakeFocus(bool reverse) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view)
    view->TakeFocus(reverse);
}

void RenderViewHost::OnMsgPageHasOSDD(int32 page_id, const GURL& doc_url,
                                      bool autodetected) {
  delegate_->PageHasOSDD(this, page_id, doc_url, autodetected);
}

void RenderViewHost::DidPrintPage(
    const ViewHostMsg_DidPrintPage_Params& params) {
  delegate_->DidPrintPage(params);
}

void RenderViewHost::OnAddMessageToConsole(const std::wstring& message,
                                           int32 line_no,
                                           const std::wstring& source_id) {
  std::wstring msg = StringPrintf(L"\"%ls,\" source: %ls (%d)", message.c_str(),
                                  source_id.c_str(), line_no);
  logging::LogMessage("CONSOLE", 0).stream() << msg;
}

void RenderViewHost::OnForwardToDevToolsAgent(const IPC::Message& message) {
  DevToolsManager::GetInstance()->ForwardToDevToolsAgent(this, message);
}

void RenderViewHost::OnForwardToDevToolsClient(const IPC::Message& message) {
  DevToolsManager::GetInstance()->ForwardToDevToolsClient(this, message);
}

void RenderViewHost::OnActivateDevToolsWindow() {
  DevToolsManager::GetInstance()->ActivateWindow(this);
}

void RenderViewHost::OnCloseDevToolsWindow() {
  DevToolsManager::GetInstance()->CloseWindow(this);
}

void RenderViewHost::OnDockDevToolsWindow() {
  DevToolsManager::GetInstance()->DockWindow(this);
}

void RenderViewHost::OnUndockDevToolsWindow() {
  DevToolsManager::GetInstance()->UndockWindow(this);
}

void RenderViewHost::OnUserMetricsRecordAction(const std::wstring& action) {
  UserMetrics::RecordComputedAction(action.c_str(), process()->profile());
}

void RenderViewHost::UnhandledKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view) {
    // TODO(brettw) why do we have to filter these types of events here. Can't
    // the renderer just send us the ones we care abount, or maybe the view
    // should be able to decide which ones it wants or not?
    if ((event.type == WebInputEvent::RawKeyDown) ||
        (event.type == WebInputEvent::KeyDown) ||
        (event.type == WebInputEvent::Char)) {
      view->HandleKeyboardEvent(event);
    }
  }
}

void RenderViewHost::OnUserGesture() {
  delegate_->OnUserGesture();
}

void RenderViewHost::OnMissingPluginStatus(int status) {
  delegate_->OnMissingPluginStatus(status);
}

void RenderViewHost::UpdateBackForwardListCount() {
  int back_list_count = 0, forward_list_count = 0;
  delegate_->GetHistoryListCount(&back_list_count, &forward_list_count);
  Send(new ViewMsg_UpdateBackForwardListCount(
      routing_id(), back_list_count, forward_list_count));
}

void RenderViewHost::GetAllSavableResourceLinksForCurrentPage(
    const GURL& page_url) {
  Send(new ViewMsg_GetAllSavableResourceLinksForCurrentPage(routing_id(),
                                                            page_url));
}

void RenderViewHost::OnReceivedSavableResourceLinksForCurrentPage(
    const std::vector<GURL>& resources_list,
    const std::vector<GURL>& referrers_list,
    const std::vector<GURL>& frames_list) {
  RenderViewHostDelegate::Save* save_delegate = delegate_->GetSaveDelegate();
  if (save_delegate) {
    save_delegate->OnReceivedSavableResourceLinksForCurrentPage(
        resources_list, referrers_list, frames_list);
  }
}

void RenderViewHost::OnDidGetApplicationInfo(
    int32 page_id,
    const webkit_glue::WebApplicationInfo& info) {
  delegate_->OnDidGetApplicationInfo(page_id, info);
}

void RenderViewHost::GetSerializedHtmlDataForCurrentPageWithLocalLinks(
    const std::vector<GURL>& links,
    const std::vector<FilePath>& local_paths,
    const FilePath& local_directory_name) {
  Send(new ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks(
      routing_id(), links, local_paths, local_directory_name));
}

void RenderViewHost::OnReceivedSerializedHtmlData(const GURL& frame_url,
                                                  const std::string& data,
                                                  int32 status) {
  RenderViewHostDelegate::Save* save_delegate = delegate_->GetSaveDelegate();
  if (save_delegate)
    save_delegate->OnReceivedSerializedHtmlData(frame_url, data, status);
}

void RenderViewHost::OnMsgShouldCloseACK(bool proceed) {
  StopHangMonitorTimeout();
  DCHECK(is_waiting_for_unload_ack_);
  is_waiting_for_unload_ack_ = false;
  delegate_->ShouldClosePage(proceed);
}

void RenderViewHost::OnQueryFormFieldAutofill(const std::wstring& field_name,
                                              const std::wstring& user_text,
                                              int64 node_id,
                                              int request_id) {
  delegate_->GetAutofillSuggestions(field_name, user_text, node_id, request_id);
}

void RenderViewHost::OnRemoveAutofillEntry(const std::wstring& field_name,
                                           const std::wstring& value) {
  delegate_->RemoveAutofillEntry(field_name, value);
}

void RenderViewHost::AutofillSuggestionsReturned(
    const std::vector<std::wstring>& suggestions,
    int64 node_id, int request_id, int default_suggestion_index) {
  Send(new ViewMsg_AutofillSuggestions(routing_id(), node_id,
      request_id, suggestions, -1));
  // Default index -1 means no default suggestion.
}

void RenderViewHost::WindowMoveOrResizeStarted() {
  Send(new ViewMsg_MoveOrResizeStarted(routing_id()));
}

void RenderViewHost::NotifyRendererUnresponsive() {
  delegate_->RendererUnresponsive(this, is_waiting_for_unload_ack_);
}

void RenderViewHost::NotifyRendererResponsive() {
  delegate_->RendererResponsive(this);
}

gfx::Rect RenderViewHost::GetRootWindowResizerRect() const {
  return delegate_->GetRootWindowResizerRect();
}

void RenderViewHost::ForwardMouseEvent(
    const WebKit::WebMouseEvent& mouse_event) {
  // We make a copy of the mouse event because
  // RenderWidgetHost::ForwardMouseEvent will delete |mouse_event|.
  WebKit::WebMouseEvent event_copy(mouse_event);
  RenderWidgetHost::ForwardMouseEvent(event_copy);

  RenderViewHostDelegate::View* view = delegate_->GetViewDelegate();
  if (view) {
    switch (event_copy.type) {
      case WebInputEvent::MouseMove:
        view->HandleMouseEvent();
        break;
      case WebInputEvent::MouseLeave:
        view->HandleMouseLeave();
        break;
      default:
        // For now, we don't care about the rest.
        break;
    }
  }
}

void RenderViewHost::ForwardEditCommand(const std::string& name,
                                        const std::string& value) {
  IPC::Message* message = new ViewMsg_ExecuteEditCommand(routing_id(),
                                                         name,
                                                         value);
  Send(message);
}

void RenderViewHost::ForwardMessageFromExternalHost(const std::string& message,
                                                    const std::string& origin,
                                                    const std::string& target) {
  Send(new ViewMsg_HandleMessageFromExternalHost(routing_id(), message, origin,
                                                 target));
}

void RenderViewHost::OnExtensionRequest(const std::string& name,
                                        const std::string& args,
                                        int request_id,
                                        bool has_callback) {
  if (!ChildProcessSecurityPolicy::GetInstance()->
          HasExtensionBindings(process()->pid())) {
    NOTREACHED() << "Blocked unauthorized use of extension bindings.";
    return;
  }

  delegate_->ProcessDOMUIMessage(name, args, request_id, has_callback);
}

void RenderViewHost::SendExtensionResponse(int request_id, bool success,
                                           const std::string& response,
                                           const std::string& error) {
  Send(new ViewMsg_ExtensionResponse(routing_id(), request_id, success,
      response, error));
}

void RenderViewHost::OnExtensionPostMessage(
    int port_id, const std::string& message) {
  URLRequestContext* context = process()->profile()->GetRequestContext();
  ExtensionMessageService::GetInstance(context)->
      PostMessageFromRenderer(port_id, message);
}

void RenderViewHost::OnAccessibilityFocusChange(int acc_obj_id) {
#if defined(OS_WIN)
  BrowserAccessibilityManager::GetInstance()->
      ChangeAccessibilityFocus(acc_obj_id, process()->pid(), routing_id());
#else
  // TODO(port): accessibility not yet implemented. See http://crbug.com/8288.
#endif
}

void RenderViewHost::OnCSSInserted() {
  delegate_->DidInsertCSS();
}
