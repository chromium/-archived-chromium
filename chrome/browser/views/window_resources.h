// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_WINDOW_RESOURCES_H_
#define CHROME_BROWSER_VIEWS_WINDOW_RESOURCES_H_

#include "SkBitmap.h"

class ChromeFont;

typedef int FramePartBitmap;

class WindowResources {
 public:
  virtual ~WindowResources() { }
  virtual SkBitmap* GetPartBitmap(FramePartBitmap part) const = 0;
  virtual const ChromeFont& GetTitleFont() const = 0;
  virtual SkColor GetTitleColor() const { return SK_ColorWHITE; }

  SkBitmap app_top_left() const { return app_top_left_; }
  SkBitmap app_top_center() const { return app_top_center_; }
  SkBitmap app_top_right() const { return app_top_right_; }

 protected:
  static void InitClass();

 private:
  // Bitmaps shared between all frame types.
  static SkBitmap app_top_left_;
  static SkBitmap app_top_center_;
  static SkBitmap app_top_right_;
};

#endif  // CHROME_BROWSER_VIEWS_WINDOW_RESOURCES_H_
