// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"

#include "base/histogram.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_trial.h"
#import "chrome/browser/cocoa/rwhvm_editcommand_helper.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/api/public/mac/WebInputEventFactory.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/glue/webmenurunner_mac.h"

using WebKit::WebInputEventFactory;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

@interface RenderWidgetHostViewCocoa (Private)
- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r;
@end

namespace {

// Maximum number of characters we allow in a tooltip.
const size_t kMaxTooltipLength = 1024;

}

// RenderWidgetHostView --------------------------------------------------------

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewMac(widget);
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, public:

RenderWidgetHostViewMac::RenderWidgetHostViewMac(RenderWidgetHost* widget)
    : render_widget_host_(widget),
      about_to_validate_and_paint_(false),
      is_loading_(false),
      is_hidden_(false),
      shutdown_factory_(this),
      parent_view_(NULL) {
  cocoa_view_ = [[[RenderWidgetHostViewCocoa alloc]
                  initWithRenderWidgetHostViewMac:this] autorelease];
  render_widget_host_->set_view(this);
}

RenderWidgetHostViewMac::~RenderWidgetHostViewMac() {
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, RenderWidgetHostView implementation:

void RenderWidgetHostViewMac::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  [parent_host_view->GetNativeView() addSubview:cocoa_view_];
  [cocoa_view_ setCloseOnDeactivate:YES];
  [cocoa_view_ setCanBeKeyView:activatable_ ? YES : NO];

  // TODO(avi):Why the hell are these screen coordinates? The Windows code calls
  // ::MoveWindow() which indicates they should be local, but when running it I
  // get global ones instead!

  NSPoint global_origin = NSPointFromCGPoint(pos.origin().ToCGPoint());
  global_origin.y = [[[cocoa_view_ window] screen] frame].size.height -
      pos.height() - global_origin.y;
  NSPoint window_origin =
      [[cocoa_view_ window] convertScreenToBase:global_origin];
  NSPoint view_origin =
      [cocoa_view_ convertPoint:window_origin fromView:nil];
  NSRect initial_frame = NSMakeRect(view_origin.x,
                                    view_origin.y,
                                    pos.width(),
                                    pos.height());
  [cocoa_view_ setFrame:initial_frame];
}

RenderWidgetHost* RenderWidgetHostViewMac::GetRenderWidgetHost() const {
  return render_widget_host_;
}

void RenderWidgetHostViewMac::DidBecomeSelected() {
  if (!is_hidden_)
    return;

  is_hidden_ = false;
  render_widget_host_->WasRestored();
}

void RenderWidgetHostViewMac::WasHidden() {
  if (is_hidden_)
    return;

  // If we receive any more paint messages while we are hidden, we want to
  // ignore them so we don't re-allocate the backing store.  We will paint
  // everything again when we become selected again.
  is_hidden_ = true;

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  render_widget_host_->WasHidden();
}

void RenderWidgetHostViewMac::SetSize(const gfx::Size& size) {
  if (is_hidden_)
    return;

  // TODO(avi): the TabContents object uses this method to size the newly
  // created widget to the correct size. At the time of this call, we're not yet
  // in the view hierarchy so |size| ends up being 0x0. However, this works for
  // us because we're using the Cocoa view struture and resizer flags to fix
  // things up as soon as the view gets added to the hierarchy. Figure out if we
  // want to keep this flow or switch back to the flow Windows uses which sets
  // the size upon creation. http://crbug.com/8285.
}

gfx::NativeView RenderWidgetHostViewMac::GetNativeView() {
  return native_view();
}

void RenderWidgetHostViewMac::MovePluginWindows(
    const std::vector<WebPluginGeometry>& plugin_window_moves) {
  // All plugin stuff is TBD. TODO(avi,awalker): fill in
  // http://crbug.com/8192
}

void RenderWidgetHostViewMac::Focus() {
  [[cocoa_view_ window] makeFirstResponder:cocoa_view_];
}

void RenderWidgetHostViewMac::Blur() {
  [[cocoa_view_ window] makeFirstResponder:nil];
}

bool RenderWidgetHostViewMac::HasFocus() {
  return [[cocoa_view_ window] firstResponder] == cocoa_view_;
}

void RenderWidgetHostViewMac::Show() {
  [cocoa_view_ setHidden:NO];

  DidBecomeSelected();
}

void RenderWidgetHostViewMac::Hide() {
  [cocoa_view_ setHidden:YES];

  WasHidden();
}

gfx::Rect RenderWidgetHostViewMac::GetViewBounds() const {
  return [cocoa_view_ NSRectToRect:[cocoa_view_ bounds]];
}

void RenderWidgetHostViewMac::UpdateCursor(const WebCursor& cursor) {
  current_cursor_ = cursor;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewMac::UpdateCursorIfOverSelf() {
  // Do something special (as Win Chromium does) for arrow cursor while loading
  // a page? TODO(avi): decide
  // Can we synchronize to the event stream? Switch to -[NSWindow
  // mouseLocationOutsideOfEventStream] if we cannot. TODO(avi): test and see
  NSEvent* event = [[cocoa_view_ window] currentEvent];
  if ([event window] != [cocoa_view_ window])
    return;

  NSPoint event_location = [event locationInWindow];
  NSPoint local_point = [cocoa_view_ convertPoint:event_location fromView:nil];

  if (!NSPointInRect(local_point, [cocoa_view_ bounds]))
    return;

  NSCursor* ns_cursor = current_cursor_.GetCursor();
  [ns_cursor set];
}

void RenderWidgetHostViewMac::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewMac::IMEUpdateStatus(int control,
                                              const gfx::Rect& caret_rect) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewMac::DidPaintRect(const gfx::Rect& rect) {
  if (is_hidden_)
    return;

  NSRect ns_rect = [cocoa_view_ RectToNSRect:rect];

  if (about_to_validate_and_paint_) {
    // As much as we'd like to use -setNeedsDisplayInRect: here, we can't. We're
    // in the middle of executing a -drawRect:, and as soon as it returns Cocoa
    // will clear its record of what needs display. If we want to handle the
    // recursive drawing, we need to do it ourselves.
    invalid_rect_ = NSUnionRect(invalid_rect_, ns_rect);
  } else {
    [cocoa_view_ setNeedsDisplayInRect:ns_rect];
    [cocoa_view_ displayIfNeeded];
  }
}

void RenderWidgetHostViewMac::DidScrollRect(
    const gfx::Rect& rect, int dx, int dy) {
  if (is_hidden_)
    return;

  // Before asking the cocoa view to scroll, shorten the rect's bounds
  // by the amount we are scrolling.  This will prevent us from moving
  // data beyond the bounds of the original rect, which in turn
  // prevents us from accidentally drawing over other parts of the
  // page (scrolbars, other frames, etc).
  gfx::Rect scroll_rect = rect;
  scroll_rect.Inset(dx < 0 ? -dx : 0,
                    dy < 0 ? -dy : 0,
                    dx > 0 ? dx : 0,
                    dy > 0 ? dy : 0);
  [cocoa_view_ scrollRect:[cocoa_view_ RectToNSRect:scroll_rect]
                       by:NSMakeSize(dx, -dy)];

  gfx::Rect new_rect = rect;
  new_rect.Offset(dx, dy);
  gfx::Rect dirty_rect = rect.Subtract(new_rect);
  [cocoa_view_ setNeedsDisplayInRect:[cocoa_view_ RectToNSRect:dirty_rect]];
}

void RenderWidgetHostViewMac::RenderViewGone() {
  // TODO(darin): keep this around, and draw sad-tab into it.
  UpdateCursorIfOverSelf();
  Destroy();
}

void RenderWidgetHostViewMac::Destroy() {
  // We've been told to destroy.
  [cocoa_view_ retain];
  [cocoa_view_ removeFromSuperview];
  [cocoa_view_ autorelease];

  // We get this call just before |render_widget_host_| deletes
  // itself.  But we are owned by |cocoa_view_|, which may be retained
  // by some other code.  Examples are TabContentsViewMac's
  // |latent_focus_view_| and TabWindowController's
  // |cachedContentView_|.
  render_widget_host_ = NULL;
}

// Called from the renderer to tell us what the tooltip text should be. It
// calls us frequently so we need to cache the value to prevent doing a lot
// of repeat work.
void RenderWidgetHostViewMac::SetTooltipText(const std::wstring& tooltip_text) {
  if (tooltip_text != tooltip_text_ && [[cocoa_view_ window] isKeyWindow]) {
    tooltip_text_ = tooltip_text;

    // Clamp the tooltip length to kMaxTooltipLength. It's a DOS issue on
    // Windows; we're just trying to be polite. Don't persist the trimmed
    // string, as then the comparison above will always fail and we'll try to
    // set it again every single time the mouse moves.
    std::wstring display_text = tooltip_text_;
    if (tooltip_text_.length() > kMaxTooltipLength)
      display_text = tooltip_text_.substr(0, kMaxTooltipLength);

    NSString* tooltip_nsstring = base::SysWideToNSString(display_text);
    [cocoa_view_ setToolTipAtMousePoint:tooltip_nsstring];
  }
}

BackingStore* RenderWidgetHostViewMac::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStore(render_widget_host_, size);
}

// Display a popup menu for WebKit using Cocoa widgets.
void RenderWidgetHostViewMac::ShowPopupWithItems(
    gfx::Rect bounds,
    int item_height,
    int selected_item,
    const std::vector<WebMenuItem>& items) {
  NSRect view_rect = [cocoa_view_ bounds];
  NSRect parent_rect = [parent_view_ bounds];
  int y_offset = bounds.y() + bounds.height();
  NSRect position = NSMakeRect(bounds.x(), parent_rect.size.height - y_offset,
                               bounds.width(), bounds.height());

  // Display the menu.
  scoped_nsobject<WebMenuRunner> menu_runner;
  menu_runner.reset([[WebMenuRunner alloc] initWithItems:items]);

  [menu_runner runMenuInView:parent_view_
                  withBounds:position
                initialIndex:selected_item];

  int window_num = [[parent_view_ window] windowNumber];
  NSEvent* event =
      webkit_glue::EventWithMenuAction([menu_runner menuItemWasChosen],
                                       window_num, item_height,
                                       [menu_runner indexOfSelectedItem],
                                       position, view_rect);

  if ([menu_runner menuItemWasChosen]) {
    // Simulate a menu selection event.
    const WebMouseEvent& mouse_event =
        WebInputEventFactory::mouseEvent(event, cocoa_view_);
    render_widget_host_->ForwardMouseEvent(mouse_event);
  } else {
    // Simulate a menu dismiss event.
    NativeWebKeyboardEvent keyboard_event(event);
    render_widget_host_->ForwardKeyboardEvent(keyboard_event);
  }
}

void RenderWidgetHostViewMac::KillSelf() {
  if (shutdown_factory_.empty()) {
    [cocoa_view_ setHidden:YES];
    MessageLoop::current()->PostTask(FROM_HERE,
        shutdown_factory_.NewRunnableMethod(
            &RenderWidgetHostViewMac::ShutdownHost));
  }
}

void RenderWidgetHostViewMac::ShutdownHost() {
  shutdown_factory_.RevokeAll();
  render_widget_host_->Shutdown();
  // Do not touch any members at this point, |this| has been deleted.
}

namespace {

// Adjusts an NSRect in screen coordinates to have an origin in the upper left,
// and stuffs it into a gfx::Rect. This is likely incorrect for a multiple-
// monitor setup.
gfx::Rect NSRectToRect(const NSRect rect, NSScreen* screen) {
  gfx::Rect new_rect(NSRectToCGRect(rect));
  new_rect.set_y([screen frame].size.height - new_rect.y() - new_rect.height());
  return new_rect;
}

}  // namespace

gfx::Rect RenderWidgetHostViewMac::GetWindowRect() {
  // TODO(shess): In case of !window, the view has been removed from
  // the view hierarchy because the tab isn't main.  Could retrieve
  // the information from the main tab for our window.
  if (!cocoa_view_ || ![cocoa_view_ window]) {
    return gfx::Rect();
  }

  NSRect bounds = [cocoa_view_ bounds];
  bounds = [cocoa_view_ convertRect:bounds toView:nil];
  bounds.origin = [[cocoa_view_ window] convertBaseToScreen:bounds.origin];
  return NSRectToRect(bounds, [[cocoa_view_ window] screen]);
}

gfx::Rect RenderWidgetHostViewMac::GetRootWindowRect() {
  // TODO(shess): In case of !window, the view has been removed from
  // the view hierarchy because the tab isn't main.  Could retrieve
  // the information from the main tab for our window.
  if (!cocoa_view_ || ![cocoa_view_ window]) {
    return gfx::Rect();
  }

  NSRect bounds = [[cocoa_view_ window] frame];
  return NSRectToRect(bounds, [[cocoa_view_ window] screen]);
}


// RenderWidgetHostViewCocoa ---------------------------------------------------

@implementation RenderWidgetHostViewCocoa

// Tons of stuff goes here, where we grab events going on in Cocoaland and send
// them into the C++ system. TODO(avi): all that jazz

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r {
  self = [super initWithFrame:NSZeroRect];
  if (self != nil) {
    editCommand_helper_.reset(new RWHVMEditCommandHelper);
    editCommand_helper_->AddEditingSelectorsToClass([self class]);

    renderWidgetHostView_ = r;
    canBeKeyView_ = YES;
    closeOnDeactivate_ = NO;
  }
  return self;
}

- (void)dealloc {
  delete renderWidgetHostView_;
  [toolTip_ release];

  [super dealloc];
}

- (void)setCanBeKeyView:(BOOL)can {
  canBeKeyView_ = can;
}

- (void)setCloseOnDeactivate:(BOOL)b {
  closeOnDeactivate_ = b;
}

- (void)mouseEvent:(NSEvent *)theEvent {
  const WebMouseEvent& event =
      WebInputEventFactory::mouseEvent(theEvent, self);
  if (renderWidgetHostView_->render_widget_host_)
    renderWidgetHostView_->render_widget_host_->ForwardMouseEvent(event);
}

- (void)keyEvent:(NSEvent *)theEvent {
  // TODO(avi): Possibly kill self? See RenderWidgetHostViewWin::OnKeyEvent and
  // http://b/issue?id=1192881 .

  NativeWebKeyboardEvent event(theEvent);
  if (renderWidgetHostView_->render_widget_host_)
    renderWidgetHostView_->render_widget_host_->ForwardKeyboardEvent(event);
}

- (void)scrollWheel:(NSEvent *)theEvent {
  const WebMouseWheelEvent& event =
      WebInputEventFactory::mouseWheelEvent(theEvent, self);
  if (renderWidgetHostView_->render_widget_host_)
    renderWidgetHostView_->render_widget_host_->ForwardWheelEvent(event);
}

- (void)setFrame:(NSRect)frameRect {
  [super setFrame:frameRect];
  if (renderWidgetHostView_->render_widget_host_)
    renderWidgetHostView_->render_widget_host_->WasResized();
}

- (void)drawRect:(NSRect)dirtyRect {
  if (!renderWidgetHostView_->render_widget_host_) {
    // TODO(shess): Consider using something more noticable?
    [[NSColor whiteColor] set];
    NSRectFill(dirtyRect);
    return;
  }

  DCHECK(renderWidgetHostView_->render_widget_host_->process()->HasConnection());
  DCHECK(!renderWidgetHostView_->about_to_validate_and_paint_);

  renderWidgetHostView_->invalid_rect_ = dirtyRect;
  renderWidgetHostView_->about_to_validate_and_paint_ = true;
  BackingStore* backing_store =
      renderWidgetHostView_->render_widget_host_->GetBackingStore(true);
  skia::PlatformCanvas* canvas = backing_store->canvas();
  renderWidgetHostView_->about_to_validate_and_paint_ = false;
  dirtyRect = renderWidgetHostView_->invalid_rect_;

  if (backing_store) {
    gfx::Rect damaged_rect([self NSRectToRect:dirtyRect]);

    gfx::Rect bitmap_rect(0, 0,
                          backing_store->size().width(),
                          backing_store->size().height());

    gfx::Rect paint_rect = bitmap_rect.Intersect(damaged_rect);
    if (!paint_rect.IsEmpty()) {
      CGContextRef context = static_cast<CGContextRef>(
          [[NSGraphicsContext currentContext] graphicsPort]);

      CGRect paint_rect_cg = paint_rect.ToCGRect();
      NSRect paint_rect_ns = [self RectToNSRect:paint_rect];
      canvas->getTopPlatformDevice().DrawToContext(
          context, paint_rect_ns.origin.x, paint_rect_ns.origin.y,
          &paint_rect_cg);
    }

    // Fill the remaining portion of the damaged_rect with white
    if (damaged_rect.right() > bitmap_rect.right()) {
      int x = std::max(bitmap_rect.right(), damaged_rect.x());
      int y = std::min(bitmap_rect.bottom(), damaged_rect.bottom());
      int width = damaged_rect.right() - x;
      int height = damaged_rect.y() - y;

      // Extra fun to get around the fact that gfx::Rects can't have
      // negative sizes.
      if (width < 0) {
        x += width;
        width = -width;
      }
      if (height < 0) {
        y += height;
        height = -height;
      }

      NSRect r = [self RectToNSRect:gfx::Rect(x, y, width, height)];
      [[NSColor whiteColor] set];
      NSRectFill(r);
    }
    if (damaged_rect.bottom() > bitmap_rect.bottom()) {
      int x = damaged_rect.x();
      int y = damaged_rect.bottom();
      int width = damaged_rect.right() - x;
      int height = std::max(bitmap_rect.bottom(), damaged_rect.y()) - y;

      // Extra fun to get around the fact that gfx::Rects can't have
      // negative sizes.
      if (width < 0) {
        x += width;
        width = -width;
      }
      if (height < 0) {
        y += height;
        height = -height;
      }

      NSRect r = [self RectToNSRect:gfx::Rect(x, y, width, height)];
      [[NSColor whiteColor] set];
      NSRectFill(r);
    }
    if (!renderWidgetHostView_->whiteout_start_time_.is_null()) {
      base::TimeDelta whiteout_duration = base::TimeTicks::Now() -
          renderWidgetHostView_->whiteout_start_time_;
      UMA_HISTOGRAM_TIMES("MPArch.RWHH_WhiteoutDuration", whiteout_duration);

      // Reset the start time to 0 so that we start recording again the next
      // time the backing store is NULL...
      renderWidgetHostView_->whiteout_start_time_ = base::TimeTicks();
    }
  } else {
    [[NSColor whiteColor] set];
    NSRectFill(dirtyRect);
    if (renderWidgetHostView_->whiteout_start_time_.is_null())
      renderWidgetHostView_->whiteout_start_time_ = base::TimeTicks::Now();
  }
}

- (BOOL)canBecomeKeyView {
  if (!renderWidgetHostView_->render_widget_host_)
    return NO;

  return canBeKeyView_;
}

- (BOOL)acceptsFirstResponder {
  if (!renderWidgetHostView_->render_widget_host_)
    return NO;

  return canBeKeyView_;
}

- (BOOL)becomeFirstResponder {
  if (!renderWidgetHostView_->render_widget_host_)
    return NO;

  renderWidgetHostView_->render_widget_host_->Focus();
  return YES;
}

- (BOOL)resignFirstResponder {
  if (!renderWidgetHostView_->render_widget_host_)
    return YES;

  if (closeOnDeactivate_)
    renderWidgetHostView_->KillSelf();

  renderWidgetHostView_->render_widget_host_->Blur();

  return YES;
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];

  return editCommand_helper_->IsMenuItemEnabled(action, self);
}

- (RenderWidgetHostViewMac*)renderWidgetHostViewMac {
  return renderWidgetHostView_;
}


// Below is the nasty tooltip stuff -- copied from WebKit's WebHTMLView.mm
// with minor modifications for code style and commenting.
//
//  The 'public' interface is -setToolTipAtMousePoint:. This differs from
// -setToolTip: in that the updated tooltip takes effect immediately,
//  without the user's having to move the mouse out of and back into the view.
//
// Unfortunately, doing this requires sending fake mouseEnter/Exit events to
// the view, which in turn requires overriding some internal tracking-rect
// methods (to keep track of its owner & userdata, which need to be filled out
// in the fake events.) --snej 7/6/09


/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *           (C) 2006, 2007 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Any non-zero value will do, but using something recognizable might help us
// debug some day.
static const NSTrackingRectTag kTrackingRectTag = 0xBADFACE;

// Override of a public NSView method, replacing the inherited functionality.
// See above for rationale.
- (NSTrackingRectTag)addTrackingRect:(NSRect)rect
                               owner:(id)owner
                            userData:(void *)data
                        assumeInside:(BOOL)assumeInside {
  DCHECK(trackingRectOwner_ == nil);
  trackingRectOwner_ = owner;
  trackingRectUserData_ = data;
  return kTrackingRectTag;
}

// Override of (apparently) a private NSView method(!) See above for rationale.
- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect
                                owner:(id)owner
                             userData:(void *)data
                         assumeInside:(BOOL)assumeInside
                       useTrackingNum:(int)tag {
  DCHECK(tag == 0 || tag == kTrackingRectTag);
  DCHECK(trackingRectOwner_ == nil);
  trackingRectOwner_ = owner;
  trackingRectUserData_ = data;
  return kTrackingRectTag;
}

// Override of (apparently) a private NSView method(!) See above for rationale.
- (void)_addTrackingRects:(NSRect *)rects
                    owner:(id)owner
             userDataList:(void **)userDataList
         assumeInsideList:(BOOL *)assumeInsideList
             trackingNums:(NSTrackingRectTag *)trackingNums
                    count:(int)count {
  DCHECK(count == 1);
  DCHECK(trackingNums[0] == 0 || trackingNums[0] == kTrackingRectTag);
  DCHECK(trackingRectOwner_ == nil);
  trackingRectOwner_ = owner;
  trackingRectUserData_ = userDataList[0];
  trackingNums[0] = kTrackingRectTag;
}

// Override of a public NSView method, replacing the inherited functionality.
// See above for rationale.
- (void)removeTrackingRect:(NSTrackingRectTag)tag {
  if (tag == 0)
    return;

  if (tag == kTrackingRectTag) {
    trackingRectOwner_ = nil;
    return;
  }

  if (tag == lastToolTipTag_) {
    [super removeTrackingRect:tag];
    lastToolTipTag_ = 0;
    return;
  }

  // If any other tracking rect is being removed, we don't know how it was
  // created and it's possible there's a leak involved (see Radar 3500217).
  NOTREACHED();
}

// Override of (apparently) a private NSView method(!)
- (void)_removeTrackingRects:(NSTrackingRectTag *)tags count:(int)count {
  for (int i = 0; i < count; ++i) {
    int tag = tags[i];
    if (tag == 0)
      continue;
    DCHECK(tag == kTrackingRectTag);
    trackingRectOwner_ = nil;
  }
}

// Sends a fake NSMouseExited event to the view for its current tracking rect.
- (void)_sendToolTipMouseExited {
  // Nothing matters except window, trackingNumber, and userData.
  int windowNumber = [[self window] windowNumber];
  NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseExited
                                              location:NSMakePoint(0, 0)
                                         modifierFlags:0
                                             timestamp:0
                                          windowNumber:windowNumber
                                               context:NULL
                                           eventNumber:0
                                        trackingNumber:kTrackingRectTag
                                              userData:trackingRectUserData_];
  [trackingRectOwner_ mouseExited:fakeEvent];
}

// Sends a fake NSMouseEntered event to the view for its current tracking rect.
- (void)_sendToolTipMouseEntered {
  // Nothing matters except window, trackingNumber, and userData.
  int windowNumber = [[self window] windowNumber];
  NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseEntered
                                              location:NSMakePoint(0, 0)
                                         modifierFlags:0
                                             timestamp:0
                                          windowNumber:windowNumber
                                               context:NULL
                                           eventNumber:0
                                        trackingNumber:kTrackingRectTag
                                              userData:trackingRectUserData_];
  [trackingRectOwner_ mouseEntered:fakeEvent];
}

// Sets the view's current tooltip, to be displayed at the current mouse
// location. (This does not make the tooltip appear -- as usual, it only
// appears after a delay.) Pass null to remove the tooltip.
- (void)setToolTipAtMousePoint:(NSString *)string {
  NSString *toolTip = [string length] == 0 ? nil : string;
  NSString *oldToolTip = toolTip_;
  if ((toolTip == nil || oldToolTip == nil) ? toolTip == oldToolTip
                                    : [toolTip isEqualToString:oldToolTip]) {
    return;
  }
  if (oldToolTip) {
    [self _sendToolTipMouseExited];
    [oldToolTip release];
  }
  toolTip_ = [toolTip copy];
  if (toolTip) {
    // See radar 3500217 for why we remove all tooltips
    // rather than just the single one we created.
    [self removeAllToolTips];
    NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
    lastToolTipTag_ = [self addToolTipRect:wideOpenRect
                                     owner:self
                                  userData:NULL];
    [self _sendToolTipMouseEntered];
  }
}

// NSView calls this to get the text when displaying the tooltip.
- (NSString *)view:(NSView *)view
  stringForToolTip:(NSToolTipTag)tag
             point:(NSPoint)point
          userData:(void *)data {
  return [[toolTip_ copy] autorelease];
}

@end

