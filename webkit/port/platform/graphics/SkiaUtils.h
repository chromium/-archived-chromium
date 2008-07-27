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

// This file declares helper routines for using Skia in WebKit.

#ifndef SkiaUtils_h
#define SkiaUtils_h

#include "GraphicsContext.h"
#include "SkPath.h"
#include "SkShader.h"
#include "PlatformContextSkia.h"

class SkRegion;

// Converts the given WebCore point to a Skia point.
void WebCorePointToSkiaPoint(const WebCore::IntPoint& src, SkPoint* dst);
void WebCorePointToSkiaPoint(const WebCore::FloatPoint& src, SkPoint* dst);

// Converts from various types of WebCore rectangles to the corresponding
// Skia rectangle types.
void WebCoreRectToSkiaRect(const WebCore::IntRect& src, SkRect* dst);
void WebCoreRectToSkiaRect(const WebCore::FloatRect& src, SkRect* dst);
void WebCoreRectToSkiaRect(const WebCore::IntRect& src, SkIRect* dst);
void WebCoreRectToSkiaRect(const WebCore::FloatRect& src, SkIRect* dst);

// Converts a WebCore |Path| to an SkPath.
inline SkPath* PathToSkPath(const WebCore::Path& path) {
  return reinterpret_cast<SkPath*>(path.platformPath());
}

// Converts a WebCore composit operation (WebCore::Composite*) to the
// corresponding Skia type.
SkPorterDuff::Mode WebCoreCompositeToSkiaComposite(WebCore::CompositeOperator);

// Converts a WebCore tiling rule to the corresponding Skia tiling mode.
SkShader::TileMode WebCoreTileToSkiaTile(WebCore::Image::TileRule);

// Converts Android colors to WebKit ones.
WebCore::Color SkPMColorToWebCoreColor(SkPMColor pm);

// A platform graphics context is actually a PlatformContextSkia.
inline PlatformContextSkia* PlatformContextToPlatformContextSkia(
    PlatformGraphicsContext* context) {
  return reinterpret_cast<PlatformContextSkia*>(context);
}

// Skia has problems when passed infinite, etc floats, filter them to 0.
inline SkScalar WebCoreFloatToSkScalar(const float& f) {
  return SkFloatToScalar(_finite(f) ? f : 0);
}

inline SkScalar WebCoreDoubleToSkScalar(const double& d) {
  return SkDoubleToScalar(_finite(d) ? d : 0);
}

// Intersects the given source rect with the region, returning the smallest
// rectangular region that encompases the result.
//
// src_rect and dest_rect can be the same.
void IntersectRectAndRegion(const SkRegion& region, const SkRect& src_rect,
                            SkRect* dest_rect);

// Computes the smallest rectangle that, which when drawn to the given canvas,
// will cover the same area as the source rectangle. It will clip to the canvas'
// clip, doing the necessary coordinate transforms.
//
// src_rect and dest_rect can be the same.
void ClipRectToCanvas(const SkCanvas& canvas, const SkRect& src_rect,
                      SkRect* dest_rect);

// Determine if a given WebKit point is contained in a path
bool SkPathContainsPoint(SkPath* orig_path, WebCore::FloatPoint point, SkPath::FillType ft);

#endif  // SkiaUtils_h
