// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We compile WebKit with PLATFORM(CAIRO) defined. This only affects a few
// places in the shared headers. Therefore, we provide this replacement
// <cairo.h> that defines the Cairo types used in these shared headers to be
// the types that we desire.

#ifndef CHROME_WEBKIT_PORT_PLATFORM_CAIRO_H__
#define CHROME_WEBKIT_PORT_PLATFORM_CAIRO_H__

#include "SkMatrix.h"

// For AffineTransform.h, this allows us to define the internal matrix type
// to be the Skia one we use internally.
typedef SkMatrix cairo_matrix_t;

// For GlyphBuffer.h
struct cairo_glyph_t {
  // Cairo defines this to be an unsigned long. PLATFORM(CG) in GlyphBuffer.h
  // defines the glyph to be an unsigned short, so we do the same here.
  unsigned short index;

  // Totally unused. This is defined by Cairo and set in GlyphBuffer.h to 0,
  // so we define it. We use unsigned short so that the size of this structure
  // is padded out to 4 bytes.
  unsigned short y;

  // Cairo also defines an |x| here, but it's never used by the shared headers.
};

#endif  // CHROME_WEBKIT_PORT_PLATFORM_CAIRO_H__

