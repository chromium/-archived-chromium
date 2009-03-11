// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_contents_view_mac.h"

#include "chrome/browser/browser.h" // TODO(beng): this dependency is awful.
#include "chrome/browser/cocoa/sad_tab_view.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#include "chrome/browser/tab_contents/web_contents.h"

#include "chrome/common/temp_scaffolding_stubs.h"

@interface WebContentsViewCocoa (Private)
- (id)initWithWebContentsViewMac:(WebContentsViewMac*)w;
- (void)processKeyboardEvent:(NSEvent*)event;
@end

// static
WebContentsView* WebContentsView::Create(WebContents* web_contents) {
  return new WebContentsViewMac(web_contents);
}

WebContentsViewMac::WebContentsViewMac(WebContents* web_contents)
    : web_contents_(web_contents) {
  registrar_.Add(this, NotificationType::WEB_CONTENTS_CONNECTED,
                 Source<WebContents>(web_contents));
  registrar_.Add(this, NotificationType::WEB_CONTENTS_DISCONNECTED,
                 Source<WebContents>(web_contents));
}

WebContentsViewMac::~WebContentsViewMac() {
}

WebContents* WebContentsViewMac::GetWebContents() {
  return web_contents_;
}

void WebContentsViewMac::CreateView() {
  WebContentsViewCocoa* view =
      [[WebContentsViewCocoa alloc] initWithWebContentsViewMac:this];
  // Under GC, ObjC and CF retains/releases are no longer equivalent. So we
  // change our ObjC retain to a CF retain so we can use a scoped_cftyperef.
  CFRetain(view);
  [view release];
  cocoa_view_.reset(view);
}

RenderWidgetHostView* WebContentsViewMac::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  DCHECK(!render_widget_host->view());
  RenderWidgetHostViewMac* view =
      new RenderWidgetHostViewMac(render_widget_host);

  // Fancy layout comes later; for now just make it our size and resize it
  // with us.
  NSView* view_view = view->native_view();
  [cocoa_view_.get() addSubview:view_view];
  [view_view setFrame:[cocoa_view_.get() bounds]];
  [view_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  return view;
}

gfx::NativeView WebContentsViewMac::GetNativeView() const {
  return cocoa_view_.get();
}

gfx::NativeView WebContentsViewMac::GetContentNativeView() const {
  if (!web_contents_->render_widget_host_view())
    return NULL;
  return web_contents_->render_widget_host_view()->GetPluginNativeView();
}

gfx::NativeWindow WebContentsViewMac::GetTopLevelNativeView() const {
  return [cocoa_view_.get() window];
}

void WebContentsViewMac::GetContainerBounds(gfx::Rect* out) const {
  *out = [cocoa_view_.get() NSRectToRect:[cocoa_view_.get() bounds]];
}

void WebContentsViewMac::StartDragging(const WebDropData& drop_data) {
  NOTIMPLEMENTED();
}

void WebContentsViewMac::OnContentsDestroy() {
  // TODO(avi):Close the find bar if any.
  if (find_bar_.get())
    find_bar_->Close();
}

void WebContentsViewMac::SetPageTitle(const std::wstring& title) {
  // Meaningless on the Mac; widgets don't have a "title" attribute
}

void WebContentsViewMac::Invalidate() {
  [cocoa_view_.get() setNeedsDisplay:YES];
}

void WebContentsViewMac::SizeContents(const gfx::Size& size) {
  // TODO(brettw) this is a hack and should be removed. See web_contents_view.h.
  NOTIMPLEMENTED();  // Leaving the hack unimplemented.
}

void WebContentsViewMac::OpenDeveloperTools() {
  NOTIMPLEMENTED();
}

void WebContentsViewMac::ForwardMessageToDevToolsClient(
    const IPC::Message& message) {
  NOTIMPLEMENTED();
}

void WebContentsViewMac::FindInPage(const Browser& browser,
                                    bool find_next, bool forward_direction) {
  if (!find_bar_.get()) {
    // We want the Chrome top-level (Frame) window.
    NSWindow* window =
        static_cast<NSWindow*>(browser.window()->GetNativeHandle());
    find_bar_.reset(new FindBarMac(this, window));
  } else {
    find_bar_->Show();
  }

  if (find_next && !find_bar_->find_string().empty())
    find_bar_->StartFinding(forward_direction);
}

void WebContentsViewMac::HideFindBar(bool end_session) {
  if (find_bar_.get()) {
    if (end_session)
      find_bar_->EndFindSession();
    else
      find_bar_->DidBecomeUnselected();
  }
}

bool WebContentsViewMac::GetFindBarWindowInfo(gfx::Point* position,
                                              bool* fully_visible) const {
  if (!find_bar_.get() ||
      [find_bar_->GetView() isHidden]) {
    *position = gfx::Point(0, 0);
    *fully_visible = false;
    return false;
  }

  NSRect frame = [find_bar_->GetView() frame];
  *position = gfx::Point(frame.origin.x, frame.origin.y);
  *fully_visible = find_bar_->IsVisible() && !find_bar_->IsAnimating();
  return true;
}

void WebContentsViewMac::UpdateDragCursor(bool is_drop_target) {
  NOTIMPLEMENTED();
}

void WebContentsViewMac::TakeFocus(bool reverse) {
  [cocoa_view_.get() becomeFirstResponder];
}

void WebContentsViewMac::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  [cocoa_view_.get() processKeyboardEvent:event.os_event];
}

void WebContentsViewMac::OnFindReply(int request_id,
                                     int number_of_matches,
                                     const gfx::Rect& selection_rect,
                                     int active_match_ordinal,
                                     bool final_update) {
  if (find_bar_.get()) {
    find_bar_->OnFindReply(request_id, number_of_matches, selection_rect,
                           active_match_ordinal, final_update);
  }
}

void WebContentsViewMac::ShowContextMenu(const ContextMenuParams& params) {
  NOTIMPLEMENTED();
}

WebContents* WebContentsViewMac::CreateNewWindowInternal(
    int route_id,
    base::WaitableEvent* modal_dialog_event) {
  // I don't understand what role this plays in the grand scheme of things. I'm
  // not going to fake it.
  NOTIMPLEMENTED();
  return NULL;
}

RenderWidgetHostView* WebContentsViewMac::CreateNewWidgetInternal(
    int route_id,
    bool activatable) {
  // I don't understand what role this plays in the grand scheme of things. I'm
  // not going to fake it.
  NOTIMPLEMENTED();
  return NULL;
}

void WebContentsViewMac::ShowCreatedWindowInternal(
    WebContents* new_web_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture) {
  // I don't understand what role this plays in the grand scheme of things. I'm
  // not going to fake it.
  NOTIMPLEMENTED();
}

void WebContentsViewMac::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view,
    const gfx::Rect& initial_pos) {
  // I don't understand what role this plays in the grand scheme of things. I'm
  // not going to fake it.
  NOTIMPLEMENTED();
}

void WebContentsViewMac::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::WEB_CONTENTS_CONNECTED: {
      if (sad_tab_.get()) {
        [sad_tab_.get() removeFromSuperview];
        sad_tab_.reset();
      }
      break;
    }
    case NotificationType::WEB_CONTENTS_DISCONNECTED: {
      SadTabView* view = [[SadTabView alloc] initWithFrame:NSZeroRect];
      CFRetain(view);
      [view release];
      sad_tab_.reset(view);

      // Set as the dominant child.
      [cocoa_view_.get() addSubview:view];
      [view setFrame:[cocoa_view_.get() bounds]];
      [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
      break;
    }
    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

@implementation WebContentsViewCocoa

- (id)initWithWebContentsViewMac:(WebContentsViewMac*)w {
  self = [super initWithFrame:NSZeroRect];
  if (self != nil) {
    webContentsView_ = w;
  }
  return self;
}

- (void)processKeyboardEvent:(NSEvent*)event {
  if ([event type] == NSKeyDown)
    [super keyDown:event];
  else if ([event type] == NSKeyUp)
    [super keyUp:event];
}

// In the Windows version, we always have cut/copy/paste enabled. This is sub-
// optimal, but we do it too. TODO(avi): Plumb the "can*" methods up from
// WebCore.

- (void)cut:(id)sender {
  webContentsView_->GetWebContents()->Cut();
}

- (void)copy:(id)sender {
  webContentsView_->GetWebContents()->Copy();
}

- (void)paste:(id)sender {
  webContentsView_->GetWebContents()->Paste();
}

// Tons of stuff goes here, where we grab events going on in Cocoaland and send
// them into the C++ system. TODO(avi): all that jazz

@end
