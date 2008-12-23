// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "config.h"
#include "webkit/glue/webkit_glue.h"

#import <AppKit/NSGraphics.h>
#import <Foundation/NSGeometry.h>

namespace webkit_glue {

static NSScreen *ScreenForWindow(NSWindow *window) {
  NSScreen *screen = [window screen];  // nil if the window is off-screen
  if (screen)
    return screen;

  NSArray *screens = [NSScreen screens];
  if ([screens count] > 0)
    return [screens objectAtIndex:0];  // screen containing the menubar

  return nil;
}

static gfx::Rect ToUserSpace(const NSRect& rect, NSWindow *destination) {
  CGRect user_rect = NSRectToCGRect(rect);

  user_rect.origin.y =
      NSMaxY([ScreenForWindow(destination) frame]) -
      (user_rect.origin.y + user_rect.size.height);  // flip

  if (destination) {
    CGFloat scale = 1 / [destination userSpaceScaleFactor];  // scale down
    user_rect.origin.x *= scale;
    user_rect.origin.y *= scale;
    user_rect.size.width *= scale;
    user_rect.size.height *= scale;
  }

  return gfx::Rect(user_rect);
}

ScreenInfo GetScreenInfoHelper(gfx::NativeView view) {
  NSString *color_space = NSColorSpaceFromDepth([[NSScreen deepestScreen] depth]);
  bool monochrome = color_space == NSCalibratedWhiteColorSpace ||
                    color_space == NSCalibratedBlackColorSpace ||
                    color_space == NSDeviceWhiteColorSpace ||
                    color_space == NSDeviceBlackColorSpace;

  ScreenInfo results;
  results.depth =
      NSBitsPerPixelFromDepth([[NSScreen deepestScreen] depth]);
  results.depth_per_component =
      NSBitsPerSampleFromDepth([[NSScreen deepestScreen] depth]);
  results.is_monochrome = 
      color_space == NSCalibratedWhiteColorSpace ||
      color_space == NSCalibratedBlackColorSpace ||
      color_space == NSDeviceWhiteColorSpace ||
      color_space == NSDeviceBlackColorSpace;
  results.rect =
      ToUserSpace([ScreenForWindow([view window]) frame], [view window]);
  results.available_rect =
      ToUserSpace([ScreenForWindow([view window]) visibleFrame], [view window]);
  return results;
}

}  // namespace webkit_glue
