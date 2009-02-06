// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"

#include "base/histogram.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_trial.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "skia/ext/platform_canvas.h"

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
      is_loading_(false),
      is_hidden_(false) {
  cocoa_view_ = [[[RenderWidgetHostViewCocoa alloc]
                  initWithRenderWidgetHostViewMac:this] autorelease];
  render_widget_host_->set_view(this);
}

RenderWidgetHostViewMac::~RenderWidgetHostViewMac() {
}

gfx::NativeView RenderWidgetHostViewMac::GetNativeView() const {
  return cocoa_view_;
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, RenderWidgetHostView implementation:

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

  NSRect rect = [cocoa_view_ frame];
  rect.size = NSSizeFromCGSize(size.ToCGSize());
  [cocoa_view_ setFrame:rect];
}

gfx::NativeView RenderWidgetHostViewMac::GetPluginNativeView() {
  NOTIMPLEMENTED();
  return nil;
}

void RenderWidgetHostViewMac::MovePluginWindows(
    const std::vector<WebPluginGeometry>& plugin_window_moves) {
  // All plugin stuff is TBD. TODO(avi,awalker): fill in
  NOTIMPLEMENTED();
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
  return gfx::Rect(NSRectToCGRect([cocoa_view_ frame]));
}

void RenderWidgetHostViewMac::UpdateCursor(const WebCursor& cursor) {
//  current_cursor_ = cursor;  // temporarily commented for link issues
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewMac::UpdateCursorIfOverSelf() {
  // Do something special (as Windows does) for arrow cursor while loading a
  // page? TODO(avi): decide
  // TODO(avi): check to see if mouse pointer is within our bounds
  // Disabled so we don't have to link in glue... yet
//  NSCursor* ns_cursor = current_cursor_.GetCursor();
//  [ns_cursor set];
}

void RenderWidgetHostViewMac::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewMac::IMEUpdateStatus(int control,
                                              const gfx::Rect& caret_rect) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewMac::Redraw(const gfx::Rect& rect) {
  [cocoa_view_ setNeedsDisplayInRect:NSRectFromCGRect(rect.ToCGRect())];
}

void RenderWidgetHostViewMac::DidPaintRect(const gfx::Rect& rect) {
  if (is_hidden_)
    return;

  Redraw(rect);
}

void RenderWidgetHostViewMac::DidScrollRect(
    const gfx::Rect& rect, int dx, int dy) {
  if (is_hidden_)
    return;
  
  [cocoa_view_ scrollRect:NSRectFromCGRect(rect.ToCGRect())
                       by:NSMakeSize(dx, dy)];
}

void RenderWidgetHostViewMac::RendererGone() {
  // TODO(darin): keep this around, and draw sad-tab into it.
  UpdateCursorIfOverSelf();
  Destroy();
}

void RenderWidgetHostViewMac::Destroy() {
  // We've been told to destroy.
  [cocoa_view_ retain];
  [cocoa_view_ removeFromSuperview];
  [cocoa_view_ autorelease];
}

void RenderWidgetHostViewMac::SetTooltipText(const std::wstring& tooltip_text) {
  if (tooltip_text != tooltip_text_) {
    tooltip_text_ = tooltip_text;

    // Clamp the tooltip length to kMaxTooltipLength. It's a DOS issue on
    // Windows; we're just trying to be polite.
    if (tooltip_text_.length() > kMaxTooltipLength)
      tooltip_text_ = tooltip_text_.substr(0, kMaxTooltipLength);

    NSString* tooltip_nsstring = base::SysWideToNSString(tooltip_text_);
    [cocoa_view_ setToolTip:tooltip_nsstring];
  }
}

void RenderWidgetHostViewMac::ShutdownHost() {
  render_widget_host_->Shutdown();
  // Do not touch any members at this point, |this| has been deleted.
}

@implementation RenderWidgetHostViewCocoa

// Tons of stuff goes here, where we grab events going on in Cocoaland and send
// them into the C++ system. TODO(avi): all that jazz

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r {
  self = [super init];
  if (self != nil) {
    renderWidgetHostView_ = r;
  }
  return self;
}

- (void)dealloc {
  delete renderWidgetHostView_;
  
  [super dealloc];
}

- (void)drawRect:(NSRect)dirtyRect {
  DCHECK(renderWidgetHostView_->render_widget_host()->process()->channel());

  BackingStore* backing_store =
      renderWidgetHostView_->render_widget_host()->GetBackingStore();
  skia::PlatformCanvas* canvas = backing_store->canvas();

  if (backing_store) {
    gfx::Rect damaged_rect(NSRectToCGRect(dirtyRect));

    gfx::Rect bitmap_rect(
        0, 0, backing_store->size().width(), backing_store->size().height());

    gfx::Rect paint_rect = bitmap_rect.Intersect(damaged_rect);
    if (!paint_rect.IsEmpty()) {
      if ([self lockFocusIfCanDraw]) {
        CGContextRef context = static_cast<CGContextRef>(
            [[NSGraphicsContext currentContext] graphicsPort]);
        
        CGRect damaged_rect_cg = damaged_rect.ToCGRect();
        canvas->getTopPlatformDevice().DrawToContext(
            context, damaged_rect.x(), damaged_rect.y(),
            &damaged_rect_cg);
        
        [self unlockFocus];
      }
    }

    // Fill the remaining portion of the damaged_rect with white
    if (damaged_rect.right() > bitmap_rect.right()) {
      NSRect r;
      r.origin.x = std::max(bitmap_rect.right(), damaged_rect.x());
      r.origin.y = std::min(bitmap_rect.bottom(), damaged_rect.bottom());
      r.size.width = damaged_rect.right() - r.origin.x;
      r.size.height = damaged_rect.y() - r.origin.y;
      [[NSColor whiteColor] set];
      NSRectFill(r);
    }
    if (damaged_rect.bottom() > bitmap_rect.bottom()) {
      NSRect r;
      r.origin.x = damaged_rect.x();
      r.origin.y = damaged_rect.bottom();
      r.size.width = damaged_rect.right() - r.origin.x;
      r.size.height = std::max(bitmap_rect.bottom(), damaged_rect.y()) -
          r.origin.y;
      [[NSColor whiteColor] set];
      NSRectFill(r);
    }
    if (!renderWidgetHostView_->whiteout_start_time().is_null()) {
      base::TimeDelta whiteout_duration = base::TimeTicks::Now() -
          renderWidgetHostView_->whiteout_start_time();

      // If field trial is active, report results in special histogram.
      static scoped_refptr<FieldTrial> trial(
          FieldTrialList::Find(BrowserTrial::kMemoryModelFieldTrial));
      if (trial.get()) {
        if (trial->boolean_value())
          UMA_HISTOGRAM_TIMES(L"MPArch.RWHH_WhiteoutDuration_trial_high_memory",
                              whiteout_duration);
        else
          UMA_HISTOGRAM_TIMES(L"MPArch.RWHH_WhiteoutDuration_trial_med_memory",
                              whiteout_duration);
      } else {
        UMA_HISTOGRAM_TIMES(L"MPArch.RWHH_WhiteoutDuration", whiteout_duration);
      }

      // Reset the start time to 0 so that we start recording again the next
      // time the backing store is NULL...
      renderWidgetHostView_->whiteout_start_time() = base::TimeTicks();
    }
  } else {
    [[NSColor whiteColor] set];
    NSRectFill(dirtyRect);
    if (renderWidgetHostView_->whiteout_start_time().is_null())
      renderWidgetHostView_->whiteout_start_time() = base::TimeTicks::Now();
  }
}

- (BOOL)canBecomeKeyView {
  return YES;  // TODO(avi): be smarter
}

- (BOOL)acceptsFirstResponder {
  return YES;  // TODO(avi): be smarter
}

- (BOOL)becomeFirstResponder {
  renderWidgetHostView_->render_widget_host()->Focus();
  
  return YES;
}

- (BOOL)resignFirstResponder {
  renderWidgetHostView_->render_widget_host()->Blur();
  
  return YES;
}

@end
