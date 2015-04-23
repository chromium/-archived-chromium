// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_theme_provider.h"

#include "base/gfx/gtk_util.h"
#include "base/logging.h"

GdkPixbuf* BrowserThemeProvider::GetPixbufNamed(int id) {
  DCHECK(CalledOnValidThread());

  // Check to see if we already have the pixbuf in the cache.
  GdkPixbufMap::const_iterator found = gdk_pixbufs_.find(id);
  if (found != gdk_pixbufs_.end())
    return found->second;

  SkBitmap* bitmap = GetBitmapNamed(id);
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(bitmap);

  // We loaded successfully.  Cache the pixbuf.
  if (pixbuf) {
    gdk_pixbufs_[id] = pixbuf;
    return pixbuf;
  }

  // We failed to retrieve the bitmap, show a debugging red square.
  LOG(WARNING) << "Unable to load GdkPixbuf with id " << id;
  NOTREACHED();  // Want to assert in debug mode.

  static GdkPixbuf* empty_bitmap = NULL;
  if (!empty_bitmap) {
    // The placeholder bitmap is bright red so people notice the problem.
    // This bitmap will be leaked, but this code should never be hit.
    scoped_ptr<SkBitmap> skia_bitmap(new SkBitmap());
    skia_bitmap->setConfig(SkBitmap::kARGB_8888_Config, 32, 32);
    skia_bitmap->allocPixels();
    skia_bitmap->eraseARGB(255, 255, 0, 0);
    empty_bitmap = gfx::GdkPixbufFromSkBitmap(skia_bitmap.get());
  }
  return empty_bitmap;
}

void BrowserThemeProvider::FreePlatformCaches() {
  DCHECK(CalledOnValidThread());

  // Free GdkPixbufs.
  for (GdkPixbufMap::iterator i = gdk_pixbufs_.begin();
       i != gdk_pixbufs_.end(); i++) {
    g_object_unref(i->second);
  }
  gdk_pixbufs_.clear();
}
