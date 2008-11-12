// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was forked off the Mac port.

#include "webkit/tools/test_shell/test_webview_delegate.h"

#include <gtk/gtk.h>

#include "base/gfx/point.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "chrome/common/page_transition_types.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/weberror.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell.h"

// WebViewDelegate -----------------------------------------------------------

TestWebViewDelegate::~TestWebViewDelegate() {
}

WebPluginDelegate* TestWebViewDelegate::CreatePluginDelegate(
    WebView* webview,
    const GURL& url,
    const std::string& mime_type,
    const std::string& clsid,
    std::string* actual_mime_type) {
  NOTIMPLEMENTED();
  return NULL;
}

void TestWebViewDelegate::ShowJavaScriptAlert(const std::wstring& message) {
  // TODO(port): remove GTK_WINDOW bit after gfx::WindowHandle is fixed.
  GtkWidget* dialog = gtk_message_dialog_new(
      GTK_WINDOW(shell_->mainWnd()), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
      GTK_BUTTONS_OK, "%s", WideToUTF8(message).c_str());
  gtk_window_set_title(GTK_DIALOG(dialog), "JavaScript Alert");
  gtk_dialog_run(GTK_DIALOG(dialog));  // Runs a nested message loop.
  gtk_widget_destroy(dialog);
}

void TestWebViewDelegate::Show(WebWidget* webview,
                               WindowOpenDisposition disposition) {
  NOTIMPLEMENTED();
}

void TestWebViewDelegate::CloseWidgetSoon(WebWidget* webwidget) {
  NOTIMPLEMENTED();
}

void TestWebViewDelegate::SetCursor(WebWidget* webwidget, 
                                    const WebCursor& cursor) {
  GdkCursorType cursor_type = cursor.GetCursorType();
  if (cursor_type_ == cursor_type)
    return;

  cursor_type_ = cursor_type;
  if (cursor_type_ == GDK_CURSOR_IS_PIXMAP) {
    NOTIMPLEMENTED();
    cursor_type = GDK_ARROW;  // Just a hack for now.
  }
  GdkCursor* gdk_cursor = gdk_cursor_new(cursor_type);
  gdk_window_set_cursor(shell_->webViewWnd()->window, gdk_cursor);
  // The window now owns the cursor.
  gdk_cursor_unref(gdk_cursor);
}

void TestWebViewDelegate::GetWindowRect(WebWidget* webwidget,
                                        gfx::Rect* out_rect) {
  DCHECK(out_rect);
  //if (WebWidgetHost* host = GetHostForWidget(webwidget)) {
    NOTIMPLEMENTED();
  //}
}

void TestWebViewDelegate::SetWindowRect(WebWidget* webwidget,
                                        const gfx::Rect& rect) {
  // TODO(port): window movement
  if (webwidget == shell_->webView()) {
    // ignored
  } else if (webwidget == shell_->popup()) {
    NOTIMPLEMENTED();
  }
}

void TestWebViewDelegate::GetRootWindowRect(WebWidget* webwidget,
                                            gfx::Rect* out_rect) {
  //if (WebWidgetHost* host = GetHostForWidget(webwidget)) {
    NOTIMPLEMENTED();
  //}
}

void TestWebViewDelegate::RunModal(WebWidget* webwidget) {
  NOTIMPLEMENTED();
}

// Private methods -----------------------------------------------------------

void TestWebViewDelegate::SetPageTitle(const std::wstring& title) {
  gtk_window_set_title(GTK_WINDOW(shell_->mainWnd()),
                       ("Test Shell - " + WideToUTF8(title)).c_str());
}

void TestWebViewDelegate::SetAddressBarURL(const GURL& url) {
  gtk_entry_set_text(GTK_ENTRY(shell_->editWnd()), url.spec().c_str());
}
