// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_theme_provider.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"

NSImage* BrowserThemeProvider::GetNSImageNamed(int id) {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the image in the cache.
  NSImageMap::const_iterator found = nsimage_cache_.find(id);
  if (found != nsimage_cache_.end())
    return found->second;

  // Why do we load the file directly into the image rather than doing the whole
  // SkBitmap > native conversion that others do? Going direct means:
  // - we use the color profiles and other embedded info in the image file
  // - we don't fall back to the default resources which we don't use on the Mac
  std::vector<unsigned char> raw_data;
  if (ReadThemeFileData(id, &raw_data)) {
    NSData* ns_data = [NSData dataWithBytes:&raw_data.front()
                                     length:raw_data.size()];
    if (ns_data) {
      NSImage* nsimage = [[NSImage alloc] initWithData:ns_data];

      // We loaded successfully.  Cache the image.
      if (nsimage) {
        nsimage_cache_[id] = nsimage;
        return nsimage;
      }
    }
  }

  return nil;
}

void BrowserThemeProvider::FreePlatformImages() {
  DCHECK(CalledOnValidThread());

  // Free images.
  for (NSImageMap::iterator i = nsimage_cache_.begin();
       i != nsimage_cache_.end(); i++) {
    [i->second release];
  }
  nsimage_cache_.clear();
}
