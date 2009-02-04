// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "webkit/tools/test_shell/webview_host.h"
#include "webkit/tools/test_shell/mac/test_shell_webview.h"

#include "base/gfx/platform_canvas.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "webkit/glue/webinputevent.h"
#include "webkit/glue/webview.h"

/*static*/
WebViewHost* WebViewHost::Create(NSView* parent_view,
                                 WebViewDelegate* delegate,
                                 const WebPreferences& prefs) {
  WebViewHost* host = new WebViewHost();

  NSRect content_rect = [parent_view frame];
  // bump down the top of the view so that it doesn't overlap the buttons
  // and URL field.  32 is an ad hoc constant.
  // TODO(awalker): replace explicit view layout with a little nib file
  // and use that for view geometry.
  content_rect.size.height -= 32;
  host->view_ = [[TestShellWebView alloc] initWithFrame:content_rect];
  // make the height and width track the window size.
  [host->view_ setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
  [parent_view addSubview:host->view_];
  [host->view_ release];

  host->webwidget_ = WebView::Create(delegate, prefs);
  host->webwidget_->Resize(gfx::Size(content_rect.size.width,
                                     content_rect.size.height));

  return host;
}

WebView* WebViewHost::webview() const {
  return static_cast<WebView*>(webwidget_);
}
