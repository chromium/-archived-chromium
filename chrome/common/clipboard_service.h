// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CLIPBOARD_SERIVCE_H__
#define CHROME_COMMON_CLIPBOARD_SERIVCE_H__

#include <string>
#include <vector>

#include "base/clipboard.h"

class SkBitmap;

class ClipboardService : public Clipboard {
 public:
  ClipboardService();

  // Adds a bitmap to the clipboard
  // This is the slowest way to copy a bitmap to the clipboard as we must fist
  // memcpy the bits into GDI and the blit the bitmap to the clipboard.
  void WriteBitmap(const SkBitmap& bitmap);

 private:

  DISALLOW_EVIL_CONSTRUCTORS(ClipboardService);
};

#endif  // CHROME_COMMON_CLIPBOARD_SERIVCE_H__

