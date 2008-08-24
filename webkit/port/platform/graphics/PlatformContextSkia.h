// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformContextSkia_h
#define PlatformContextSkia_h

#include "SkGraphicsContext.h"

namespace WebCore {
class GraphicsContext;
class GraphicsContextPlatformPrivate;
class PlatformScrollbar;
class ScrollView;
}

// This class is the "PlatformGraphicsContext" class when running under the
// Skia/Windows port. Name it PlatformContextSkia until we fix GraphicsContext.h.
class PlatformContextSkia : public SkGraphicsContext {
    // It initialize the back reference to GraphicsContextPlatformPrivate when the
    // PlatformContextSkia instance is associated with the GraphicsContext.
    friend class WebCore::GraphicsContextPlatformPrivate;
public:
    // For printing, there shouldn't be any canvas. canvas can be NULL.
    PlatformContextSkia(gfx::PlatformCanvas* canvas);
    ~PlatformContextSkia();

protected:
    // Make it accessible to friends.
    using SkGraphicsContext::setPaintContext;

private:
};

#endif  // PlatformContextSkia_h

