// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_host.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"

#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

#include "webkit/glue/context_menu.h"

ExtensionHost::ExtensionHost(Extension* extension, SiteInstance* site_instance)
    : extension_(extension), view_(NULL), did_stop_loading_(false) {
  render_view_host_ = new RenderViewHost(
      site_instance, this, MSG_ROUTING_NONE, NULL);
  render_view_host_->AllowExtensionBindings();
}

ExtensionHost::~ExtensionHost() {
  render_view_host_->Shutdown();  // deletes render_view_host
}

SiteInstance* ExtensionHost::site_instance() const {
  return render_view_host_->site_instance();
}

void ExtensionHost::CreateRenderView(const GURL& url,
                                     RenderWidgetHostView* host_view) {
  render_view_host_->set_view(host_view);
  render_view_host_->CreateRenderView();
  render_view_host_->NavigateToURL(url);
}

void ExtensionHost::DidContentsPreferredWidthChange(const int pref_width) {
  if (view_)
    view_->DidContentsPreferredWidthChange(pref_width);
}

void ExtensionHost::RenderViewCreated(RenderViewHost* rvh) {
  URLRequestContext* context = rvh->process()->profile()->GetRequestContext();
  ExtensionMessageService::GetInstance(context)->RegisterExtension(
      extension_->id(), rvh->process()->pid());
}

WebPreferences ExtensionHost::GetWebkitPrefs() {
  PrefService* prefs = render_view_host()->process()->profile()->GetPrefs();
  const bool kIsDomUI = true;
  return RenderViewHostDelegateHelper::GetWebkitPrefs(prefs, kIsDomUI);
}

void ExtensionHost::RunJavaScriptMessage(
    const std::wstring& message,
    const std::wstring& default_prompt,
    const GURL& frame_url,
    const int flags,
    IPC::Message* reply_msg,
    bool* did_suppress_message) {
  // Automatically cancel the javascript alert (otherwise the renderer hangs
  // indefinitely).
  *did_suppress_message = true;
  render_view_host()->JavaScriptMessageBoxClosed(reply_msg, true, L"");
}

void ExtensionHost::DidStartLoading(RenderViewHost* render_view_host) {
  static const StringPiece toolstrip_css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_EXTENSIONS_TOOLSTRIP_CSS));
  render_view_host->InsertCSSInWebFrame(L"", toolstrip_css.as_string());
}

void ExtensionHost::DidStopLoading(RenderViewHost* render_view_host) {
  render_view_host->WasResized();
  did_stop_loading_ = true;
  if (view_)
    view_->ShowIfCompletelyLoaded();
}

ExtensionFunctionDispatcher* ExtensionHost::
    CreateExtensionFunctionDispatcher(RenderViewHost *render_view_host,
                                      const std::string& extension_id) {
  return new ExtensionFunctionDispatcher(render_view_host, GetBrowser(),
                                         extension_id);
}

RenderViewHostDelegate::View* ExtensionHost::GetViewDelegate() const {
  // TODO(erikkay) this is unfortunate.  The interface declares that this method
  // must be const (no good reason for it as far as I can tell) which means you
  // can't return self without doing this const_cast.  Either we need to change
  // the interface, or we need to split out the view delegate into another
  // object (which is how WebContents works).
  return const_cast<ExtensionHost*>(this);
}

void ExtensionHost::CreateNewWindow(int route_id,
                                    base::WaitableEvent* modal_dialog_event) {
  delegate_view_helper_.CreateNewWindow(
      route_id, modal_dialog_event, render_view_host()->process()->profile(),
      site_instance());
}

void ExtensionHost::CreateNewWidget(int route_id, bool activatable) {
  delegate_view_helper_.CreateNewWidget(route_id, activatable,
                                        site_instance()->GetProcess());
}

void ExtensionHost::ShowCreatedWindow(int route_id,
                                      WindowOpenDisposition disposition,
                                      const gfx::Rect& initial_pos,
                                      bool user_gesture) {
  WebContents* contents = delegate_view_helper_.GetCreatedWindow(route_id);
  if (contents) {
    // TODO(erikkay) is it safe to pass in NULL as source?
    GetBrowser()->AddTabContents(contents, disposition, initial_pos,
                                 user_gesture);
  }
}

void ExtensionHost::ShowCreatedWidget(int route_id,
                                      const gfx::Rect& initial_pos) {
  RenderWidgetHostView* widget_host_view =
      delegate_view_helper_.GetCreatedWidget(route_id);
  GetBrowser()->BrowserRenderWidgetShowing();
  // TODO(erikkay): These two lines could be refactored with TabContentsView.
  widget_host_view->InitAsPopup(render_view_host()->view(), initial_pos);
  widget_host_view->GetRenderWidgetHost()->Init();
}

void ExtensionHost::ShowContextMenu(const ContextMenuParams& params) {
  // TODO(erikkay) - This is a temporary hack.  Show a menu here instead.
  render_view_host()->InspectElementAt(params.x, params.y);
}

void ExtensionHost::StartDragging(const WebDropData& drop_data) {
}

void ExtensionHost::UpdateDragCursor(bool is_drop_target) {
}

void ExtensionHost::TakeFocus(bool reverse) {
}

void ExtensionHost::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
}

Browser* ExtensionHost::GetBrowser() {
  if (view_)
    return view_->browser();
  Browser* browser = BrowserList::FindBrowserWithProfile(
      render_view_host()->process()->profile());
  // TODO(mpcomplete): what this verifies doesn't actually happen yet.
  CHECK(browser) << "ExtensionHost running in Profile with no Browser active."
      " It should have been deleted.";
  return browser;
}
