// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/status_bubble_mac.h"

#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/common/gfx/text_elider.h"
#include "googleurl/src/gurl.h"
#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"

namespace {

const int kWindowHeight = 18;
// The width of the bubble in relation to the width of the parent window.
const float kWindowWidthPercent = 1.0f/3.0f;

// How close the mouse can get to the infobubble before it starts sliding
// off-screen.
const int kMousePadding = 20;

const int kTextPadding = 3;
const int kTextPositionX = 4;
const int kTextPositionY = 2;

const float kWindowFill = 0.8f;
const float kWindowEdge = 0.7f;

// The roundedness of the edges of our bubble.
const int kBubbleCornerRadius = 4.0f;

// How long each fade should last for.
const int kShowFadeDuration = 0.120f;
const int kHideFadeDuration = 0.200f;

}

// TODO(avi):
// - do display delay
// - figure out why the show/fade durations aren't working

enum BubbleStyle {
  STYLE_BOTTOM,    // Hanging off the bottom of the parent window
  STYLE_FLOATING,  // Between BOTTOM and STANDARD
  STYLE_STANDARD   // Nestled in the corner of the parent window
};

@interface StatusBubbleViewCocoa : NSView {
 @private
  NSString* content_;
  BubbleStyle style_;
}

- (void)setContent:(NSString*)content;
- (void)setStyle:(BubbleStyle)style;
- (NSFont*)font;
@end

StatusBubbleMac::StatusBubbleMac(NSWindow* parent)
    : parent_(parent),
      window_(nil),
      status_text_(nil),
      url_text_(nil) {
}

StatusBubbleMac::~StatusBubbleMac() {
  Hide();
}

void StatusBubbleMac::SetStatus(const std::wstring& status) {
  Create();

  NSString* status_ns = base::SysWideToNSString(status);

  SetStatus(status_ns, false);
}

void StatusBubbleMac::SetURL(const GURL& url, const std::wstring& languages) {
  Create();

  NSRect frame = [window_ frame];
  int text_width = static_cast<int>(frame.size.width -
                                    kTextPositionX -
                                    kTextPadding);
  NSFont* font = [[window_ contentView] font];
  ChromeFont font_chr =
      ChromeFont::CreateFont(base::SysNSStringToWide([font fontName]),
                             [font pointSize]);

  std::wstring status = gfx::ElideUrl(url, font_chr, text_width, languages);
  NSString* status_ns = base::SysWideToNSString(status);

  SetStatus(status_ns, true);
}

void StatusBubbleMac::SetStatus(NSString* status, bool is_url) {
  NSString** main;
  NSString** backup;

  if (is_url) {
    main = &url_text_;
    backup = &status_text_;
  } else {
    main = &status_text_;
    backup = &url_text_;
  }

  if ([status isEqualToString:*main])
    return;

  [*main release];
  *main = [status retain];
  if ([*main length] > 0) {
    [[window_ contentView] setContent:*main];
  } else if ([*backup length] > 0) {
    [[window_ contentView] setContent:*backup];
  } else {
    Hide();
  }

  FadeIn();
}

void StatusBubbleMac::Hide() {
  FadeOut();

  if (window_) {
    [parent_ removeChildWindow:window_];
    [window_ release];
    window_ = nil;
  }

  [status_text_ release];
  status_text_ = nil;
  [url_text_ release];
  url_text_ = nil;
}

void StatusBubbleMac::MouseMoved() {
  if (!window_)
    return;

  NSPoint cursor_location = [NSEvent mouseLocation];
  --cursor_location.y;  // docs say the y coord starts at 1 not 0; don't ask why

  // Get the normal position of the frame.
  NSRect window_frame = [window_ frame];
  window_frame.origin = [parent_ frame].origin;

  // Get the cursor position relative to the popup.
  cursor_location.x -= NSMaxX(window_frame);
  cursor_location.y -= NSMaxY(window_frame);

  // If the mouse is in a position where we think it would move the
  // status bubble, figure out where and how the bubble should be moved.
  if (cursor_location.y < kMousePadding &&
      cursor_location.x < kMousePadding) {
    int offset = kMousePadding - cursor_location.y;

    // Make the movement non-linear.
    offset = offset * offset / kMousePadding;

    // When the mouse is entering from the right, we want the offset to be
    // scaled by how horizontally far away the cursor is from the bubble.
    if (cursor_location.x > 0) {
      offset = offset * ((kMousePadding - cursor_location.x) / kMousePadding);
    }

    // Cap the offset and change the visual presentation of the bubble
    // depending on where it ends up (so that rounded corners square off
    // and mate to the edges of the tab content).
    if (offset >= NSHeight(window_frame)) {
      offset = NSHeight(window_frame);
      [[window_ contentView] setStyle:STYLE_BOTTOM];
    } else if (offset > 0) {
      [[window_ contentView] setStyle:STYLE_FLOATING];
    } else {
      [[window_ contentView] setStyle:STYLE_STANDARD];
    }

    offset_ = offset;
    window_frame.origin.y -= offset;
    [window_ setFrame:window_frame display:YES];
  } else {
    offset_ = 0;
    [[window_ contentView] setStyle:STYLE_STANDARD];
    [window_ setFrame:window_frame display:YES];
  }
}

void StatusBubbleMac::Create() {
  if (window_)
    return;

  NSRect rect = [parent_ frame];
  rect.size.height = kWindowHeight;
  rect.size.width = static_cast<int>(kWindowWidthPercent * rect.size.width);
  // TODO(avi):fix this for RTL
  window_ = [[NSWindow alloc] initWithContentRect:rect
                                        styleMask:NSBorderlessWindowMask
                                          backing:NSBackingStoreBuffered
                                            defer:YES];
  [window_ setMovableByWindowBackground:NO];
  [window_ setBackgroundColor:[NSColor clearColor]];
  [window_ setLevel:NSNormalWindowLevel];
  [window_ setOpaque:NO];
  [window_ setHasShadow:NO];

  StatusBubbleViewCocoa* view =
      [[[StatusBubbleViewCocoa alloc] initWithFrame:NSZeroRect] autorelease];
  [window_ setContentView:view];

  [parent_ addChildWindow:window_ ordered:NSWindowAbove];

  [window_ setAlphaValue:0.0f];
  [window_ orderFront:nil];

  offset_ = 0;
  [view setStyle:STYLE_STANDARD];
  MouseMoved();
}

void StatusBubbleMac::FadeIn() {
  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext] setDuration:kShowFadeDuration];
  [[window_ animator] setAlphaValue:1.0f];
  [NSAnimationContext endGrouping];
}

void StatusBubbleMac::FadeOut() {
  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext] setDuration:kHideFadeDuration];
  [[window_ animator] setAlphaValue:0.0f];
  [NSAnimationContext endGrouping];
}

@implementation StatusBubbleViewCocoa

- (void)dealloc {
  [content_ release];
  [super dealloc];
}

- (void)setContent:(NSString*)content {
  [content_ autorelease];
  content_ = [content copy];
  [self setNeedsDisplay:YES];
}

- (void)setStyle:(BubbleStyle)style {
  style_ = style;
  [self setNeedsDisplay:YES];
}

- (NSFont*)font {
  return [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
}

- (void)drawRect:(NSRect)rect {
  float tl_radius, tr_radius, bl_radius, br_radius;

  switch (style_) {
    case STYLE_BOTTOM:
      tl_radius = 0.0f;
      tr_radius = 0.0f;
      bl_radius = kBubbleCornerRadius;
      br_radius = kBubbleCornerRadius;
      break;
    case STYLE_FLOATING:
      tl_radius = 0.0f;
      tr_radius = kBubbleCornerRadius;
      bl_radius = kBubbleCornerRadius;
      br_radius = kBubbleCornerRadius;
      break;
    case STYLE_STANDARD:
      tl_radius = 0.0f;
      tr_radius = kBubbleCornerRadius;
      bl_radius = 0.0f;
      br_radius = 0.0f;
      break;
  }

  // Background / Edge

  NSRect bounds = [self bounds];
  NSBezierPath *border = [NSBezierPath gtm_bezierPathWithRoundRect:bounds
                                               topLeftCornerRadius:tl_radius
                                              topRightCornerRadius:tr_radius
                                            bottomLeftCornerRadius:bl_radius
                                           bottomRightCornerRadius:br_radius];

  [[NSColor colorWithDeviceWhite:kWindowFill alpha:1.0f] set];
  [border fill];

  border = [NSBezierPath gtm_bezierPathWithRoundRect:bounds
                                 topLeftCornerRadius:tl_radius
                                topRightCornerRadius:tr_radius
                              bottomLeftCornerRadius:bl_radius
                             bottomRightCornerRadius:br_radius];

  [[NSColor colorWithDeviceWhite:kWindowEdge alpha:1.0f] set];
  [border stroke];

  // Text

  NSFont* textFont = [self font];
  NSShadow* textShadow = [[[NSShadow alloc] init] autorelease];
  [textShadow setShadowBlurRadius:1.5f];
  [textShadow setShadowColor:[NSColor whiteColor]];
  [textShadow setShadowOffset:NSMakeSize(0.0f, -1.0f)];

  NSDictionary* textDict = [NSDictionary dictionaryWithObjectsAndKeys:
    textFont, NSFontAttributeName,
    textShadow, NSShadowAttributeName,
    nil];
  [content_ drawAtPoint:NSMakePoint(kTextPositionX, kTextPositionY)
         withAttributes:textDict];
}

@end
