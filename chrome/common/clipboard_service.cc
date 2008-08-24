// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Many of these functions are based on those found in
// webkit/port/platform/PasteboardWin.cpp

#include "chrome/common/clipboard_service.h"

#include "SkBitmap.h"

ClipboardService::ClipboardService() {
}

void ClipboardService::WriteBitmap(const SkBitmap& bitmap) {
  SkAutoLockPixels bitmap_lock(bitmap);
  Clipboard::WriteBitmap(bitmap.getPixels(),
                         gfx::Size(bitmap.width(), bitmap.height()));
}

