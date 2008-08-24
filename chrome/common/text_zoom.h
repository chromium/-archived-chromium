// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_TEXT_ZOOM_H__
#define CHROME_COMMON_TEXT_ZOOM_H__

// Used in AlterTextSize IPC call.
namespace text_zoom {
  enum TextSize {
    TEXT_SMALLER  = -1,
    TEXT_STANDARD = 0,
    TEXT_LARGER   = 1,
  };
}

#endif

