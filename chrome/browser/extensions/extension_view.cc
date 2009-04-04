// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view.h"

#include "chrome/browser/extensions/extension.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/resource_bundle.h"

#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

ExtensionView::ExtensionView(Extension* extension,
                             const GURL& url,
                             Profile* profile)
    : HWNDHtmlView(url, this, false),
      extension_(extension),
      profile_(profile) {
  // Set the width initially to 0, so that the WebCore::Document can
  // correctly compute the minPrefWidth which is returned in
  // DidContentsChangeSize()
  set_preferred_size(gfx::Size(0, 100));
  SetVisible(false);
}

void ExtensionView::DidStopLoading(RenderViewHost* render_view_host,
      int32 page_id) {
  SetVisible(true);
  render_view_host->WasResized();
}

void ExtensionView::DidContentsPreferredWidthChange(const int pref_width) {
  if (pref_width > 0) {
    // SchedulePaint first because new_width may be smaller and we want
    // the Parent to paint the vacated space.
    SchedulePaint();
    set_preferred_size(gfx::Size(pref_width, 100));
    SizeToPreferredSize();

    // TODO(rafaelw): This assumes that the extension view is a child of an
    // ExtensionToolstrip, which is a child of the BookmarkBarView. There should
    // be a way to do this where the ExtensionView doesn't have to know it's
    // containment hierarchy.
    if (GetParent() != NULL && GetParent()->GetParent() != NULL) {
      GetParent()->GetParent()->Layout();
    }

    SchedulePaint();
    render_view_host()->WasResized();
  }
}

void ExtensionView::CreatingRenderer() {
  render_view_host()->AllowExtensionBindings();
}

void ExtensionView::RenderViewCreated(RenderViewHost* rvh) {
  ExtensionMessageService::GetInstance()->RegisterExtension(
      extension_->id(), render_view_host()->process()->pid());
}

WebPreferences ExtensionView::GetWebkitPrefs() {
  // TODO(mpcomplete): return some reasonable prefs.
  return WebPreferences();
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
