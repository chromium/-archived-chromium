// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_THEME_PROVIDER_H_
#define APP_THEME_PROVIDER_H_

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkColor.h"

#if defined(OS_LINUX)
#include <gdk/gdk.h>
#endif

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

#if defined(OS_LINUX)
  // Gets the GdkPixbuf with the specified |id|.  Returns a pointer to a shared
  // instance of the GdkPixbuf.  This shared GdkPixbuf is owned by the theme
  // provider and should not be freed.
  //
  // The bitmap is assumed to exist. This function will log in release, and
  // assert in debug mode if it does not. On failure, this will return a
  // pointer to a shared empty placeholder bitmap so it will be visible what
  // is missing.
  virtual GdkPixbuf* GetPixbufNamed(int id) = 0;
#endif
};

#endif  // APP_THEME_PROVIDER_H_
