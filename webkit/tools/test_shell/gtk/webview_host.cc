// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "webkit/tools/test_shell/webview_host.h"

#include "base/gfx/platform_canvas.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "webkit/glue/webinputevent.h"
#include "webkit/glue/webview.h"

/*static*/
WebViewHost* WebViewHost::Create(GtkWidget* box,
                                 WebViewDelegate* delegate,
                                 const WebPreferences& prefs) {
  // TODO(agl):
  // /usr/local/google/agl/src/chrome/src/webkit/tools/test_shell/gtk/webview_host.cc:19: error: no matching function for call to 'WebWidgetHost::Create(GtkWidget*&, WebViewDelegate*&)'
  WebViewHost* host = reinterpret_cast<WebViewHost *>(WebWidgetHost::Create(box, NULL));

  // TODO(erg):
  // - Set "host->view_"
  // - Call "host->webwidget_->Resize"
  host->webwidget_ = WebView::Create(delegate, prefs);
  host->webwidget_->Resize(gfx::Size(640, 480));
  host->webwidget_->Layout();

  return host;
}

WebView* WebViewHost::webview() const {
  return static_cast<WebView*>(webwidget_);
}
