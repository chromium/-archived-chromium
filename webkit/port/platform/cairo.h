// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
