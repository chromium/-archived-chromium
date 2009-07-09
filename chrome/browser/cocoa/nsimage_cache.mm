// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/nsimage_cache.h"

#include "base/logging.h"
#include "base/mac_util.h"

namespace nsimage_cache {

static NSMutableDictionary *image_cache = nil;

NSImage *ImageNamed(NSString *name) {
  DCHECK(name);

  // NOTE: to make this thread safe, we'd have to sync on the cache and
  // also force all the bundle calls on the main thread.

  if (!image_cache) {
    image_cache = [[NSMutableDictionary alloc] init];
    DCHECK(image_cache);
  }

  NSImage *result = [image_cache objectForKey:name];
  if (!result) {
    DLOG_IF(INFO, [[name pathExtension] length] == 0)
        << "Suggest including the extension in the image name";

    NSString *path = [mac_util::MainAppBundle() pathForImageResource:name];
    if (path) {
      @try {
        result = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];
        [image_cache setObject:result forKey:name];
      }
      @catch (id err) {
        DLOG(ERROR) << "Failed to load the image for name '"
            << [name UTF8String] << "' from path '" << [path UTF8String]
            << "', error: " << [err description];
        result = nil;
      }
    }
  }

  // TODO: if we put ever limit the cache size, this should retain & autorelease
  // the image.
  return result;
}

void Clear(void) {
  // NOTE: to make this thread safe, we'd have to sync on the cache.
  [image_cache removeAllObjects];
}

}  // nsimage_cache
