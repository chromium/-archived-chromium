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


#ifndef BASE_GFX_SKIA_UTILS_MAC_H__
#define BASE_GFX_SKIA_UTILS_MAC_H__

#include "SkColor.h"
#include <CoreGraphics/CGColor.h>

struct SkIRect;
struct SkPoint;
struct SkRect;

namespace gfx {

// Converts a Skia point to a CoreGraphics CGPoint.
// Both use same in-memory format.
inline const CGPoint& SkPointToCGPoint(const SkPoint& point) {
  return reinterpret_cast<const CGPoint&>(point);
}

// Converts a CoreGraphics point to a Skia CGPoint.
// Both use same in-memory format.
inline const SkPoint& CGPointToSkPoint(const CGPoint& point) {
  return reinterpret_cast<const SkPoint&>(point);
}
  
// Rectangle converters.
SkRect CGRectToSkRect(const CGRect& rect);
SkIRect CGRectToSkIRect(const CGRect& rect);

// Converts a Skia rect to a CoreGraphics CGRect.
CGRect SkIRectToCGRect(const SkIRect& rect);
CGRect SkRectToCGRect(const SkRect& rect);

// Converts CGColorRef to the ARGB layout Skia expects.
SkColor CGColorRefToSkColor(CGColorRef color);

// Converts ARGB to CGColorRef.
CGColorRef SkColorToCGColorRef(SkColor color);

}  // namespace gfx

#endif
