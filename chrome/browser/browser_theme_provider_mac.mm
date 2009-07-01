// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_theme_provider.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "skia/ext/skia_utils_mac.h"

NSImage* BrowserThemeProvider::GetNSImageNamed(int id) {
  DCHECK(CalledOnValidThread());

  if (!HasCustomImage(id))
    return nil;

  // Check to see if we already have the image in the cache.
  NSImageMap::const_iterator found = nsimage_cache_.find(id);
  if (found != nsimage_cache_.end())
    return found->second;

  // Why don't we load the file directly into the image instead of the whole
  // SkBitmap > native conversion?
  // - For consistency with other platforms.
  // - To get the generated tinted images.
  SkBitmap* bitmap = GetBitmapNamed(id);
  NSImage* nsimage = gfx::SkBitmapToNSImage(*bitmap);

  // We loaded successfully.  Cache the image.
  if (nsimage) {
    nsimage_cache_[id] = [nsimage retain];
    return nsimage;
  }

  // We failed to retrieve the bitmap, show a debugging red square.
  LOG(WARNING) << "Unable to load NSImage with id " << id;
  NOTREACHED();  // Want to assert in debug mode.

  static NSImage* empty_image = NULL;
  if (!empty_image) {
    // The placeholder image is bright red so people notice the problem.  This
    // image will be leaked, but this code should never be hit.
    NSRect image_rect = NSMakeRect(0, 0, 32, 32);
    empty_image = [[NSImage alloc] initWithSize:image_rect.size];
    [empty_image lockFocus];
    [[NSColor redColor] set];
    NSRectFill(image_rect);
    [empty_image unlockFocus];
  }

  return empty_image;
}

NSColor* BrowserThemeProvider::GetNSColorTint(int id) {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the color in the cache.
  NSColorMap::const_iterator found = nscolor_cache_.find(id);
  if (found != nscolor_cache_.end())
    return found->second;

  TintMap::iterator tint_iter = tints_.find(GetTintKey(id));
  if (tint_iter != tints_.end()) {
    skia::HSL tint = tint_iter->second;

    // The tint is HSL, not HSB, but we're cheating for now. TODO(avi,alcor):
    // determine how much this matters and fix it if necessary.
    // http://crbug.com/15760
    NSColor* tint_color = [NSColor colorWithCalibratedHue:tint.h
                                               saturation:tint.s
                                               brightness:tint.l
                                                    alpha:1.0];

    // We loaded successfully.  Cache the color.
    if (tint_color) {
      nscolor_cache_[id] = [tint_color retain];
      return tint_color;
    }
  }

  return nil;
}

void BrowserThemeProvider::FreePlatformCaches() {
  DCHECK(CalledOnValidThread());

  // Free images.
  for (NSImageMap::iterator i = nsimage_cache_.begin();
       i != nsimage_cache_.end(); i++) {
    [i->second release];
  }
  nsimage_cache_.clear();

  // Free colors.
  for (NSColorMap::iterator i = nscolor_cache_.begin();
       i != nscolor_cache_.end(); i++) {
    [i->second release];
  }
  nscolor_cache_.clear();
}
