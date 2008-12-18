// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SkiaFont_Win_h
#define SkiaFont_Win_h

#include <windows.h>
#include <usp10.h>

class SkCanvas;
class SkPaint;
struct SkPoint;

// This file provices Skia equivalents to Windows text drawing functions. They
// will get the outlines from Windows and draw then using Skia using the given
// parameters in the paint arguments. This allows more complex effects and
// transforms to be drawn than Windows allows.
//
// These functions will be significantly slower than Windows GDI, and the text
// will look different (no ClearType), so use only when necessary.
//
// When you call a Skia* text drawing function, various glyph outlines will be
// cached. As a result, you should call RemoveFontFromSkiaFontWinCache when
// the font is destroyed so that the cache does not outlive the font (since the
// HFONTs are recycled).
namespace WebCore {

// Analog of the Windows GDI function DrawText, except using the given SkPaint
// attributes for the text. See above for more.
//
// Returns true of the text was drawn successfully. False indicates an error
// from Windows.
bool SkiaDrawText(HFONT hfont,
                  SkCanvas* canvas,
                  const SkPoint& point,
                  SkPaint* paint,
                  const WORD* glyphs,
                  const int* advances,
                  int num_glyphs);

// This mirrors the features of ScriptTextOut.
/* TODO(brettw) finish this implementation.
bool SkiaDrawComplexText(HFONT font,
                         SkCanvas* canvas,
                         const SkPoint& point,
                         SkPaint* paint,
                         UINT fuOptions,
                         const SCRIPT_ANALYSIS* psa,
                         const WORD* pwGlyphs,
                         int cGlyphs,
                         const int* advances,
                         const int* justifies,
                         const GOFFSET* glyph_offsets);
*/

// Removes any cached glyphs from the outline cache corresponding to the given
// font handle.
void RemoveFontFromSkiaFontWinCache(HFONT hfont);

}  // namespace WebCore

#endif  // SkiaFont_Win_h
