// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/basictypes.h"

#include "WTF/ListHashSet.h"
#include "WTF/Vector.h"

#include "SkiaFontWin.h"

#include "SkCanvas.h"
#include "SkPaint.h"

namespace WebCore {

namespace {

struct CachedOutlineKey {
    CachedOutlineKey() : font(NULL), glyph(0), path(NULL) {}
    CachedOutlineKey(HFONT f, WORD g) : font(f), glyph(g), path(NULL) {}

    HFONT font;
    WORD glyph;

    // The lifetime of this pointer is managed externally to this class. Be sure
    // to call DeleteOutline to remove items.
    SkPath* path;
};

const bool operator==(const CachedOutlineKey& a, const CachedOutlineKey& b)
{
    return a.font == b.font && a.glyph == b.glyph;
}

struct CachedOutlineKeyHash {
    static unsigned hash(const CachedOutlineKey& key)
    {
        return bit_cast<unsigned>(key.font) + key.glyph;
    }

    static unsigned equal(const CachedOutlineKey& a,
                          const CachedOutlineKey& b)
    {
        return a.font == b.font && a.glyph == b.glyph;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

typedef ListHashSet<CachedOutlineKey, CachedOutlineKeyHash> OutlineCache;
OutlineCache outlineCache;

// The global number of glyph outlines we'll cache.
const int outlineCacheSize = 256;

inline FIXED SkScalarToFIXED(SkScalar x)
{
    return bit_cast<FIXED>(SkScalarToFixed(x));
}

inline SkScalar FIXEDToSkScalar(FIXED fixed)
{
    return SkFixedToScalar(bit_cast<SkFixed>(fixed));
}

// Removes the given key from the cached outlines, also deleting the path.
void DeleteOutline(OutlineCache::iterator deleteMe)
{
    delete deleteMe->path;
    outlineCache.remove(deleteMe);
}

void AddPolyCurveToPath(const TTPOLYCURVE* polyCurve, SkPath* path)
{
    switch (polyCurve->wType) {
    case TT_PRIM_LINE:
        for (WORD i = 0; i < polyCurve->cpfx; i++) {
          path->lineTo(FIXEDToSkScalar(polyCurve->apfx[i].x),
                       -FIXEDToSkScalar(polyCurve->apfx[i].y));
        }
        break;

    case TT_PRIM_QSPLINE:
        // FIXME(brettw) doesn't this duplicate points if we do the loop > once?
        for (WORD i = 0; i < polyCurve->cpfx - 1; i++) {
            SkScalar bx = FIXEDToSkScalar(polyCurve->apfx[i].x);
            SkScalar by = FIXEDToSkScalar(polyCurve->apfx[i].y);

            SkScalar cx = FIXEDToSkScalar(polyCurve->apfx[i + 1].x);
            SkScalar cy = FIXEDToSkScalar(polyCurve->apfx[i + 1].y);
            if (i < polyCurve->cpfx - 2) {
                // We're not the last point, compute C.
                cx = SkScalarAve(bx, cx);
                cy = SkScalarAve(by, cy);
            }

            // Need to flip the y coordinates since the font's coordinate system is
            // flipped from ours vertically.
            path->quadTo(bx, -by, cx, -cy);
        }
        break;

    case TT_PRIM_CSPLINE:
        // FIXME
        break;
    }
}

// Fills the given SkPath with the outline for the given glyph index. The font
// currently selected into the given DC is used. Returns true on success.
bool GetPathForGlyph(HDC dc, WORD glyph, SkPath* path)
{
    char buffer[4096];
    GLYPHMETRICS gm;
    MAT2 mat = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};  // Each one is (fract,value).

    DWORD totalSize = GetGlyphOutlineW(dc, glyph, GGO_GLYPH_INDEX | GGO_NATIVE,
                                     &gm, arraysize(buffer), buffer, &mat);
    if (totalSize == GDI_ERROR)
        return false;

    const char* curGlyph = buffer;
    const char* endGlyph = &buffer[totalSize];
    while (curGlyph < endGlyph) {
        const TTPOLYGONHEADER* polyHeader =
            reinterpret_cast<const TTPOLYGONHEADER*>(curGlyph);
        path->moveTo(FIXEDToSkScalar(polyHeader->pfxStart.x),
                     -FIXEDToSkScalar(polyHeader->pfxStart.y));

        const char* curPoly = curGlyph + sizeof(TTPOLYGONHEADER);
        const char* endPoly = curGlyph + polyHeader->cb;
        while (curPoly < endPoly) {
            const TTPOLYCURVE* polyCurve =
                reinterpret_cast<const TTPOLYCURVE*>(curPoly);
            AddPolyCurveToPath(polyCurve, path);
            curPoly += sizeof(WORD) * 2 + sizeof(POINTFX) * polyCurve->cpfx;
        }
        curGlyph += polyHeader->cb;
    }

    path->close();
    return true;
}

// Returns a SkPath corresponding to the give glyph in the given font. The font
// should be selected into the given DC. The returned path is owned by the
// hashtable. Returns NULL on error.
const SkPath* GetCachedPathForGlyph(HDC hdc, HFONT font, WORD glyph)
{
    CachedOutlineKey key(font, glyph);
    OutlineCache::iterator found = outlineCache.find(key);
    if (found != outlineCache.end()) {
        // Keep in MRU order by removing & reinserting the value.
        key = *found;
        outlineCache.remove(found);
        outlineCache.add(key);
        return key.path;
    }

    key.path = new SkPath;
    if (!GetPathForGlyph(hdc, glyph, key.path))
      return NULL;

    if (outlineCache.size() > outlineCacheSize) {
        // The cache is too big, find the oldest value (first in the list).
        DeleteOutline(outlineCache.begin());
    }

    outlineCache.add(key);
    return key.path;
}

}  // namespace

bool SkiaDrawText(HFONT hfont,
                  SkCanvas* canvas,
                  const SkPoint& point,
                  SkPaint* paint,
                  const WORD* glyphs,
                  const int* advances,
                  int num_glyphs)
{
    HDC dc = GetDC(0);
    HGDIOBJ old_font = SelectObject(dc, hfont);

    canvas->save();
    canvas->translate(point.fX, point.fY);

    for (int i = 0; i < num_glyphs; i++) {
        const SkPath* path = GetCachedPathForGlyph(dc, hfont, glyphs[i]);
        if (!path)
            return false;
        canvas->drawPath(*path, *paint);
        canvas->translate(advances[i], 0);
    }

    canvas->restore();

    SelectObject(dc, old_font);
    ReleaseDC(0, dc);
    return true;
}

/* TODO(brettw) finish this implementation
bool SkiaDrawComplexText(HFONT font,
                         SkCanvas* canvas,
                         const SkPoint& point,
                         SkPaint* paint
                         UINT fuOptions,
                         const SCRIPT_ANALYSIS* psa,
                         const WORD* pwGlyphs,
                         int cGlyphs,
                         const int* advances,
                         const int* justifies,
                         const GOFFSET* glyph_offsets)
{
    HDC dc = GetDC(0);
    HGDIOBJ old_font = SelectObject(dc, hfont);

    canvas->save();
    canvas->translate(point.fX, point.fY);

    for (int i = 0; i < cGlyphs; i++) {
        canvas->translate(glyph_offsets[i].du, glyph_offsets[i].dv);


        

        // Undo the offset for this glyph.
        canvas->translate(-glyph_offsets[i].du, -glyph_offsets[i].dv);

        // And advance to where we're drawing the next one. We use the justifies
        // run since that is the justified advances for each character, rather than
        // the adnvaces one.
        canvas->translate(justifies[i], 0);
    }

    canvas->restore();

    SelectObject(dc, old_font);
    ReleaseDC(0, dc);
}*/

void RemoveFontFromSkiaFontWinCache(HFONT hfont)
{
    // ListHashSet isn't the greatest structure for deleting stuff out of, but
    // removing entries will be relatively rare (we don't remove fonts much, nor
    // do we draw out own glyphs using these routines much either).
    //
    // We keep a list of all glyphs we're removing which we do in a separate
    // pass.
    Vector<CachedOutlineKey> outlinesToDelete;
    for (OutlineCache::iterator i = outlineCache.begin();
         i != outlineCache.end(); ++i)
        outlinesToDelete.append(*i);

    for (Vector<CachedOutlineKey>::iterator i = outlinesToDelete.begin();
         i != outlinesToDelete.end(); ++i)
        DeleteOutline(outlineCache.find(*i));
}

}  // namespace WebCore
