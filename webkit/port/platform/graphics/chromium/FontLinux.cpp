// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Font.h"

#include "FloatRect.h"
#include "NotImplemented.h"

namespace WebCore {

// TODO(agl): stubs only

void Font::drawGlyphs(GraphicsContext*, const SimpleFontData*,
                      const GlyphBuffer&, int from, int to,
                      const FloatPoint&) const
{
    notImplemented();
}

void Font::drawComplexText(GraphicsContext* context, const TextRun& run,
                           const FloatPoint& point, int from, int to) const
{
    notImplemented();
}

float Font::floatWidthForComplexText(const TextRun& run) const
{
    notImplemented();
    return 0;
}

int Font::offsetForPositionForComplexText(const TextRun& run, int x,
                                          bool includePartialGlyphs) const 
{
    notImplemented();
    return 0;
}

FloatRect Font::selectionRectForComplexText(const TextRun& run,
                                            const IntPoint& point, int h,
                                            int from, int to) const 
{
    notImplemented();
    return FloatRect();
}

}  // namespace WebCore
