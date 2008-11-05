// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CLIPBOARD_SERVICE_H__
#define CHROME_COMMON_CLIPBOARD_SERVICE_H__

#include <string>
#include <vector>

#include "base/clipboard.h"

class SkBitmap;

class ClipboardService : public Clipboard {
 public:
  ClipboardService() {}

 private:

  DISALLOW_EVIL_CONSTRUCTORS(ClipboardService);
};

#endif  // CHROME_COMMON_CLIPBOARD_SERVICE_H__
