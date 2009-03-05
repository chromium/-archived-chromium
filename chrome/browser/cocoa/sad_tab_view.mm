// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/sad_tab_view.h"

#include "base/sys_string_conversions.h"
#include "chrome/common/l10n_util.h"
#include "grit/generated_resources.h"

static const int kSadTabOffset = -64;
static const int kIconTitleSpacing = 20;
static const int kTitleMessageSpacing = 15;

@implementation SadTabView

- (void)drawRect:(NSRect)dirtyRect {
  NSImage* sadTabImage = [NSImage imageNamed:@"sadtab"];
  NSString* title =
      base::SysWideToNSString(l10n_util::GetString(IDS_SAD_TAB_TITLE));
  NSString* message =
      base::SysWideToNSString(l10n_util::GetString(IDS_SAD_TAB_MESSAGE));

  NSColor* textColor = [NSColor whiteColor];
  NSColor* backgroundColor = [NSColor colorWithCalibratedRed:(35.0f/255.0f)
                                                       green:(48.0f/255.0f)
                                                        blue:(64.0f/255.0f)
                                                       alpha:1.0];

  // Layout
  NSFont* titleFont = [NSFont boldSystemFontOfSize:[NSFont systemFontSize]];
  NSFont* messageFont = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];

  NSDictionary* titleAttrs = [NSDictionary dictionaryWithObjectsAndKeys:
                              titleFont, NSFontAttributeName,
                              textColor, NSForegroundColorAttributeName,
                              nil];
  NSDictionary* messageAttrs = [NSDictionary dictionaryWithObjectsAndKeys:
                                messageFont, NSFontAttributeName,
                                textColor, NSForegroundColorAttributeName,
                                nil];

  NSAttributedString* titleString =
      [[[NSAttributedString alloc] initWithString:title
                                       attributes:titleAttrs] autorelease];
  NSAttributedString* messageString =
      [[[NSAttributedString alloc] initWithString:message
                                       attributes:messageAttrs] autorelease];

  NSRect viewBounds = [self bounds];

  NSSize sadTabImageSize = [sadTabImage size];
  CGFloat iconWidth = sadTabImageSize.width;
  CGFloat iconHeight = sadTabImageSize.height;
  CGFloat iconX = (viewBounds.size.width - iconWidth) / 2;
  CGFloat iconY =
      ((viewBounds.size.height - iconHeight) / 2) - kSadTabOffset;

  NSSize titleSize = [titleString size];
  CGFloat titleX = (viewBounds.size.width - titleSize.width) / 2;
  CGFloat titleY = iconY - kIconTitleSpacing - titleSize.height;

  NSSize messageSize = [messageString size];
  CGFloat messageX = (viewBounds.size.width - messageSize.width) / 2;
  CGFloat messageY = titleY - kTitleMessageSpacing - messageSize.height;

  // Paint
  [backgroundColor set];
  NSRectFill(viewBounds);

  [sadTabImage drawAtPoint:NSMakePoint(iconX, iconY)
                  fromRect:NSZeroRect
                 operation:NSCompositeSourceOver
                  fraction:1.0f];
  [titleString drawAtPoint:NSMakePoint(titleX, titleY)];
  [messageString drawAtPoint:NSMakePoint(messageX, messageY)];
}

@end
