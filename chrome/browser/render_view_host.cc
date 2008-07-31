// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/render_view_host.h"

#include <string>
#include <vector>

#include "base/string_util.h"
#include "chrome/app/result_codes.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cross_site_request_manager.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/render_widget_host.h"
#include "chrome/browser/render_widget_host_view.h"
#include "chrome/browser/render_view_host_delegate.h"
#include "chrome/browser/renderer_security_policy.h"
#include "chrome/browser/debugger/debugger_wrapper.h"
#include "chrome/browser/site_instance.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/thumbnail_score.h"
#include "net/base/net_util.h"
#include "skia/include/SkBitmap.h"

namespace {

void FilterURL(RendererSecurityPolicy* policy, int renderer_id, GURL* url) {
  if (!url->is_valid())
    return;  // We don't need to block invalid URLs.

  if (url->SchemeIs("about")) {
    // The renderer treats all URLs in the about: scheme as being about:blank.
    // Canonicalize about: URLs to about:blank.
    *url = GURL("about:blank");
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
                               HANDLE modal_dialog_event)
    : RenderWidgetHost(instance->GetProcess(), routing_id),
      instance_(instance),
      enable_dom_ui_bindings_(false),
      delegate_(delegate),
      renderer_initialized_(false),
      waiting_for_drag_context_response_(false),
      debugger_attached_(false),
      modal_dialog_count_(0),
      navigations_suspended_(false),
      suspended_nav_message_(NULL),
      run_modal_reply_msg_(NULL),
      has_unload_listener_(false),
      is_waiting_for_unload_ack_(false) {
  DCHECK(instance_);
  DCHECK(delegate_);
  if (modal_dialog_event == NULL)
    modal_dialog_event = CreateEvent(NULL, TRUE, FALSE, NULL);

  modal_dialog_event_.Set(modal_dialog_event);
}

RenderViewHost::~RenderViewHost() {
  OnDebugDisconnect();

  // Be sure to clean up any leftover state from cross-site requests.
  Singleton<CrossSiteRequestManager>()->SetHasPendingCrossSiteRequest(
      process()->host_id(), routing_id_, false);
}

bool RenderViewHost::CreateRenderView() {
  DCHECK(!IsRenderViewLive()) << "Creating view twice";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!process_->Init())
    return false;
  DCHECK(process_->channel());
  DCHECK(process_->profile());

  renderer_initialized_ = true;

  HANDLE modal_dialog_event;
  HANDLE renderer_process_handle = process()->process();
  if (renderer_process_handle == NULL)
    renderer_process_handle = GetCurrentProcess();

  BOOL result = DuplicateHandle(GetCurrentProcess(),
      modal_dialog_event_.Get(),
      renderer_process_handle,
      &modal_dialog_event,
      SYNCHRONIZE,
      FALSE,
      0);
  DCHECK(result) << "Couldn't duplicate the modal dialog handle for the renderer.";

  DCHECK(view_);
  Send(new ViewMsg_New(view_->GetPluginHWND(),
                       modal_dialog_event,
                       delegate_->GetWebkitPrefs(),
                       routing_id_));

  // Set the alternate error page, which is profile specific, in the renderer.
  GURL url = delegate_->GetAlternateErrorPageURL();
  SetAlternateErrorPageURL(url);

  // If it's enabled, tell the renderer to set up the Javascript bindings for
  // sending messages back to the browser.
  if (enable_dom_ui_bindings_)
    Send(new ViewMsg_AllowDOMUIBindings(routing_id_));

  // Let our delegate know that we created a RenderView.
  delegate_->RendererCreated(this);

  return true;
}

bool RenderViewHost::IsRenderViewLive() const {
  return process_->channel() && renderer_initialized_;
}

void RenderViewHost::Init() {
  RenderWidgetHost::Init();
  renderer_initialized_ = true;
}

void RenderViewHost::NavigateToEntry(const NavigationEntry& entry,
                                     bool is_reload) {
  ViewMsg_Navigate_Params params;
  MakeNavigateParams(entry, is_reload, &params);

  RendererSecurityPolicy::GetInstance()->GrantRequestURL(
      process()->host_id(), params.url);

  DoNavigate(new ViewMsg_Navigate(routing_id_, params));

  UpdateBackForwardListCount();
}

void RenderViewHost::NavigateToURL(const GURL& url) {
  ViewMsg_Navigate_Params params;
  params.page_id = -1;
  params.url = url;
  params.transition = PageTransition::LINK;
  params.reload = false;

  RendererSecurityPolicy::GetInstance()->GrantRequestURL(
      process()->host_id(), params.url);

  DoNavigate(new ViewMsg_Navigate(routing_id_, params));
}

void RenderViewHost::DoNavigate(ViewMsg_Navigate* nav_message) {
  // Only send the message if we aren't suspended at the start of a cross-site
  // request.
  if (navigations_suspended_) {
    // Shouldn't be possible to have a second navigation while suspended, since
    // navigations will only be suspended during a cross-site request.  If a
    // second navigation occurs, WebContents will cancel this pending RVH
    // create a new pending RVH.
    DCHECK(!suspended_nav_message_.get());
    suspended_nav_message_.reset(nav_message);
  } else {
    Send(nav_message);
  }
}

void RenderViewHost::LoadAlternateHTMLString(const std::string& html_text,
                                             bool new_navigation,
                                             const GURL& display_url,
                                             const std::string& security_info) {
  Send(new ViewMsg_LoadAlternateHTMLText(routing_id_, html_text,
                                         new_navigation, display_url,
                                         security_info));
}

void RenderViewHost::SetNavigationsSuspended(bool suspend) {
  DCHECK(navigations_suspended_ != suspend);
  navigations_suspended_ = suspend;
  if (!suspend && suspended_nav_message_.get()) {
    // Resume navigation
    Send(suspended_nav_message_.release());
  }
}

void RenderViewHost::AttemptToClosePage(bool is_closing_browser) {
  if (IsRenderViewLive()) {
    // Start the hang monitor in case the renderer hangs in the beforeunload
    // handler.
    DCHECK(!is_waiting_for_unload_ack_);
    is_waiting_for_unload_ack_ = true;
    StartHangMonitorTimeout(kUnloadTimeoutMS);
    Send(new ViewMsg_ShouldClose(routing_id_, is_closing_browser));
  } else {
    // This RenderViewHost doesn't have a live renderer, so just skip running
    // the onbeforeunload handler.
    OnMsgShouldCloseACK(true, is_closing_browser);
  }
}

void RenderViewHost::OnProceedWithClosePage(bool is_closing_browser) {
  // Start the hang monitor in case the renderer hangs in the unload handler.
  DCHECK(!is_waiting_for_unload_ack_);
  is_waiting_for_unload_ack_ = true;
  StartHangMonitorTimeout(kUnloadTimeoutMS);
  ClosePage(site_instance()->process_host_id(), 
            routing_id(),
            is_closing_browser);
}

// static
void RenderViewHost::ClosePageIgnoringUnloadEvents(int render_process_host_id,
                                                   int request_id,
                                                   bool is_closing_browser) {
  RenderViewHost* rvh = RenderViewHost::FromID(render_process_host_id,
                                               request_id);
  if (!rvh)
    return;

  rvh->StopHangMonitorTimeout();
  DCHECK(rvh->is_waiting_for_unload_ack_);
  rvh->is_waiting_for_unload_ack_ = false;

  // The RenderViewHost's delegate is a WebContents.
  TabContents* tab = static_cast<WebContents*>(rvh->delegate());
  rvh->UnloadListenerHasFired();

  if (is_closing_browser) {
    tab->delegate()->UnloadFired(tab);
  } else {
    rvh->delegate()->Close(rvh);
  }
}

void RenderViewHost::ClosePage(int new_render_process_host_id,
                               int new_request_id,
                               bool is_closing_browser) {
  if (IsRenderViewLive()) {
    Send(new ViewMsg_ClosePage(routing_id_,
                               new_render_process_host_id,
                               new_request_id,
                               is_closing_browser));
  } else {
    // This RenderViewHost doesn't have a live renderer, so just skip closing
    // the page.  We must notify the ResourceDispatcherHost on the IO thread,
    // which we will do through the RenderProcessHost's widget helper.
    process()->CrossSiteClosePageACK(new_render_process_host_id,
                                     new_request_id,
                                     is_closing_browser);
  }
}

void RenderViewHost::SetHasPendingCrossSiteRequest(bool has_pending_request) {
  Singleton<CrossSiteRequestManager>()->SetHasPendingCrossSiteRequest(
      process()->host_id(), routing_id_, has_pending_request);
}

void RenderViewHost::OnCrossSiteResponse(int new_render_process_host_id,
                                         int new_request_id) {
  delegate_->OnCrossSiteResponse(new_render_process_host_id, new_request_id);
}

void RenderViewHost::Stop() {
  Send(new ViewMsg_Stop(routing_id_));
}

bool RenderViewHost::GetPrintedPagesCount(const ViewMsg_Print_Params& params) {
  return Send(new ViewMsg_GetPrintedPagesCount(routing_id_, params));
}

bool RenderViewHost::PrintPages(const ViewMsg_PrintPages_Params& params) {
  return Send(new ViewMsg_PrintPages(routing_id_, params));
}

void RenderViewHost::StartFinding(int request_id,
                                  const std::wstring& search_string,
                                  bool forward,
                                  bool match_case,
                                  bool find_next) {
  FindInPageRequest request;
  request.request_id = request_id;
  request.search_string = search_string;
  request.forward = forward;
  request.match_case = match_case;
  request.find_next = find_next;
  Send(new ViewMsg_Find(routing_id_, request));

  // This call is asynchronous and returns immediately.
  // The result of the search is sent as a notification message by the renderer.
}

void RenderViewHost::StopFinding(bool clear_selection) {
  Send(new ViewMsg_StopFinding(routing_id_, clear_selection));
}

void RenderViewHost::SendFindReplyAck() {
  Send(new ViewMsg_FindReplyACK(routing_id_));
}

void RenderViewHost::AlterTextSize(text_zoom::TextSize size) {
  Send(new ViewMsg_AlterTextSize(routing_id_, size));
}

void RenderViewHost::SetPageEncoding(const std::wstring& encoding_name) {
  Send(new ViewMsg_SetPageEncoding(routing_id_, encoding_name));
}

void RenderViewHost::SetAlternateErrorPageURL(const GURL& url) {
  Send(new ViewMsg_SetAltErrorPageURL(routing_id_, url));
}

void RenderViewHost::FillForm(const FormData& form_data) {
  Send(new ViewMsg_FormFill(routing_id_, form_data));
}

void RenderViewHost::FillPasswordForm(
    const PasswordFormDomManager::FillData& form_data) {
  Send(new ViewMsg_FillPasswordForm(routing_id_, form_data));
}

void RenderViewHost::DragTargetDragEnter(const WebDropData& drop_data,
    const gfx::Point& client_pt, const gfx::Point& screen_pt) {
  // Grant the renderer the ability to load the drop_data.
  RendererSecurityPolicy* policy = RendererSecurityPolicy::GetInstance();
  policy->GrantRequestURL(process()->host_id(), drop_data.url);
  for (std::vector<std::wstring>::const_iterator iter(drop_data.filenames.begin());
       iter != drop_data.filenames.end(); ++iter) {
    policy->GrantRequestURL(process()->host_id(),
                            net::FilePathToFileURL(*iter));
    policy->GrantUploadFile(process()->host_id(), *iter);
  }
  Send(new ViewMsg_DragTargetDragEnter(routing_id_, drop_data, client_pt,
                                       screen_pt));
}

void RenderViewHost::DragTargetDragOver(
    const gfx::Point& client_pt, const gfx::Point& screen_pt) {
  Send(new ViewMsg_DragTargetDragOver(routing_id_, client_pt, screen_pt));
}

void RenderViewHost::DragTargetDragLeave() {
  Send(new ViewMsg_DragTargetDragLeave(routing_id_));
}

void RenderViewHost::DragTargetDrop(
    const gfx::Point& client_pt, const gfx::Point& screen_pt) {
  Send(new ViewMsg_DragTargetDrop(routing_id_, client_pt, screen_pt));
}

void RenderViewHost::UploadFile(const std::wstring& file_path,
                                const std::wstring& form,
                                const std::wstring& file,
                                const std::wstring& submit,
                                const std::wstring& other_values) {
  if (!process_->channel())
    return;

  RendererSecurityPolicy::GetInstance()->GrantUploadFile(process()->host_id(),
                                                         file);
  ViewMsg_UploadFile_Params p;
  p.file_path = file_path;
  p.form = form;
  p.file = file;
  p.submit = submit;
  p.other_values = other_values;
  Send(new ViewMsg_UploadFile(routing_id_, p));
}

void RenderViewHost::ReservePageIDRange(int size) {
  Send(new ViewMsg_ReservePageIDRange(routing_id_, size));
}

void RenderViewHost::ExecuteJavascriptInWebFrame(
    const std::wstring& frame_xpath, const std::wstring& jscript) {
  Send(new ViewMsg_ScriptEvalRequest(routing_id_, frame_xpath, jscript));
}

void RenderViewHost::AddMessageToConsole(
    const std::wstring& frame_xpath, const std::wstring& msg,
    ConsoleMessageLevel level) {
  Send(new ViewMsg_AddMessageToConsole(routing_id_, frame_xpath, msg, level));
}

void RenderViewHost::SendToDebugger(const std::wstring& cmd) {
  Send(new ViewMsg_SendToDebugger(routing_id_, cmd));
}

void RenderViewHost::DebugAttach() {
  if (!debugger_attached_)
    Send(new ViewMsg_DebugAttach(routing_id_));
}

void RenderViewHost::DebugDetach() {
  if (debugger_attached_) {
    SendToDebugger(L"quit");
    debugger_attached_ = false;
  }
}

void RenderViewHost::DebugBreak() {
  if (debugger_attached_)
    SendToDebugger(L"break");
}

void RenderViewHost::Undo() {
  Send(new ViewMsg_Undo(routing_id_));
}

void RenderViewHost::Redo() {
  Send(new ViewMsg_Redo(routing_id_));
}

void RenderViewHost::Cut() {
  Send(new ViewMsg_Cut(routing_id_));
}

void RenderViewHost::Copy() {
  Send(new ViewMsg_Copy(routing_id_));
}

void RenderViewHost::Paste() {
  Send(new ViewMsg_Paste(routing_id_));
}

void RenderViewHost::Replace(const std::wstring& text_to_replace) {
  Send(new ViewMsg_Replace(routing_id_, text_to_replace));
}

void RenderViewHost::Delete() {
  Send(new ViewMsg_Delete(routing_id_));
}

void RenderViewHost::SelectAll() {
  Send(new ViewMsg_SelectAll(routing_id_));
}

int RenderViewHost::DownloadImage(const GURL& url, int image_size) {
  if (!url.is_valid()) {
    NOTREACHED();
    return 0;
  }
  static int next_id = 1;
  int id = next_id++;
  Send(new ViewMsg_DownloadImage(routing_id_, id, url, image_size));
  return id;
}

void RenderViewHost::GetApplicationInfo(int32 page_id) {
  Send(new ViewMsg_GetApplicationInfo(routing_id_, page_id));
}

void RenderViewHost::CaptureThumbnail() {
  Send(new ViewMsg_CaptureThumbnail(routing_id_));
}

void RenderViewHost::JavaScriptMessageBoxClosed(IPC::Message* reply_msg,
                                                bool success,
                                                const std::wstring& prompt) {
  if (is_waiting_for_unload_ack_)
    StartHangMonitorTimeout(kUnloadTimeoutMS);

  if (--modal_dialog_count_ == 0)
    ResetEvent(modal_dialog_event_.Get());
  ViewHostMsg_RunJavaScriptMessage::WriteReplyParams(reply_msg, success, prompt);
  Send(reply_msg);
}

void RenderViewHost::ModalHTMLDialogClosed(IPC::Message* reply_msg,
                                           const std::string& json_retval) {
  if (is_waiting_for_unload_ack_)
    StartHangMonitorTimeout(kUnloadTimeoutMS);

  if (--modal_dialog_count_ == 0)
    ResetEvent(modal_dialog_event_.Get());

  ViewHostMsg_ShowModalHTMLDialog::WriteReplyParams(reply_msg, json_retval);
  Send(reply_msg);
}

void RenderViewHost::CopyImageAt(int x, int y) {
  Send(new ViewMsg_CopyImageAt(routing_id_, x, y));
}

void RenderViewHost::InspectElementAt(int x, int y) {
  RendererSecurityPolicy::GetInstance()->GrantInspectElement(
      process()->host_id());
  Send(new ViewMsg_InspectElement(routing_id_, x, y));
}

void RenderViewHost::ShowJavaScriptConsole() {
  RendererSecurityPolicy::GetInstance()->GrantInspectElement(
      process()->host_id());

  Send(new ViewMsg_ShowJavaScriptConsole(routing_id_));
}

void RenderViewHost::DragSourceEndedAt(
    int client_x, int client_y, int screen_x, int screen_y) {
  Send(new ViewMsg_DragSourceEndedOrMoved(
      routing_id_, client_x, client_y, screen_x, screen_y, true));
}

void RenderViewHost::DragSourceMovedTo(
    int client_x, int client_y, int screen_x, int screen_y) {
  Send(new ViewMsg_DragSourceEndedOrMoved(
      routing_id_, client_x, client_y, screen_x, screen_y, false));
}

void RenderViewHost::DragSourceSystemDragEnded() {
  Send(new ViewMsg_DragSourceSystemDragEnded(routing_id_));
}

void RenderViewHost::AllowDomAutomationBindings() {
  // Expose the binding that allows the DOM to send messages here.
  Send(new ViewMsg_AllowDomAutomationBindings(routing_id_, true));
}

void RenderViewHost::AllowDOMUIBindings() {
  DCHECK(!renderer_initialized_);
  enable_dom_ui_bindings_ = true;
  RendererSecurityPolicy::GetInstance()->GrantDOMUIBindings(process()->host_id());
}

void RenderViewHost::SetDOMUIProperty(const std::string& name,
                                      const std::string& value) {
  DCHECK(enable_dom_ui_bindings_);
  Send(new ViewMsg_SetDOMUIProperty(routing_id_, name, value));
}

// static
void RenderViewHost::MakeNavigateParams(const NavigationEntry& entry,
                                        bool reload,
                                        ViewMsg_Navigate_Params* params) {
  params->page_id = entry.GetPageID();
  params->url = entry.GetURL();
  params->transition = entry.GetTransitionType();
  params->state = entry.GetContentState();
  params->reload = reload;
}

bool RenderViewHost::CanBlur() const {
  return delegate_->CanBlur();
}

void RenderViewHost::SetInitialFocus(bool reverse) {
  Send(new ViewMsg_SetInitialFocus(routing_id_, reverse));
}

void RenderViewHost::UpdateWebPreferences(const WebPreferences& prefs) {
  Send(new ViewMsg_UpdateWebPreferences(routing_id_, prefs));
}

void RenderViewHost::InstallMissingPlugin() {
  Send(new ViewMsg_InstallMissingPlugin(routing_id_));
}

void RenderViewHost::FileSelected(const std::wstring& path) {
  RendererSecurityPolicy::GetInstance()->GrantUploadFile(process()->host_id(),
                                                         path);
  Send(new ViewMsg_RunFileChooserResponse(routing_id_, path));
}

void RenderViewHost::LoadStateChanged(const GURL& url,
                                      net::LoadState load_state) {
  delegate_->LoadStateChanged(url, load_state);
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHost, IPC message handlers:

void RenderViewHost::OnMessageReceived(const IPC::Message& msg) {
  if (msg.is_sync() && !msg.is_caller_pumping_messages()) {
    NOTREACHED() << "Can't send sync messages to UI thread without pumping " \
        "messages in the renderer or else deadlocks can occur if the page" \
        "has windowed plugins!";
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
    reply->set_reply_error();
    Send(reply);
    return;
  }

  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderViewHost, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateViewWithRoute, OnMsgCreateView)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidgetWithRoute, OnMsgCreateWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowView, OnMsgShowView)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnMsgShowWidget)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_RunModal, OnMsgRunModal)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RendererReady, OnMsgRendererReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RendererGone, OnMsgRendererGone)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_FrameNavigate, OnMsgNavigate(msg))
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateState, OnMsgUpdateState)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTitle, OnMsgUpdateTitle)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateEncoding, OnMsgUpdateEncoding)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTargetURL, OnMsgUpdateTargetURL)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_Thumbnail, OnMsgThumbnail(msg))
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
    IPC_MESSAGE_HANDLER(ViewHostMsg_DomOperationResponse,
                        OnMsgDomOperationResponse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DOMUISend,
                        OnMsgDOMUISend)
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
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartDragging, OnMsgStartDragging)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateDragCursor, OnUpdateDragCursor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PageHasOSDD, OnMsgPageHasOSDD)
    IPC_MESSAGE_HANDLER(ViewHostMsg_InspectElement_Reply,
                        OnMsgInspectElementReply)
    IPC_MESSAGE_FORWARD(ViewHostMsg_DidGetPrintedPagesCount,
                        delegate_,
                        RenderViewHostDelegate::DidGetPrintedPagesCount)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidPrintPage, DidPrintPage)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AddMessageToConsole, OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DebuggerOutput, OnDebuggerOutput);
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidDebugAttach, DidDebugAttach);
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
    IPC_MESSAGE_HANDLER(ViewHostMsg_UnloadListenerChanged,
                        OnUnloadListenerChanged);
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
      ResetEvent(modal_dialog_event_.Get());
    Send(run_modal_reply_msg_);
    run_modal_reply_msg_ = NULL;
  }
  RenderWidgetHost::Shutdown();
}

void RenderViewHost::OnMsgCreateView(int route_id, HANDLE modal_dialog_event) {
  delegate_->CreateView(route_id, modal_dialog_event);
}

void RenderViewHost::OnMsgCreateWidget(int route_id) {
  delegate_->CreateWidget(route_id);
}

void RenderViewHost::OnMsgShowView(int route_id,
                                   WindowOpenDisposition disposition,
                                   const gfx::Rect& initial_pos,
                                   bool user_gesture) {
  delegate_->ShowView(route_id, disposition, initial_pos, user_gesture);
}

void RenderViewHost::OnMsgShowWidget(int route_id,
                                     const gfx::Rect& initial_pos) {
  delegate_->ShowWidget(route_id, initial_pos);
}

void RenderViewHost::OnMsgRunModal(IPC::Message* reply_msg) {
  DCHECK(!run_modal_reply_msg_);
  if (modal_dialog_count_++ == 0)
    SetEvent(modal_dialog_event_.Get());
  run_modal_reply_msg_ = reply_msg;

  // TODO(darin): Bug 1107929: Need to inform our delegate to show this view in
  // an app-modal fashion.
}

void RenderViewHost::OnMsgRendererReady() {
  WasResized();
  delegate_->RendererReady(this);
}

void RenderViewHost::OnMsgRendererGone() {
  // Must reset these to ensure that mouse move events work with a new renderer.
  mouse_move_pending_ = false;
  next_mouse_move_.reset();

  // Clearing this flag causes us to re-create the renderer when recovering
  // from a crashed renderer.
  renderer_initialized_ = false;

  // Reset some fields in preparation for recovering from a crash.
  resize_ack_pending_ = false;
  current_size_ = gfx::Size();
  is_hidden_ = false;

  backing_store_.reset();

  if (view_) {
    view_->RendererGone();
    view_ = NULL;  // The View should be deleted by RendererGone.
  }
  delegate_->RendererGone(this);
  OnDebugDisconnect();
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

  const int renderer_id = process()->host_id();
  RendererSecurityPolicy* policy = RendererSecurityPolicy::GetInstance();
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
                                      const GURL& url,
                                      const std::wstring& title,
                                      const std::string& state) {
  GURL validated_url(url);
  FilterURL(RendererSecurityPolicy::GetInstance(),
            process()->host_id(), &validated_url);

  delegate_->UpdateState(this, page_id, validated_url, title, state);
}

void RenderViewHost::OnMsgUpdateTitle(int32 page_id,
                                      const std::wstring& title) {
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
  Send(new ViewMsg_UpdateTargetURL_ACK(routing_id_));
}

void RenderViewHost::OnMsgThumbnail(const IPC::Message& msg) {
  // crack the message
  void* iter = NULL;
  GURL url;
  if (!IPC::ParamTraits<GURL>::Read(&msg, &iter, &url))
    return;

  ThumbnailScore score;
  if (!IPC::ParamTraits<ThumbnailScore>::Read(&msg, &iter, &score))
    return;

  // thumbnail data
  SkBitmap bitmap;
  if (!IPC::ParamTraits<SkBitmap>::Read(&msg, &iter, &bitmap))
    return;

  delegate_->UpdateThumbnail(url, bitmap, score);
}

void RenderViewHost::OnMsgClose() {
  delegate_->Close(this);
}

void RenderViewHost::OnMsgRequestMove(const gfx::Rect& pos) {
  delegate_->RequestMove(pos);
}

void RenderViewHost::OnMsgDidRedirectProvisionalLoad(int32 page_id,
                                                     const GURL& source_url,
                                                     const GURL& target_url) {
  delegate_->DidRedirectProvisionalLoad(page_id, source_url, target_url);
}

void RenderViewHost::OnMsgDidStartLoading(int32 page_id) {
  delegate_->DidStartLoading(this, page_id);

  if (view_) {
    view_->UpdateCursorIfOverSelf();
  }
}

void RenderViewHost::OnMsgDidStopLoading(int32 page_id) {
  delegate_->DidStopLoading(this, page_id);

  if (view_) {
    view_->UpdateCursorIfOverSelf();
  }
}

void RenderViewHost::OnMsgDidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& security_info) {
  delegate_->DidLoadResourceFromMemoryCache(url, security_info);
}

void RenderViewHost::OnMsgDidStartProvisionalLoadForFrame(bool is_main_frame,
                                                          const GURL& url) {
  GURL validated_url(url);
  FilterURL(RendererSecurityPolicy::GetInstance(),
            process()->host_id(), &validated_url);

  delegate_->DidStartProvisionalLoadForFrame(this, is_main_frame, validated_url);
}

void RenderViewHost::OnMsgDidFailProvisionalLoadWithError(
    bool is_main_frame,
    int error_code,
    const GURL& url,
    bool showing_repost_interstitial) {
  GURL validated_url(url);
  FilterURL(RendererSecurityPolicy::GetInstance(),
            process()->host_id(), &validated_url);

  delegate_->DidFailProvisionalLoadWithError(this, is_main_frame,
                                             error_code, validated_url,
                                             showing_repost_interstitial);
}

void RenderViewHost::OnMsgFindReply(int request_id,
                                    int number_of_matches,
                                    const gfx::Rect& selection_rect,
                                    int active_match_ordinal,
                                    bool final_update) {
  delegate_->FindReply(request_id, number_of_matches,
                       selection_rect, active_match_ordinal, final_update);
  SendFindReplyAck();
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

void RenderViewHost::OnMsgContextMenu(
    const ViewHostMsg_ContextMenu_Params& params) {
  // Validate the URLs in |params|.  If the renderer can't request the URLs
  // directly, don't show them in the context menu.
  ViewHostMsg_ContextMenu_Params validated_params(params);
  const int renderer_id = process()->host_id();
  RendererSecurityPolicy* policy = RendererSecurityPolicy::GetInstance();

  FilterURL(policy, renderer_id, &validated_params.link_url);
  FilterURL(policy, renderer_id, &validated_params.image_url);
  FilterURL(policy, renderer_id, &validated_params.page_url);
  FilterURL(policy, renderer_id, &validated_params.frame_url);

  delegate_->ShowContextMenu(validated_params);
}

void RenderViewHost::OnMsgOpenURL(const GURL& url,
                                  WindowOpenDisposition disposition) {
  GURL validated_url(url);
  FilterURL(RendererSecurityPolicy::GetInstance(),
            process()->host_id(), &validated_url);

  delegate_->RequestOpenURL(validated_url, disposition);
}

void RenderViewHost::OnMsgDomOperationResponse(
    const std::string& json_string, int automation_id) {
  delegate_->DomOperationResponse(json_string, automation_id);
}

void RenderViewHost::OnMsgDOMUISend(
    const std::string& message, const std::string& content) {
  if (!RendererSecurityPolicy::GetInstance()->
          HasDOMUIBindings(process()->host_id())) {
    NOTREACHED() << "Blocked unauthorized use of DOMUIBindings.";
    return;
  }
  delegate_->ProcessDOMUIMessage(message, content);
}

void RenderViewHost::OnMsgGoToEntryAtOffset(int offset) {
  delegate_->GoToEntryAtOffset(offset);
}

void RenderViewHost::OnMsgSetTooltipText(const std::wstring& tooltip_text) {
  if (view_) {
    view_->SetTooltipText(tooltip_text);
  }
}

void RenderViewHost::OnMsgRunFileChooser(const std::wstring& default_file) {
  delegate_->RunFileChooser(default_file);
}

void RenderViewHost::OnMsgRunJavaScriptMessage(
    const std::wstring& message,
    const std::wstring& default_prompt,
    const int flags,
    IPC::Message* reply_msg) {
  StopHangMonitorTimeout();
  if (modal_dialog_count_++ == 0)
    SetEvent(modal_dialog_event_.Get());
  delegate_->RunJavaScriptMessage(message, default_prompt, flags, reply_msg);
}

void RenderViewHost::OnMsgRunBeforeUnloadConfirm(const std::wstring& message,
                                                 IPC::Message* reply_msg) {
  StopHangMonitorTimeout();
  if (modal_dialog_count_++ == 0)
    SetEvent(modal_dialog_event_.Get());
  delegate_->RunBeforeUnloadConfirm(message, reply_msg);
}

void RenderViewHost::OnMsgShowModalHTMLDialog(
    const GURL& url, int width, int height, const std::string& json_arguments,
    IPC::Message* reply_msg) {
  StopHangMonitorTimeout();
  if (modal_dialog_count_++ == 0)
    SetEvent(modal_dialog_event_.Get());
  delegate_->ShowModalHTMLDialog(url, width, height, json_arguments, reply_msg);
}

void RenderViewHost::OnMsgPasswordFormsSeen(
    const std::vector<PasswordForm>& forms) {
  delegate_->PasswordFormsSeen(forms);
}

void RenderViewHost::OnMsgStartDragging(
    const WebDropData& drop_data) {
  delegate_->StartDragging(drop_data);
}

void RenderViewHost::OnUpdateDragCursor(bool is_drop_target) {
  delegate_->UpdateDragCursor(is_drop_target);
}

void RenderViewHost::OnTakeFocus(bool reverse) {
  delegate_->TakeFocus(reverse);
}

void RenderViewHost::OnMsgPageHasOSDD(int32 page_id, const GURL& doc_url,
                                      bool autodetected) {
  delegate_->PageHasOSDD(this, page_id, doc_url, autodetected);
}

void RenderViewHost::OnMsgInspectElementReply(int num_resources) {
  delegate_->InspectElementReply(num_resources);
}

void RenderViewHost::DidPrintPage(
    const ViewHostMsg_DidPrintPage_Params& params) {
  delegate_->DidPrintPage(params);
}

void RenderViewHost::OnAddMessageToConsole(const std::wstring& message,
                                           int32 line_no,
                                           const std::wstring& source_id) {
  std::wstring msg = StringPrintf(L"\"%s,\" source: %s (%d)", message.c_str(),
                                  source_id.c_str(), line_no);
  logging::LogMessage("CONSOLE", 0).stream() << msg;
  if (debugger_attached_)
    g_browser_process->debugger_wrapper()->DebugMessage(msg);
}

void RenderViewHost::OnDebuggerOutput(const std::wstring& output) {
  if (debugger_attached_)
    g_browser_process->debugger_wrapper()->DebugMessage(output);
}

void RenderViewHost::DidDebugAttach() {
  if (!debugger_attached_) {
    debugger_attached_ = true;
    SendToDebugger(L"attach");
  }
}

void RenderViewHost::OnUserMetricsRecordAction(const std::wstring& action) {
  UserMetrics::RecordComputedAction(action.c_str(), process_->profile());
}

void RenderViewHost::UnhandledInputEvent(const WebInputEvent& event) {
  if ((event.type == WebInputEvent::KEY_DOWN) ||
      (event.type == WebInputEvent::CHAR))
    delegate_->HandleKeyboardEvent(
        static_cast<const WebKeyboardEvent&>(event));
}

void RenderViewHost::OnMissingPluginStatus(int status) {
  delegate_->OnMissingPluginStatus(status);
}

void RenderViewHost::UpdateBackForwardListCount() {
  int back_list_count, forward_list_count;
  delegate_->GetHistoryListCount(&back_list_count, &forward_list_count);
  Send(new ViewMsg_UpdateBackForwardListCount(
      routing_id_, back_list_count, forward_list_count));
}

void RenderViewHost::GetAllSavableResourceLinksForCurrentPage(
    const GURL& page_url) {
  Send(new ViewMsg_GetAllSavableResourceLinksForCurrentPage(routing_id_,
                                                            page_url));
}

void RenderViewHost::OnReceivedSavableResourceLinksForCurrentPage(
    const std::vector<GURL>& resources_list,
    const std::vector<GURL>& referrers_list,
    const std::vector<GURL>& frames_list) {
  delegate_->OnReceivedSavableResourceLinksForCurrentPage(resources_list,
                                                          referrers_list,
                                                          frames_list);
}

void RenderViewHost::OnDidGetApplicationInfo(
    int32 page_id,
    const webkit_glue::WebApplicationInfo& info) {
  delegate_->OnDidGetApplicationInfo(page_id, info);
}

void RenderViewHost::GetSerializedHtmlDataForCurrentPageWithLocalLinks(
    const std::vector<std::wstring>& links,
    const std::vector<std::wstring>& local_paths,
    const std::wstring& local_directory_name) {
  Send(new ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks(
      routing_id_, links, local_paths, local_directory_name));
}

void RenderViewHost::OnReceivedSerializedHtmlData(const GURL& frame_url,
                                                  const std::string& data,
                                                  int32 status) {
  delegate_->OnReceivedSerializedHtmlData(frame_url, data, status);
}

void RenderViewHost::OnMsgShouldCloseACK(bool proceed,
                                         bool is_closing_browser) {
  StopHangMonitorTimeout();
  DCHECK(is_waiting_for_unload_ack_);
  is_waiting_for_unload_ack_ = false;

  if (is_closing_browser) {
    // The RenderViewHost's delegate is a WebContents.
    TabContents* tab = static_cast<WebContents*>(delegate());
    if (!tab)
      return;

    tab->delegate()->BeforeUnloadFired(tab, proceed);
  } else {
    delegate_->ShouldClosePage(proceed);
  }
}

void RenderViewHost::OnUnloadListenerChanged(bool has_listener) {
  has_unload_listener_ = has_listener;
}

void RenderViewHost::NotifyRendererUnresponsive() {
  if (is_waiting_for_unload_ack_) {
    // If the tab hangs in the beforeunload/unload handler there's really
    // nothing we can do to recover. We can safely kill the process and the
    // Browser will deal with the crash appropriately.
    TerminateProcess(process()->process(), ResultCodes::HUNG);
    return;
  }

  // If the debugger is attached, we're going to be unresponsive anytime it's
  // stopped at a breakpoint.
  if (!debugger_attached_)
    delegate_->RendererUnresponsive(this);
}

void RenderViewHost::NotifyRendererResponsive() {
  delegate_->RendererResponsive(this);
}

void RenderViewHost::OnDebugDisconnect() {
  if (debugger_attached_) {
    debugger_attached_ = false;
    g_browser_process->debugger_wrapper()->OnDebugDisconnect();
  }
}

void RenderViewHost::OnThemeChanged() {
  Send (new ViewMsg_ThemeChanged(routing_id_));
}

