// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_THEME_PROVIDER_H_
#define APP_THEME_PROVIDER_H_

#include "skia/include/SkColor.h"

class SkBitmap;

////////////////////////////////////////////////////////////////////////////////
//
// ThemeProvider
//
//   ThemeProvider is an abstract class that defines the API that should be
//   implemented to provide bitmaps and color information for a given theme.
//
////////////////////////////////////////////////////////////////////////////////

class ThemeProvider {
 public:
  virtual ~ThemeProvider() { }

  // Get the bitmap specified by |id|. An implementation of ThemeProvider should
  // have its own source of ids (e.g. an enum, or external resource bundle).
  virtual SkBitmap* GetBitmapNamed(int id) = 0;

  // Get the color specified by |id|.
  virtual SkColor GetColor(int id) = 0;
};

#endif  // APP_THEME_PROVIDER_H_
