// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GFX_GTK_UTIL_H_
#define APP_GFX_GTK_UTIL_H_

#include <gtk/gtk.h>

typedef struct _GdkPixbuf GdkPixbuf;
class SkBitmap;

namespace gfx {

// Convert and copy a SkBitmap to a GdkPixbuf. NOTE: this uses BGRAToRGBA, so
// it is an expensive operation.
GdkPixbuf* GdkPixbufFromSkBitmap(const SkBitmap* bitmap);

}  // namespace gfx

#endif  // APP_GFX_GTK_UTIL_H_
