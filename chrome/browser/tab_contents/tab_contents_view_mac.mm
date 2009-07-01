// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents_view_mac.h"

#include "chrome/browser/browser.h" // TODO(beng): this dependency is awful.
#include "chrome/browser/cocoa/sad_tab_view.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#include "chrome/browser/tab_contents/render_view_context_menu_mac.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"

#include "chrome/common/temp_scaffolding_stubs.h"

@interface TabContentsViewCocoa (Private)
- (id)initWithTabContentsViewMac:(TabContentsViewMac*)w;
- (void)processKeyboardEvent:(NSEvent*)event;
@end

// static
TabContentsView* TabContentsView::Create(TabContents* tab_contents) {
  return new TabContentsViewMac(tab_contents);
}

TabContentsViewMac::TabContentsViewMac(TabContents* tab_contents)
    : TabContentsView(tab_contents) {
  registrar_.Add(this, NotificationType::TAB_CONTENTS_CONNECTED,
                 Source<TabContents>(tab_contents));
}

TabContentsViewMac::~TabContentsViewMac() {
}

void TabContentsViewMac::CreateView() {
  TabContentsViewCocoa* view =
      [[TabContentsViewCocoa alloc] initWithTabContentsViewMac:this];
  cocoa_view_.reset(view);
}

RenderWidgetHostView* TabContentsViewMac::CreateViewForWidget(
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

gfx::NativeView TabContentsViewMac::GetNativeView() const {
  return cocoa_view_.get();
}

gfx::NativeView TabContentsViewMac::GetContentNativeView() const {
  if (!tab_contents()->render_widget_host_view())
    return NULL;
  return tab_contents()->render_widget_host_view()->GetNativeView();
}

gfx::NativeWindow TabContentsViewMac::GetTopLevelNativeWindow() const {
  return [cocoa_view_.get() window];
}

void TabContentsViewMac::GetContainerBounds(gfx::Rect* out) const {
  *out = [cocoa_view_.get() NSRectToRect:[cocoa_view_.get() bounds]];
}

void TabContentsViewMac::StartDragging(const WebDropData& drop_data) {
  NOTIMPLEMENTED();

  // Until we have d'n'd implemented, just immediately pretend we're
  // already done with the drag and drop so we don't get stuck
  // thinking we're in mid-drag.
  // TODO(port): remove me when the above NOTIMPLEMENTED is fixed.
  if (tab_contents()->render_view_host())
    tab_contents()->render_view_host()->DragSourceSystemDragEnded();
}

void TabContentsViewMac::OnContentsDestroy() {
}

void TabContentsViewMac::RenderViewCreated(RenderViewHost* host) {
  // We want updates whenever the intrinsic width of the webpage
  // changes. Put the RenderView into that mode.
  int routing_id = host->routing_id();
  host->Send(new ViewMsg_EnableIntrinsicWidthChangedMode(routing_id));
}

void TabContentsViewMac::SetPageTitle(const std::wstring& title) {
  // Meaningless on the Mac; widgets don't have a "title" attribute
}

void TabContentsViewMac::OnTabCrashed() {
  if (!sad_tab_.get()) {
    SadTabView* view = [[SadTabView alloc] initWithFrame:NSZeroRect];
    sad_tab_.reset(view);

    // Set as the dominant child.
    [cocoa_view_.get() addSubview:view];
    [view setFrame:[cocoa_view_.get() bounds]];
    [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  }
}

void TabContentsViewMac::SizeContents(const gfx::Size& size) {
  // TODO(brettw) this is a hack and should be removed. See tab_contents_view.h.
  NOTIMPLEMENTED();  // Leaving the hack unimplemented.
}

void TabContentsViewMac::Focus() {
  [[cocoa_view_.get() window] makeFirstResponder:GetContentNativeView()];
}

void TabContentsViewMac::SetInitialFocus() {
  if (tab_contents()->FocusLocationBarByDefault())
    tab_contents()->delegate()->SetFocusToLocationBar();
  else
    [[cocoa_view_.get() window] makeFirstResponder:GetContentNativeView()];
}

void TabContentsViewMac::StoreFocus() {
  NSResponder* current_focus = [[cocoa_view_.get() window] firstResponder];
  if ([current_focus isKindOfClass:[NSView class]]) {
    NSView* current_focus_view = (NSView*)current_focus;

    if ([current_focus_view isDescendantOf:cocoa_view_.get()]) {
      latent_focus_view_.reset([current_focus_view retain]);
      return;
    }
  }

  latent_focus_view_.reset();
}

void TabContentsViewMac::RestoreFocus() {
  // TODO(avi): Could we be restoring a view that's no longer in the key view
  // chain?
  if (latent_focus_view_.get()) {
    [[cocoa_view_.get() window] makeFirstResponder:latent_focus_view_.get()];
    latent_focus_view_.reset();
  } else {
    // TODO(shess): If location-bar gets focus by default, this will
    // select-all in the field.  If there was a specific selection in
    // the field when we navigated away from it, we should restore
    // that selection.
    SetInitialFocus();
  }
}

void TabContentsViewMac::UpdateDragCursor(bool is_drop_target) {
  NOTIMPLEMENTED();
}

void TabContentsViewMac::GotFocus() {
  NOTIMPLEMENTED();
}

// This is called when we the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void TabContentsViewMac::TakeFocus(bool reverse) {
  if (reverse) {
    [[cocoa_view_ window] selectPreviousKeyView:cocoa_view_.get()];
  } else {
    [[cocoa_view_ window] selectNextKeyView:cocoa_view_.get()];
  }
}

void TabContentsViewMac::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  [cocoa_view_.get() processKeyboardEvent:event.os_event];
}

void TabContentsViewMac::ShowContextMenu(const ContextMenuParams& params) {
  RenderViewContextMenuMac menu(tab_contents(),
                                params,
                                GetNativeView());
  menu.Init();
}

RenderWidgetHostView* TabContentsViewMac::CreateNewWidgetInternal(
    int route_id,
    bool activatable) {
  // A RenderWidgetHostViewMac has lifetime scoped to the view. We'll retain it
  // to allow it to survive the trip without being hosted.
  RenderWidgetHostView* widget_view =
      TabContentsView::CreateNewWidgetInternal(route_id, activatable);
  RenderWidgetHostViewMac* widget_view_mac =
      static_cast<RenderWidgetHostViewMac*>(widget_view);
  [widget_view_mac->native_view() retain];

  // |widget_view_mac| needs to know how to position itself in our view.
  widget_view_mac->set_parent_view(cocoa_view_);

  return widget_view;
}

void TabContentsViewMac::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view,
    const gfx::Rect& initial_pos) {
  TabContentsView::ShowCreatedWidgetInternal(widget_host_view, initial_pos);

  // A RenderWidgetHostViewMac has lifetime scoped to the view. Now that it's
  // properly embedded (or purposefully ignored) we can release the retain we
  // took in CreateNewWidgetInternal().
  RenderWidgetHostViewMac* widget_view_mac =
      static_cast<RenderWidgetHostViewMac*>(widget_host_view);
  [widget_view_mac->native_view() release];
}

void TabContentsViewMac::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::TAB_CONTENTS_CONNECTED: {
      if (sad_tab_.get()) {
        [sad_tab_.get() removeFromSuperview];
        sad_tab_.reset();
      }
      break;
    }
    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

@implementation TabContentsViewCocoa

- (id)initWithTabContentsViewMac:(TabContentsViewMac*)w {
  self = [super initWithFrame:NSZeroRect];
  if (self != nil) {
    TabContentsView_ = w;
  }
  return self;
}

- (void)processKeyboardEvent:(NSEvent*)event {
  if ([event type] == NSKeyDown)
    [super keyDown:event];
  else if ([event type] == NSKeyUp)
    [super keyUp:event];
}

- (void)mouseEvent:(NSEvent *)theEvent {
  if (TabContentsView_->tab_contents()->delegate()) {
    if ([theEvent type] == NSMouseMoved)
      TabContentsView_->tab_contents()->delegate()->
          ContentsMouseEvent(TabContentsView_->tab_contents(), true);
    if ([theEvent type] == NSMouseExited)
      TabContentsView_->tab_contents()->delegate()->
          ContentsMouseEvent(TabContentsView_->tab_contents(), false);
  }
}

- (BOOL)mouseDownCanMoveWindow {
  // This is needed to prevent mouseDowns from moving the window
  // around.  The default implementation returns YES only for opaque
  // views.  TabContentsViewCocoa does not draw itself in any way, but
  // its subviews do paint their entire frames.  Returning NO here
  // saves us the effort of overriding this method in every possible
  // subview.
  return NO;
}

// In the Windows version, we always have cut/copy/paste enabled. This is sub-
// optimal, but we do it too. TODO(avi): Plumb the "can*" methods up from
// WebCore.

- (void)cut:(id)sender {
  TabContentsView_->tab_contents()->Cut();
}

- (void)copy:(id)sender {
  TabContentsView_->tab_contents()->Copy();
}

- (void)paste:(id)sender {
  TabContentsView_->tab_contents()->Paste();
}

// Tons of stuff goes here, where we grab events going on in Cocoaland and send
// them into the C++ system. TODO(avi): all that jazz

@end
