// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view.h"

#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/extensions/extension.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/jsmessage_box_handler.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"

#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

#include "webkit/glue/context_menu.h"

ExtensionView::ExtensionView(Extension* extension,
                             const GURL& url,
                             SiteInstance* instance,
                             Browser* browser)
    : HWNDHtmlView(url, this, false, instance),
      extension_(extension),
      browser_(browser),
      did_stop_loading_(false),
      pending_preferred_width_(0) {
}

void ExtensionView::ShowIfCompletelyLoaded() {
  // We wait to show the ExtensionView until it has loaded and our parent has
  // given us a background. These can happen in different orders.
  if (did_stop_loading_ && !render_view_host()->view()->background().empty()) {
    SetVisible(true);
    DidContentsPreferredWidthChange(pending_preferred_width_);
  }
}

void ExtensionView::SetBackground(const SkBitmap& background) {
  HWNDHtmlView::SetBackground(background);
  ShowIfCompletelyLoaded();
}

void ExtensionView::DidStopLoading(RenderViewHost* render_view_host,
      int32 page_id) {
  render_view_host->WasResized();
  did_stop_loading_ = true;
  ShowIfCompletelyLoaded();
}

void ExtensionView::DidContentsPreferredWidthChange(const int pref_width) {
  // Don't actually do anything with this information until we have been shown.
  // Size changes will not be honored by lower layers while we are hidden.
  if (!IsVisible()) {
    pending_preferred_width_ = pref_width;
  } else if (pref_width > 0) {
    set_preferred_size(gfx::Size(pref_width, height()));
    SizeToPreferredSize();

    // TODO(rafaelw): This assumes that the extension view is a child of an
    // ExtensionToolstrip, which is a child of the BookmarkBarView. There should
    // be a way to do this where the ExtensionView doesn't have to know it's
    // containment hierarchy.
    if (GetParent() != NULL && GetParent()->GetParent() != NULL) {
      GetParent()->GetParent()->Layout();
    }

    SchedulePaint();
  }
}

void ExtensionView::CreatingRenderer() {
  render_view_host()->AllowExtensionBindings();
  SetVisible(false);
}

void ExtensionView::RenderViewCreated(RenderViewHost* rvh) {
  URLRequestContext* context = rvh->process()->profile()->GetRequestContext();
  ExtensionMessageService::GetInstance(context)->RegisterExtension(
      extension_->id(), render_view_host()->process()->pid());
}

WebPreferences ExtensionView::GetWebkitPrefs() {
  PrefService* prefs = render_view_host()->process()->profile()->GetPrefs();
  bool isDomUI = true;
  return RenderViewHostDelegateHelper::GetWebkitPrefs(prefs, isDomUI);
}

void ExtensionView::RunJavaScriptMessage(
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

void ExtensionView::DidStartLoading(RenderViewHost* render_view_host,
                                    int32 page_id) {
  static const StringPiece toolstrip_css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_EXTENSIONS_TOOLSTRIP_CSS));
  render_view_host->InsertCSSInWebFrame(L"", toolstrip_css.as_string());
}

RenderViewHostDelegate::View* ExtensionView::GetViewDelegate() const {
  // TODO(erikkay) this is unfortunate.  The interface declares that this method
  // must be const (no good reason for it as far as I can tell) which means you
  // can't return self without doing this const_cast.  Either we need to change
  // the interface, or we need to split out the view delegate into another
  // object (which is how WebContents works).
  return const_cast<ExtensionView*>(this);
}

void ExtensionView::CreateNewWindow(int route_id,
                                    base::WaitableEvent* modal_dialog_event) {
  delegate_view_helper_.CreateNewWindow(route_id, modal_dialog_event,
                                        browser_->profile(), site_instance());
}

void ExtensionView::CreateNewWidget(int route_id, bool activatable) {
  delegate_view_helper_.CreateNewWidget(route_id, activatable,
                                        site_instance()->GetProcess());
}

void ExtensionView::ShowCreatedWindow(int route_id,
                                      WindowOpenDisposition disposition,
                                      const gfx::Rect& initial_pos,
                                      bool user_gesture) {
  WebContents* contents = delegate_view_helper_.GetCreatedWindow(route_id);
  if (contents) {
    browser_->AddTabContents(contents, disposition, initial_pos, user_gesture);
  }
}

void ExtensionView::ShowCreatedWidget(int route_id,
                                      const gfx::Rect& initial_pos) {
  RenderWidgetHostView* widget_host_view =
      delegate_view_helper_.GetCreatedWidget(route_id);
  browser_->BrowserRenderWidgetShowing();
  // TODO(erikkay): These two lines could be refactored with TabContentsView.
  widget_host_view->InitAsPopup(render_view_host()->view(),
                                initial_pos);
  widget_host_view->GetRenderWidgetHost()->Init();
}

void ExtensionView::ShowContextMenu(const ContextMenuParams& params) {
  // TODO(erikkay) - This is a temporary hack.  Show a menu here instead.
  render_view_host()->InspectElementAt(params.x, params.y);
}

void ExtensionView::StartDragging(const WebDropData& drop_data) {
}

void ExtensionView::UpdateDragCursor(bool is_drop_target) {
}

void ExtensionView::TakeFocus(bool reverse) {
}

void ExtensionView::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
}
