// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_webview_delegate.h"

#import <Cocoa/Cocoa.h>
#include "base/sys_string_conversions.h"
#include "base/string_util.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"
#include "webkit/tools/test_shell/test_shell.h"

// MenuDelegate ----------------------------------------------------------------
// A class for determining whether an item was selected from an HTML select
// control, or if the menu was dismissed without making a selection. If a menu
// item is selected, MenuDelegate is informed and sets a flag which can be
// queried after the menu has finished running.

@interface MenuDelegate : NSObject {
 @private
  NSMenu* menu_;  // Non-owning
  BOOL menuItemWasChosen_;
}
- (id)initWithItems:(const std::vector<MenuItem>&)items forMenu:(NSMenu*)menu;
- (void)addItem:(const MenuItem&)item;
- (BOOL)menuItemWasChosen;
- (void)menuItemSelected:(id)sender;
@end

@implementation MenuDelegate

- (id)initWithItems:(const std::vector<MenuItem>&)items forMenu:(NSMenu*)menu {
  if ((self = [super init])) {
    menu_ = menu;
    menuItemWasChosen_ = NO;
    for (int i = 0; i < static_cast<int>(items.size()); ++i)
      [self addItem:items[i]];
  }
  return self;
}

- (void)addItem:(const MenuItem&)item {
  if (item.type == MenuItem::SEPARATOR) {
    [menu_ addItem:[NSMenuItem separatorItem]];
    return;
  }

  NSString* title = base::SysUTF16ToNSString(item.label);
  NSMenuItem* menu_item = [menu_ addItemWithTitle:title
                                           action:@selector(menuItemSelected:)
                                    keyEquivalent:@""];
  [menu_item setEnabled:(item.enabled && item.type != MenuItem::GROUP)];
  [menu_item setTarget:self];
}

// Reflects the result of the user's interaction with the popup menu. If NO, the
// menu was dismissed without the user choosing an item, which can happen if the
// user clicked outside the menu region or hit the escape key. If YES, the user
// selected an item from the menu.
- (BOOL)menuItemWasChosen {
  return menuItemWasChosen_;
}

- (void)menuItemSelected:(id)sender {
  menuItemWasChosen_ = YES;
}

@end  // MenuDelegate


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
void TestWebViewDelegate::ShowWithItems(
    WebWidget* webview,
    const gfx::Rect& bounds,
    int item_height,
    int selected_index,
    const std::vector<MenuItem>& items) {
  // Populate the menu.
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
  [menu setAutoenablesItems:NO];
  MenuDelegate* menu_delegate =
      [[[MenuDelegate alloc] initWithItems:items forMenu:menu] autorelease];

  // Set up the button cell, converting to NSView coordinates. The menu is
  // positioned such that the currently selected menu item appears over the
  // popup button, which is the expected Mac popup menu behavior.
  NSPopUpButtonCell* button = [[NSPopUpButtonCell alloc] initTextCell:@""
                                                            pullsDown:NO];
  [button autorelease];
  [button setMenu:menu];
  [button selectItemAtIndex:selected_index];
  NSView* web_view = shell_->webViewWnd();
  NSRect view_rect = [web_view bounds];
  int y_offset = bounds.y() + bounds.height();
  NSRect position = NSMakeRect(bounds.x(), view_rect.size.height - y_offset,
                               bounds.width(), bounds.height());

  // Display the menu, and set a flag to determine if something was chosen. If
  // nothing was chosen (i.e., the user dismissed the popup by the "ESC" key or
  // clicking outside popup's region), send a dismiss message to WebKit.
  [button performClickWithFrame:position inView:shell_->webViewWnd()];

  // Get the selected item and forward to WebKit. WebKit expects an input event
  // (mouse down, keyboard activity) for this, so we calculate the proper
  // position based on the selected index and provided bounds.
  WebWidgetHost* popup = shell_->popupHost();
  NSEvent* event = nil;
  double event_time = (double)(AbsoluteToDuration(UpTime())) / 1000.0;
  int window_num = [shell_->mainWnd() windowNumber];

  if ([menu_delegate menuItemWasChosen]) {
    // Construct a mouse up event to simulate the selection of an appropriate
    // menu item.
    NSPoint click_pos;
    click_pos.x = position.size.width / 2;

    // This is going to be hard to calculate since the button is painted by
    // WebKit, the menu by Cocoa, and we have to translate the selected_item
    // index to a coordinate that WebKit's PopupMenu expects which uses a
    // different font *and* expects to draw the menu below the button like we do
    // on Windows.
    // The WebKit popup menu thinks it will draw just below the button, so
    // create the click at the offset based on the selected item's index and
    // account for the different coordinate system used by NSView.
    int item_offset = [button indexOfSelectedItem] * item_height +
                      item_height / 2;
    click_pos.y = view_rect.size.height - item_offset;
    event = [NSEvent mouseEventWithType:NSLeftMouseUp
                               location:click_pos
                          modifierFlags:0
                              timestamp:event_time
                           windowNumber:window_num
                                context:nil
                            eventNumber:0
                             clickCount:1
                               pressure:1.0];
    popup->MouseEvent(event);
  } else {
    // Fake an ESC key event (keyCode = 0x1B, from webinputevent_mac.mm) and
    // forward that to WebKit.
    NSPoint key_pos;
    key_pos.x = 0;
    key_pos.y = 0;
    event = [NSEvent keyEventWithType:NSKeyUp
                             location:key_pos
                        modifierFlags:0
                            timestamp:event_time
                         windowNumber:window_num
                              context:nil
                           characters:@""
          charactersIgnoringModifiers:@""
                            isARepeat:NO
                              keyCode:0x1B];
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
                                    const WebCursor& cursor) {
  NSCursor* ns_cursor = cursor.GetCursor();
  [ns_cursor set];
}

void TestWebViewDelegate::GetWindowRect(WebWidget* webwidget,
                                        gfx::Rect* out_rect) {
  DCHECK(out_rect);
  if (WebWidgetHost* host = GetHostForWidget(webwidget)) {
    NSView *view = host->view_handle();
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
    NSView *view = host->view_handle();
    NSRect rect = [[[view window] contentView] frame];
    *out_rect = gfx::Rect(NSRectToCGRect(rect));
  }
}

@interface NSWindow(OSInternals)
- (NSRect)_growBoxRect;
@end

void TestWebViewDelegate::GetRootWindowResizerRect(WebWidget* webwidget,
                                                   gfx::Rect* out_rect) {
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
