// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_webview_delegate.h"

#import <Cocoa/Cocoa.h>
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "webkit/api/public/WebCursorInfo.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"
#include "webkit/glue/webmenurunner_mac.h"
#include "webkit/tools/test_shell/test_shell.h"

using WebKit::WebCursorInfo;
using WebKit::WebRect;

// WebViewDelegate -----------------------------------------------------------

TestWebViewDelegate::~TestWebViewDelegate() {
}

WebPluginDelegate* TestWebViewDelegate::CreatePluginDelegate(
    WebView* webview,
    const GURL& url,
    const std::string& mime_type,
    const std::string& clsid,
    std::string* actual_mime_type) {
  WebWidgetHost *host = GetHostForWidget(webview);
  if (!host)
    return NULL;
  gfx::NativeView view = host->view_handle();

  bool allow_wildcard = true;
  WebPluginInfo info;
  if (!NPAPI::PluginList::Singleton()->GetPluginInfo(url, mime_type, clsid,
                                                     allow_wildcard, &info,
                                                     actual_mime_type))
    return NULL;

  if (actual_mime_type && !actual_mime_type->empty())
    return WebPluginDelegateImpl::Create(info.path, *actual_mime_type, view);
  else
    return WebPluginDelegateImpl::Create(info.path, mime_type, view);
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

// Display a HTML select menu.
void TestWebViewDelegate::ShowAsPopupWithItems(
    WebWidget* webview,
    const WebRect& bounds,
    int item_height,
    int selected_index,
    const std::vector<WebMenuItem>& items) {
  // Set up the menu position.
  NSView* web_view = shell_->webViewWnd();
  NSRect view_rect = [web_view bounds];
  int y_offset = bounds.y + bounds.height;
  NSRect position = NSMakeRect(bounds.x, view_rect.size.height - y_offset,
                               bounds.width, bounds.height);

  // Display the menu.
  scoped_nsobject<WebMenuRunner> menu_runner;
  menu_runner.reset([[WebMenuRunner alloc] initWithItems:items]);

  [menu_runner runMenuInView:shell_->webViewWnd()
                  withBounds:position
                initialIndex:selected_index];

  // Get the selected item and forward to WebKit. WebKit expects an input event
  // (mouse down, keyboard activity) for this, so we calculate the proper
  // position based on the selected index and provided bounds.
  WebWidgetHost* popup = shell_->popupHost();
  int window_num = [shell_->mainWnd() windowNumber];
  NSEvent* event =
      webkit_glue::EventWithMenuAction([menu_runner menuItemWasChosen],
                                       window_num, item_height,
                                       [menu_runner indexOfSelectedItem],
                                       position, view_rect);

  if ([menu_runner menuItemWasChosen]) {
    // Construct a mouse up event to simulate the selection of an appropriate
    // menu item.
    popup->MouseEvent(event);
  } else {
    // Fake an ESC key event (keyCode = 0x1B, from webinputevent_mac.mm) and
    // forward that to WebKit.
    popup->KeyEvent(event);
  }
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
                                    const WebCursorInfo& cursor_info) {
  NSCursor* ns_cursor = WebCursor(cursor_info).GetCursor();
  [ns_cursor set];
}

void TestWebViewDelegate::GetWindowRect(WebWidget* webwidget,
                                        WebRect* out_rect) {
  DCHECK(out_rect);
  if (WebWidgetHost* host = GetHostForWidget(webwidget)) {
    NSView *view = host->view_handle();
    NSRect rect = [view frame];
    *out_rect = gfx::Rect(NSRectToCGRect(rect));
  }
}

void TestWebViewDelegate::SetWindowRect(WebWidget* webwidget,
                                        const WebRect& rect) {
  // TODO: Mac window movement
  if (webwidget == shell_->webView()) {
    // ignored
  } else if (webwidget == shell_->popup()) {
    // MoveWindow(shell_->popupWnd(),
    //            rect.x(), rect.y(), rect.width(), rect.height(), FALSE);
  }
}

void TestWebViewDelegate::GetRootWindowRect(WebWidget* webwidget,
                                            WebRect* out_rect) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget)) {
    NSView *view = host->view_handle();
    NSRect rect = [[[view window] contentView] frame];
    *out_rect = gfx::Rect(NSRectToCGRect(rect));
  }
}

@interface NSWindow(OSInternals)
- (NSRect)_growBoxRect;
@end

void TestWebViewDelegate::GetRootWindowResizerRect(WebWidget* webwidget,
                                                   WebRect* out_rect) {
  NSRect resize_rect = NSMakeRect(0, 0, 0, 0);
  WebWidgetHost* host = GetHostForWidget(webwidget);
  // To match the WebKit screen shots, we need the resize area to overlap
  // the scroll arrows, so in layout test mode, we don't return a real rect.
  if (!(shell_->layout_test_mode()) && host) {
    NSView *view = host->view_handle();
    NSWindow* window = [view window];
    resize_rect = [window _growBoxRect];
    // The scrollbar assumes that the resizer goes all the way down to the
    // bottom corner, so we ignore any y offset to the rect itself and use the
    // entire bottom corner.
    resize_rect.origin.y = 0;
    // Convert to view coordinates from window coordinates.
    resize_rect = [view convertRect:resize_rect fromView:nil];
    // Flip the rect in view coordinates
    resize_rect.origin.y =
        [view frame].size.height - resize_rect.origin.y -
        resize_rect.size.height;
  }
  *out_rect = gfx::Rect(NSRectToCGRect(resize_rect));
}

void TestWebViewDelegate::DidMove(WebWidget* webwidget,
                                  const WebPluginGeometry& move) {
  // TODO(port): add me once plugins work.
}

void TestWebViewDelegate::RunModal(WebWidget* webwidget) {
  NOTIMPLEMENTED();
}

void TestWebViewDelegate::UpdateSelectionClipboard(bool is_empty_selection) {
  // No selection clipboard on mac, do nothing.
}

// Private methods -----------------------------------------------------------

void TestWebViewDelegate::SetPageTitle(const std::wstring& title) {
  [[shell_->webViewHost()->view_handle() window]
      setTitle:[NSString stringWithUTF8String:WideToUTF8(title).c_str()]];
}

void TestWebViewDelegate::SetAddressBarURL(const GURL& url) {
  const char* frameURL = url.spec().c_str();
  NSString *address = [NSString stringWithUTF8String:frameURL];
  [shell_->editWnd() setStringValue:address];
}
