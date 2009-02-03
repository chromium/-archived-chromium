// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store.h"

BackingStore::BackingStore(const gfx::Size& size)
    : size_(size) {
  if (!canvas_.initialize(size.width(), size.height(), true))
    SK_CRASH();
}

BackingStore::~BackingStore() {
}

bool BackingStore::PaintRect(base::ProcessHandle process,
                             BitmapWireData bitmap,
                             const gfx::Rect& bitmap_rect) {
  if (bitmap.width() != bitmap_rect.width() ||
      bitmap.height() != bitmap_rect.height() ||
      bitmap.config() != SkBitmap::kARGB_8888_Config) {
    return false;
  }

  canvas_.drawBitmap(bitmap, bitmap_rect.x(), bitmap_rect.y());
  return true;
}

void BackingStore::ScrollRect(base::ProcessHandle process,
                              BitmapWireData bitmap,
                              const gfx::Rect& bitmap_rect,
                              int dx, int dy,
                              const gfx::Rect& clip_rect,
                              const gfx::Size& view_size) {
  // TODO(port): implement scrolling
  NOTIMPLEMENTED();
}
