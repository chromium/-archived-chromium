// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper methods for Skia SVG rendering, inspired by CgSupport.*

#ifndef SkiaSupport_h
#define SkiaSupport_h

#if ENABLE(SVG)

#include "SVGResource.h"

namespace WebCore {

class Path;
class FloatRect;
class RenderStyle;
class RenderObject;
class GraphicsContext;

void applyStrokeStyleToContext(GraphicsContext*, const RenderStyle*, const RenderObject*);
void applyFillStyleToContext(GraphicsContext*, const RenderStyle*, const RenderObject*);

// Returns a statically allocated 1x1 GraphicsContext intended for temporary
// operations. Please save() the state and restore() it when you're done with
// the context.
GraphicsContext* scratchContext();

// Computes the bounding box for the stroke and style currently selected into
// the given bounding box. This also takes into account the stroke width.
FloatRect boundingBoxForCurrentStroke(const GraphicsContext* context);

// Returns the bounding box for the given path of the given style, including the
// stroke width.
FloatRect strokeBoundingBox(const Path& path, RenderStyle*, const RenderObject*);

}


#endif // ENABLE(SVG)
#endif // #ifndef SkiaSupport_h

