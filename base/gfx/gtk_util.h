// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GFX_GTK_UTIL_H_
#define BASE_GFX_GTK_UTIL_H_

#include <vector>

typedef struct _GdkColor GdkColor;
typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GdkRegion GdkRegion;
typedef struct _GtkWidget GtkWidget;
class SkBitmap;

// Define a macro for creating GdkColors from RGB values.  This is a macro to
// allow static construction of literals, etc.  Use this like:
//   GdkColor white = GDK_COLOR_RGB(0xff, 0xff, 0xff);
#define GDK_COLOR_RGB(r, g, b) {0, r * 257, g * 257, b * 257}

namespace gfx {

class Rect;

extern const GdkColor kGdkWhite;
extern const GdkColor kGdkBlack;

// Modify the given region by subtracting the given rectangles.
void SubtractRectanglesFromRegion(GdkRegion* region,
                                  const std::vector<Rect>& cutouts);

// Convert and copy a SkBitmap to a GdkPixbuf.  NOTE: This is an expensive
// operation, all of the pixels must be copied and their order swapped.
GdkPixbuf* GdkPixbufFromSkBitmap(const SkBitmap* bitmap);

// Create a GtkBin with |child| as its child widget.  This bin will paint a
// border of color |color| with the sizes specified in pixels.
GtkWidget* CreateGtkBorderBin(GtkWidget* child, const GdkColor* color,
                              int top, int bottom, int left, int right);

}  // namespace gfx

#endif  // BASE_GFX_GTK_UTIL_H_
