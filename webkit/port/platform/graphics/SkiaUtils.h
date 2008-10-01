// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares helper routines for using Skia in WebKit.

#ifndef SkiaUtils_h
#define SkiaUtils_h

#include <wtf/PassRefPtr.h>
#include "base/float_util.h"
#include "GraphicsContext.h"
#include "SkPath.h"
#include "SkShader.h"
#include "PlatformContextSkia.h"

class WebCore::SharedBuffer;
class SkRegion;

// Converts a WebCore composit operation (WebCore::Composite*) to the
// corresponding Skia type.
SkPorterDuff::Mode WebCoreCompositeToSkiaComposite(WebCore::CompositeOperator);

// Converts Android colors to WebKit ones.
WebCore::Color SkPMColorToWebCoreColor(SkPMColor pm);

// Skia has problems when passed infinite, etc floats, filter them to 0.
inline SkScalar WebCoreFloatToSkScalar(const float& f) {
  return SkFloatToScalar(base::IsFinite(f) ? f : 0);
}

inline SkScalar WebCoreDoubleToSkScalar(const double& d) {
  return SkDoubleToScalar(base::IsFinite(d) ? d : 0);
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

// Constructs a BMP V4 bitmap from an SkBitmap.
PassRefPtr<WebCore::SharedBuffer> SerializeSkBitmap(const SkBitmap&);

#endif  // SkiaUtils_h
