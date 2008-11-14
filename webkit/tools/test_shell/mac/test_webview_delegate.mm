// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_webview_delegate.h"

#import <Cocoa/Cocoa.h>
#include "base/string_util.h"
#include "webkit/glue/webview.h"
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
  // TODO(awalker): once Mac plugins are working, enable this code to
  // connect up the web plugin delegate for a plugin.
  NOTIMPLEMENTED();
  return NULL;
}

void TestWebViewDelegate::ShowJavaScriptAlert(const std::wstring& message) {
  NSString *text =
      [NSString stringWithUTF8String:WideToUTF8(message).c_str()];
  NSAlert *alert = [NSAlert alertWithMessageText:@"JavaScript Alert"
                                   defaultButton:@"OK"
                                 alternateButton:nil
                                     otherButton:nil
                       informativeTextWithFormat:text];
  [alert runModal];
}


// WebWidgetDelegate ---------------------------------------------------------

void TestWebViewDelegate::Show(WebWidget* webview,
                               WindowOpenDisposition disposition) {
}

void TestWebViewDelegate::CloseWidgetSoon(WebWidget* webwidget) {
  if (webwidget == shell_->webView()) {
    NSWindow *win = shell_->mainWnd();
    // Tell Cocoa to close the window, which will let the window's delegate
    // handle getting rid of the shell. |shell_| will still be alive for a short
    // period of time after this call returns (cleanup is done after going back
    // to the event loop), so we should make sure we don't leave it dangling.
    [win performClose:nil];
    shell_ = NULL;
  } else if (webwidget == shell_->popup()) {
    shell_->ClosePopup();
  }
}

void TestWebViewDelegate::SetCursor(WebWidget* webwidget, 
                                    const WebCursor& cursor) {
  //TODO: Mac cursor handling
}

void TestWebViewDelegate::GetWindowRect(WebWidget* webwidget,
                                        gfx::Rect* out_rect) {
  DCHECK(out_rect);
  if (WebWidgetHost* host = GetHostForWidget(webwidget)) {
    NSView *view = host->window_handle();
    NSRect rect = [view frame];
    *out_rect = gfx::Rect(NSRectToCGRect(rect));
  }
}

void TestWebViewDelegate::SetWindowRect(WebWidget* webwidget,
                                        const gfx::Rect& rect) {
  // TODO: Mac window movement
  if (webwidget == shell_->webView()) {
    // ignored
  } else if (webwidget == shell_->popup()) {
    // MoveWindow(shell_->popupWnd(),
    //            rect.x(), rect.y(), rect.width(), rect.height(), FALSE);
  }
}

void TestWebViewDelegate::GetRootWindowRect(WebWidget* webwidget,
                                            gfx::Rect* out_rect) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget)) {
    NSView *view = host->window_handle();
    NSRect rect = [[[view window] contentView] frame];
    *out_rect = gfx::Rect(NSRectToCGRect(rect));
  }
}

void TestWebViewDelegate::RunModal(WebWidget* webwidget) {
  NOTIMPLEMENTED();
}

// Private methods -----------------------------------------------------------

void TestWebViewDelegate::SetPageTitle(const std::wstring& title) {
  [[shell_->webViewHost()->window_handle() window]
      setTitle:[NSString stringWithUTF8String:WideToUTF8(title).c_str()]];
}

void TestWebViewDelegate::SetAddressBarURL(const GURL& url) {
  const char* frameURL = url.spec().c_str();
  NSString *address = [NSString stringWithUTF8String:frameURL];
  [shell_->editWnd() setStringValue:address];
}
