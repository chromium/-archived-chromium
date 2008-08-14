/*
** Copyright 2006, Google Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#include <math.h>

#include "config.h"
#include "GraphicsContext.h"
#include "GraphicsContextPrivate.h"
#include "wtf/MathExtras.h"

#include "Assertions.h"
#include "AffineTransform.h"
#include "NativeImageSkia.h"
#include "NotImplemented.h"
#include "SkBlurDrawLooper.h"
#include "SkCornerPathEffect.h"
#include "SkiaUtils.h"

#ifdef ANDROID_CANVAS_IMPL
# include "SkBitmap.h"
# include "SkGradientShader.h"
#endif

#include "base/gfx/platform_canvas_win.h"

using namespace std;

namespace WebCore {

namespace {

// "Reasonable" functions ------------------------------------------------------
//
// These functions check certain graphics primitives for being "reasonable".
// We don't like to send crazy data to the graphics layer that might overflow,
// and this helps us avoid some of those cases.
//
// THESE ARE NOT PERFECT. We can't guarantee what the graphics layer is doing.
// Ideally, all of these would be fixed in the graphics layer and we would not
// have to do any checking. You can uncomment the CHECK_REASONABLE flag to
// check the graphics layer.
#define CHECK_REASONABLE

static bool IsCoordinateReasonable(float coord)
{
#ifdef CHECK_REASONABLE
    // First check for valid floats.
    if (!_finite(coord))
        return false;

    // Skia uses 16.16 fixed point and 26.6 fixed point in various places. If
    // the transformed point exceeds 15 bits, we just declare that it's
    // unreasonable to catch both of these cases.
    static const int maxPointMagnitude = 32767;
    if (coord > maxPointMagnitude || coord < -maxPointMagnitude)
        return false;

    return true;
#else
    return true;
#endif
}

static bool IsPointReasonable(const SkMatrix& transform, const SkPoint& pt)
{
#ifdef CHECK_REASONABLE
    // Now check for points that will overflow. We check the *transformed*
    // points since this is what will be rasterized.
    SkPoint xPt;
    transform.mapPoints(&xPt, &pt, 1);
    return IsCoordinateReasonable(xPt.fX) && IsCoordinateReasonable(xPt.fY);
#else
    return true;
#endif
}

static bool IsRectReasonable(const SkMatrix& transform, const SkRect& rc)
{
#ifdef CHECK_REASONABLE
    SkPoint topleft = {rc.fLeft, rc.fTop};
    SkPoint bottomright = {rc.fRight, rc.fBottom};
    return IsPointReasonable(transform, topleft) &&
           IsPointReasonable(transform, bottomright);
#else
    return true;
#endif
}

bool IsPathReasonable(const SkMatrix& transform, const SkPath& path)
{
#ifdef CHECK_REASONABLE
    SkPoint current_points[4];
    SkPath::Iter iter(path, false);
    for (SkPath::Verb verb = iter.next(current_points);
         verb != SkPath::kDone_Verb;
         verb = iter.next(current_points)) {
        switch (verb) {
        case SkPath::kMove_Verb:
            // This move will be duplicated in the next verb, so we can ignore it.
            break;
        case SkPath::kLine_Verb:
            // iter.next returns 2 points
            if (!IsPointReasonable(transform, current_points[0]) ||
                !IsPointReasonable(transform, current_points[1]))
                return false;
            break;
        case SkPath::kQuad_Verb:
            // iter.next returns 3 points
            if (!IsPointReasonable(transform, current_points[0]) ||
                !IsPointReasonable(transform, current_points[1]) ||
                !IsPointReasonable(transform, current_points[2]))
                return false;
            break;
        case SkPath::kCubic_Verb:
            // iter.next returns 4 points
            if (!IsPointReasonable(transform, current_points[0]) ||
                !IsPointReasonable(transform, current_points[1]) ||
                !IsPointReasonable(transform, current_points[2]) ||
                !IsPointReasonable(transform, current_points[3]))
                return false;
            break;
        case SkPath::kClose_Verb:
        case SkPath::kDone_Verb:
        default:
            break;
        }
    }
    return true;
#else
    return true;
#endif
}

// Local helper functions ------------------------------------------------------

void add_corner_arc(SkPath* path, const SkRect& rect, const IntSize& size, int startAngle)
{
    SkIRect     ir;
    int         rx = SkMin32(SkScalarRound(rect.width()), size.width());
    int         ry = SkMin32(SkScalarRound(rect.height()), size.height());
    
    ir.set(-rx, -ry, rx, ry);
    switch (startAngle) {
        case   0: ir.offset(rect.fRight - ir.fRight, rect.fBottom - ir.fBottom); break;
        case  90: ir.offset(rect.fLeft - ir.fLeft,      rect.fBottom - ir.fBottom); break;
        case 180: ir.offset(rect.fLeft - ir.fLeft,      rect.fTop - ir.fTop); break;
        case 270: ir.offset(rect.fRight - ir.fRight, rect.fTop - ir.fTop); break;
        default: SkASSERT(!"unexpected startAngle in add_corner_arc");
    }

    SkRect r;
    r.set(ir);
    path->arcTo(r, SkIntToScalar(startAngle), SkIntToScalar(90), false);
}

U8CPU F2B(float x)
{
    return (int)(x * 255);
}

SkColor make_color(float a, float r, float g, float b)
{
    return SkColorSetARGB(F2B(a), F2B(r), F2B(g), F2B(b));
}

// Determine the total number of stops needed, including pseudo-stops at the
// ends as necessary.
size_t total_stops_needed(const WebCore::CanvasGradient::ColorStop* stopData,
                          size_t count)
{
    const WebCore::CanvasGradient::ColorStop* stop = stopData;
    size_t count_used = count;
    if (count < 1 || stop->stop > 0.0)
      count_used++;
    stop += count - 1;
    if (count < 2 || stop->stop < 1.0)
      count_used++;
    return count_used;
}

// Collect sorted stop position and color information into the pos and colors 
// buffers, ensuring stops at both 0.0 and 1.0.  The buffers must be large
// enough to hold information for all stops, including the new endpoints if
// stops at 0.0 and 1.0 aren't already included.
void fill_stops(const WebCore::CanvasGradient::ColorStop* stopData,
                size_t count, SkScalar* pos, SkColor* colors)
{ 
    const WebCore::CanvasGradient::ColorStop* stop = stopData;
    size_t start = 0;
    if (count < 1) {
        // A gradient with no stops must be transparent black.
        pos[0] = WebCoreFloatToSkScalar(0.0);
        colors[0] = make_color(0.0, 0.0, 0.0, 0.0);
        start = 1;
    } else if (stop->stop > 0.0) {
        // Copy the first stop to 0.0. The first stop position may have a slight
        // rounding error, but we don't care in this float comparison, since
        // 0.0 comes through cleanly and people aren't likely to want a gradient
        // with a stop at (0 + epsilon).
        pos[0] = WebCoreFloatToSkScalar(0.0);
        colors[0] = make_color(stop->alpha, stop->red, stop->green, stop->blue);
        start = 1;
    }

    for (size_t i = start; i < start + count; i++)
    {
        pos[i] = WebCoreFloatToSkScalar(stop->stop);
        colors[i] = make_color(stop->alpha, stop->red, stop->green, stop->blue);
        ++stop;
    }

    // Copy the last stop to 1.0 if needed.  See comment above about this float
    // comparison.
    if (count < 1 || (--stop)->stop < 1.0) {
        pos[start + count] = WebCoreFloatToSkScalar(1.0);
        colors[start + count] = colors[start + count - 1];
    }
}

COMPILE_ASSERT(GraphicsContextPlatformPrivate::NoStroke == NoStroke, AssertNoStroke);
COMPILE_ASSERT(GraphicsContextPlatformPrivate::SolidStroke == SolidStroke, AssertSolidStroke);
COMPILE_ASSERT(GraphicsContextPlatformPrivate::DottedStroke == DottedStroke, AssertDottedStroke);
COMPILE_ASSERT(GraphicsContextPlatformPrivate::DashedStroke == DashedStroke, AssertDashedStroke);

// Note: Remove this function as soon as StrokeStyle is moved in GraphicsTypes.h.
GraphicsContextPlatformPrivate::StrokeStyle StrokeStyle2StrokeStyle(StrokeStyle style)
{
    return static_cast<GraphicsContextPlatformPrivate::StrokeStyle>(style);
}

}

////////////////////////////////////////////////////////////////////////////////////////////////

// This may be called with a NULL pointer to create a graphics context that has
// no painting.
GraphicsContext::GraphicsContext(PlatformGraphicsContext *gc)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate(PlatformContextToPlatformContextSkia(gc)))
{
    setPaintingDisabled(!m_data->canvas());
}

GraphicsContext::~GraphicsContext()
{
    delete m_data;
    this->destroyGraphicsContextPrivate(m_common);
}

void GraphicsContext::savePlatformState()
{
    // Save our private State.
    m_data->save();
}

void GraphicsContext::restorePlatformState()
{
    // Restore our private State.
    m_data->restore();
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r;
    WebCoreRectToSkiaRect(rect, &r);
    if (!IsRectReasonable(m_data->canvas()->getTotalMatrix(), r)) {
        // See the fillRect below.
        ClipRectToCanvas(*m_data->canvas(), r, &r);
    }

    m_data->drawRect(r);
}
  
// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    StrokeStyle penStyle = strokeStyle();
    if (penStyle == NoStroke)
        return;

    SkPaint paint;
    SkPoint pts[2];

    WebCorePointToSkiaPoint(point1, &pts[0]);
    WebCorePointToSkiaPoint(point2, &pts[1]);
    if (!IsPointReasonable(m_data->canvas()->getTotalMatrix(), pts[0]) ||
        !IsPointReasonable(m_data->canvas()->getTotalMatrix(), pts[1]))
        return;

    //we know these are vertical or horizontal lines, so the length will just be the sum of the 
    //displacement component vectors give or take 1 - probably worth the speed up of no square 
    //root, which also won't be exact
    SkPoint disp = pts[1]-pts[0];
    int length = SkScalarRound(disp.fX+disp.fY);
    //int length = SkScalarRound(disp.length());
    int width = m_data->setup_paint_stroke(&paint, NULL, length);
    
    // "borrowed" this comment and idea from GraphicsContextCG.cpp
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
   
    bool isVerticalLine = pts[0].fX == pts[1].fX;
    
    if (width & 1) //odd
    {
        if (isVerticalLine)
        {
            pts[0].fX = pts[0].fX + SK_ScalarHalf;
            pts[1].fX = pts[0].fX;
        }
        else                        //Horizontal line
        {
            pts[0].fY = pts[0].fY + SK_ScalarHalf;
            pts[1].fY = pts[0].fY;
        }
    }
    m_data->canvas()->drawPoints(SkCanvas::kLines_PointMode, 2, pts, paint);
}

static void setrect_for_underline(SkRect* r, GraphicsContext* context, const IntPoint& point, int width)
{
    int lineThickness = SkMax32(static_cast<int>(context->strokeThickness()), 1);
    
    r->fLeft    = SkIntToScalar(point.x());
    r->fTop     = SkIntToScalar(point.y());
    r->fRight   = r->fLeft + SkIntToScalar(width);
    r->fBottom  = r->fTop + SkIntToScalar(lineThickness);
}

void GraphicsContext::drawLineForText(const IntPoint& pt, int width, bool printing)
{
    if (paintingDisabled())
        return;

    SkRect  r;
    SkPaint paint;

    setrect_for_underline(&r, this, pt, width);
    paint.setColor(this->strokeColor().rgb());
    m_data->canvas()->drawRect(r, paint);
}

void GraphicsContext::drawLineForMisspellingOrBadGrammar(const IntPoint& pt,
                                                         int width,
                                                         bool grammar)
{
    if (paintingDisabled())
        return;

    // Create the pattern we'll use to draw the underline.
    static SkBitmap* misspell_bitmap = NULL;
    if (!misspell_bitmap) {
        // We use a 2-pixel-high misspelling indicator because that seems to be
        // what WebKit is designed for, and how much room there is in a typical
        // page for it.
        const int row_pixels = 32;  // Must be multiple of 4 for pattern below.
        const int col_pixels = 2;
        misspell_bitmap = new SkBitmap;
        misspell_bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                                   row_pixels, col_pixels);
        misspell_bitmap->allocPixels();

        misspell_bitmap->eraseARGB(0, 0, 0, 0);
        const uint32_t line_color = 0xFFFF0000;  // Opaque red.
        const uint32_t anti_color = 0x60600000;  // Semitransparent red.

        // Pattern:  X o   o X o   o X
        //             o X o   o X o
        uint32_t* row1 = misspell_bitmap->getAddr32(0, 0);
        uint32_t* row2 = misspell_bitmap->getAddr32(0, 1);
        for (int x = 0; x < row_pixels; x ++) {
            switch (x % 4) {
            case 0:
                row1[x] = line_color;
                break;
            case 1:
                row1[x] = anti_color;
                row2[x] = anti_color;
                break;
            case 2:
                row2[x] = line_color;
                break;
            case 3:
                row1[x] = anti_color;
                row2[x] = anti_color;
                break;
            }
        }
    }

    // Offset it vertically by 1 so that there's some space under the text.
    SkScalar origin_x = SkIntToScalar(pt.x());
    SkScalar origin_y = SkIntToScalar(pt.y()) + 1;

    // Make a shader for the bitmap with an origin of the box we'll draw. This
    // shader is refcounted and will have an initial refcount of 1.
    SkShader* shader = SkShader::CreateBitmapShader(
        *misspell_bitmap, SkShader::kRepeat_TileMode,
        SkShader::kRepeat_TileMode);
    SkMatrix matrix;
    matrix.reset();
    matrix.postTranslate(origin_x, origin_y);
    shader->setLocalMatrix(matrix);

    // Assign the shader to the paint & release our reference. The paint will
    // now own the shader and the shader will be destroyed when the paint goes
    // out of scope.
    SkPaint paint;
    paint.setShader(shader);
    shader->unref();

    SkRect rect;
    rect.set(origin_x,
             origin_y,
             origin_x + SkIntToScalar(width),
             origin_y + SkIntToScalar(misspell_bitmap->height()));
    m_data->canvas()->drawRect(rect, paint);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    SkRect  oval;
    
    WebCoreRectToSkiaRect(rect, &oval);
    if (!IsRectReasonable(m_data->canvas()->getTotalMatrix(), oval))
        return;

    if (fillColor().rgb() & 0xFF000000) {
        m_data->setup_paint_fill(&paint);
        m_data->canvas()->drawOval(oval, paint);
    }
    if (strokeStyle() != NoStroke) {
        paint.reset();
        m_data->setup_paint_stroke(&paint, &oval, 0);
        m_data->canvas()->drawOval(oval, paint);
    }
}

static inline int fast_mod(int value, int max)
{
    int sign = SkExtractSign(value);

    value = SkApplySign(value, sign);
    if (value >= max) {
        value %= max;
    }
    return SkApplySign(value, sign);
}

void GraphicsContext::strokeArc(const IntRect& r, int startAngle, int angleSpan)
{
    if (paintingDisabled())
        return;

    SkPath  path;
    SkPaint paint;
    SkRect  oval;
    
    WebCoreRectToSkiaRect(r, &oval);

    if (strokeStyle() == NoStroke) {
        m_data->setup_paint_fill(&paint);   // we want the fill color
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(WebCoreFloatToSkScalar(strokeThickness()));
    }
    else {
        m_data->setup_paint_stroke(&paint, NULL, 0);
    }

    // we do this before converting to scalar, so we don't overflow SkFixed
    startAngle = fast_mod(startAngle, 360);
    angleSpan = fast_mod(angleSpan, 360);

    path.addArc(oval, SkIntToScalar(-startAngle), SkIntToScalar(-angleSpan));    
    if (!IsPathReasonable(m_data->canvas()->getTotalMatrix(), path))
        return;
    m_data->canvas()->drawPath(path, paint);
}

void GraphicsContext::drawConvexPolygon(size_t numPoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    SkPaint paint;
    SkPath  path;

    path.incReserve(numPoints);
    path.moveTo(WebCoreFloatToSkScalar(points[0].x()), WebCoreFloatToSkScalar(points[0].y()));
    for (size_t i = 1; i < numPoints; i++)
        path.lineTo(WebCoreFloatToSkScalar(points[i].x()), WebCoreFloatToSkScalar(points[i].y()));

    if (!IsPathReasonable(m_data->canvas()->getTotalMatrix(), path))
        return;

    if (fillColor().rgb() & 0xFF000000) {
        m_data->setup_paint_fill(&paint);
        m_data->canvas()->drawPath(path, paint);
    }

    if (strokeStyle() != NoStroke) {
        paint.reset();
        m_data->setup_paint_stroke(&paint, NULL, 0);
        m_data->canvas()->drawPath(path, paint);
    }
}

#ifdef ANDROID_CANVAS_IMPL

static void check_set_shader(SkPaint* paint, SkShader* s0, SkShader* s1)
{
    if (s0) {
        paint->setShader(s0);
    }
    else if (s1) {
        paint->setShader(s1);
    }
}


void GraphicsContext::fillPath(const Path& webCorePath, PlatformGradient* grad, PlatformPattern* pat)
{
    fillPath(PathToSkPath(webCorePath), grad, pat);
}

void GraphicsContext::fillPath(PlatformPath* path, PlatformGradient* grad, PlatformPattern* pat)
{
    if (paintingDisabled())
        return;
    if (!IsPathReasonable(m_data->canvas()->getTotalMatrix(), *path))
      return;

    SkPaint paint;

    m_data->setup_paint_fill(&paint);
    check_set_shader(&paint, grad, pat);

    m_data->canvas()->drawPath(*path, paint);
}


void GraphicsContext::strokePath(const Path& webCorePath, PlatformGradient* grad, PlatformPattern* pat)
{
    strokePath(PathToSkPath(webCorePath), grad, pat);
}

void GraphicsContext::strokePath(PlatformPath* path, PlatformGradient* grad, PlatformPattern* pat)
{
    if (paintingDisabled())
        return;
    if (!IsPathReasonable(m_data->canvas()->getTotalMatrix(), *path))
      return;

    SkPaint paint;

    m_data->setup_paint_stroke(&paint, NULL, 0);
    check_set_shader(&paint, grad, pat);

    m_data->canvas()->drawPath(*path, paint);
}

void GraphicsContext::fillRect(const FloatRect& rect, PlatformGradient* grad, PlatformPattern* pat)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    m_data->setup_paint_fill(&paint);
    check_set_shader(&paint, grad, pat);

    SkRect r;
    WebCoreRectToSkiaRect(rect, &r);
    if (!IsRectReasonable(m_data->canvas()->getTotalMatrix(), r)) {
        // See the other version of fillRect below.
        ClipRectToCanvas(*m_data->canvas(), r, &r);
    }

    m_data->canvas()->drawRect(r, paint);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth, PlatformGradient* grad, PlatformPattern* pat)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    m_data->setup_paint_stroke(&paint, NULL, 0);
    paint.setStrokeWidth(WebCoreFloatToSkScalar(lineWidth));
    check_set_shader(&paint, grad, pat);

    SkRect r;
    WebCoreRectToSkiaRect(rect, &r);
    if (!IsRectReasonable(m_data->canvas()->getTotalMatrix(), r))
        return;

    m_data->canvas()->drawRect(r, paint);
}

PlatformGradient* GraphicsContext::newPlatformLinearGradient(const FloatPoint& p0,
    const FloatPoint& p1,
    const WebCore::CanvasGradient::ColorStop* stopData, size_t count)
{
    SkPoint pts[2];
    WebCorePointToSkiaPoint(p0, &pts[0]);
    WebCorePointToSkiaPoint(p1, &pts[1]);

    size_t count_used = total_stops_needed(stopData, count);
    ASSERT(count_used >= 2);
    ASSERT(count_used >= count);
    
    SkAutoMalloc    storage(count_used * (sizeof(SkColor) + sizeof(SkScalar)));
    SkColor*        colors = (SkColor*)storage.get();
    SkScalar*       pos = (SkScalar*)(colors + count_used);

    fill_stops(stopData, count, pos, colors);
    return SkGradientShader::CreateLinear(pts, colors, pos,
                                          static_cast<int>(count_used),
                                          SkShader::kClamp_TileMode);
}

PlatformGradient* GraphicsContext::newPlatformRadialGradient(const FloatPoint& p0, float r0,
    const FloatPoint& p1, float r1,
    const WebCore::CanvasGradient::ColorStop* stopData, size_t count)
{
    SkPoint center;
    WebCorePointToSkiaPoint(p1, &center);
    SkMatrix identity;
    identity.reset();
    if (!IsPointReasonable(identity, center)) {
        center.fX = 0;
        center.fY = 0;
    }

    size_t count_used = total_stops_needed(stopData, count);
    ASSERT(count_used >= 2);
    ASSERT(count_used >= count);
    
    SkAutoMalloc    storage(count_used * (sizeof(SkColor) + sizeof(SkScalar)));
    SkColor*        colors = (SkColor*)storage.get();
    SkScalar*       pos = (SkScalar*)(colors + count_used);
    
    fill_stops(stopData, count, pos, colors);
    return SkGradientShader::CreateRadial(center, WebCoreFloatToSkScalar(r1),
                                          colors, pos,
                                          static_cast<int>(count_used),
                                          SkShader::kClamp_TileMode);
}

void GraphicsContext::freePlatformGradient(PlatformGradient* shader)
{
    shader->safeUnref();
}

PlatformPattern* GraphicsContext::newPlatformPattern(Image* image,
                                                     Image::TileRule hRule,
                                                     Image::TileRule vRule)
{
    if (NULL == image)
        return NULL;

    NativeImageSkia* bm = image->getBitmap();
    if (NULL == bm)
        return NULL;

    return SkShader::CreateBitmapShader(*bm,
                                        WebCoreTileToSkiaTile(hRule),
                                        WebCoreTileToSkiaTile(vRule));
}

void GraphicsContext::freePlatformPattern(PlatformPattern* shader)
{
    shader->safeUnref();
}

GraphicsContext* GraphicsContext::createOffscreenContext(int width, int height)
{
    gfx::PlatformCanvasWin* canvas = new gfx::PlatformCanvasWin(width, height, false);
    PlatformContextSkia* pgc = new PlatformContextSkia(canvas);
    canvas->drawARGB(0, 0, 0, 0, SkPorterDuff::kClear_Mode);

    // Ensure that the PlatformContextSkia deletes the PlatformCanvas.
    pgc->setShouldDelete(true);

    GraphicsContext* gc =
        new GraphicsContext(reinterpret_cast<PlatformGraphicsContext*>(pgc));

    // Ensure that the GraphicsContext deletes the PlatformContextSkia.
    gc->setShouldDelete(true);

    // The caller is responsible for deleting this pointer.
    return gc;
}

void GraphicsContext::drawOffscreenContext(GraphicsContext* ctx, const FloatRect* srcRect, const FloatRect& dstRect)
{
    if (paintingDisabled() || ctx->paintingDisabled())
        return;

    const SkBitmap& bm = ctx->m_data->canvas()->getDevice()->accessBitmap(false);
    SkIRect         src;
    SkRect          dst;
    SkPaint         paint;
    if (srcRect) {
        WebCoreRectToSkiaRect(*srcRect, &src);
        // FIXME(brettw) FIX THIS YOU RETARD!
    }

    WebCoreRectToSkiaRect(dstRect, &dst);
    if (!IsRectReasonable(m_data->canvas()->getTotalMatrix(), dst))
        return;

    paint.setFilterBitmap(true);

    m_data->canvas()->drawBitmapRect(bm,
                                     srcRect ? &src : NULL,
                                     dst,
                                     &paint);
}

FloatRect GraphicsContext::getClipLocalBounds() const
{
    SkRect r;

    if (!m_data->canvas()->getClipBounds(&r))
        r.setEmpty();

    return FloatRect(SkScalarToFloat(r.fLeft), SkScalarToFloat(r.fTop),
                     SkScalarToFloat(r.width()), SkScalarToFloat(r.height()));
}

FloatRect GraphicsContext::getPathBoundingBox(const Path& path) const
{
    SkRect r;
    SkPaint paint;
    m_data->setup_paint_stroke(&paint, NULL, 0);

    SkPath boundingPath;
    paint.getFillPath(*path.platformPath(), &boundingPath);

    boundingPath.computeBounds(&r, SkPath::kExact_BoundsType);

    return FloatRect(
        SkScalarToFloat(r.fLeft),
        SkScalarToFloat(r.fTop),
        SkScalarToFloat(r.width()),
        SkScalarToFloat(r.height()));
}


bool GraphicsContext::strokeContains(const Path& path, const FloatPoint& point) const
{
    SkRegion rgn, clip;
    SkPaint paint;

    m_data->setup_paint_stroke(&paint, NULL, 0);

    SkPath strokePath;
    paint.getFillPath(*path.platformPath(), &strokePath);

    return SkPathContainsPoint(&strokePath, point, SkPath::kWinding_FillType);
}


#endif  // ANDROID_CANVAS_IMPL

#if 0
static int getBlendedColorComponent(int c, int a)
{
    // We use white.
    float alpha = (float)(a) / 255;
    int whiteBlend = 255 - a;
    c -= whiteBlend;
    return (int)(c/alpha);
}
#endif

void GraphicsContext::fillRect(const IntRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    if (color.rgb() & 0xFF000000) {
        SkPaint paint;
        SkRect  r;

        WebCoreRectToSkiaRect(rect, &r);

        static const float kMaxCoord = 32767;
        if (!IsRectReasonable(m_data->canvas()->getTotalMatrix(), r)) {
            // Special case when the rectangle overflows fixed point. This is a
            // workaround to fix bug 1212844. When the input rectangle is very
            // large, it can overflow Skia's internal fixed point rect. This
            // should be fixable in Skia (since the output bitmap isn't that
            // large), but until that is fixed, we try to handle it ourselves.
            //
            // We manually clip the rectangle to the current clip rect. This
            // will prevent overflow. The rectangle will be transformed to the
            // canvas' coordinate space before it is converted to fixed point
            // so we are guaranteed not to overflow after doing this.
            ClipRectToCanvas(*m_data->canvas(), r, &r);
        }

        m_data->setup_paint_common(&paint);
        paint.setColor(color.rgb());
        m_data->canvas()->drawRect(r, paint);
    }
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    if (color.rgb() & 0xFF000000) {
        SkPaint paint;
        SkRect  r;
        
        WebCoreRectToSkiaRect(rect, &r);
        if (!IsRectReasonable(m_data->canvas()->getTotalMatrix(), r)) {
            // See other fillRect() above.
            ClipRectToCanvas(*m_data->canvas(), r, &r);
        }

        m_data->setup_paint_common(&paint);
        paint.setColor(color.rgb());
        m_data->canvas()->drawRect(r, paint);
    }
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
                                      const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color)
{
    if (paintingDisabled())
        return;

    SkRect r;
    WebCoreRectToSkiaRect(rect, &r);
    if (!IsRectReasonable(m_data->canvas()->getTotalMatrix(), r)) {
        // See fillRect().
        ClipRectToCanvas(*m_data->canvas(), r, &r);
    }

    SkPaint paint;
    SkPath  path;

    add_corner_arc(&path, r, topRight, 270);
    add_corner_arc(&path, r, bottomRight, 0);
    add_corner_arc(&path, r, bottomLeft, 90);
    add_corner_arc(&path, r, topLeft, 180);

    m_data->setup_paint_fill(&paint);
    m_data->canvas()->drawPath(path, paint);
    return fillRect(rect, color);
}

void GraphicsContext::clip(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect  r;
    WebCoreRectToSkiaRect(rect, &r);
    if (!IsRectReasonable(m_data->canvas()->getTotalMatrix(), r))
        return;

    m_data->canvas()->clipRect(r);
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;

    const SkPath& p = *PathToSkPath(path);
    if (!IsPathReasonable(m_data->canvas()->getTotalMatrix(), p))
        return;

    m_data->canvas()->clipPath(p);
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    if (paintingDisabled())
        return;
    SkPath  path;
    SkRect r;
    WebCoreRectToSkiaRect(rect, &r);
    if (!IsRectReasonable(m_data->canvas()->getTotalMatrix(), r))
        return;

    path.addOval(r, SkPath::kCW_Direction);
    // only perform the inset if we won't invert r
    if (2*thickness < rect.width() && 2*thickness < rect.height())
    {
        r.inset(SkIntToScalar(thickness) ,SkIntToScalar(thickness));
        path.addOval(r, SkPath::kCCW_Direction);
    }
    m_data->canvas()->clipPath(path);
}


void GraphicsContext::clipOut(const IntRect& rect)
{
    if (paintingDisabled())
        return;
    SkRect  r;
    WebCoreRectToSkiaRect(rect, &r);
    if (!IsRectReasonable(m_data->canvas()->getTotalMatrix(), r))
        return;

    m_data->canvas()->clipRect(r, SkRegion::kDifference_Op);
}

void GraphicsContext::clipOutEllipseInRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;
    SkRect  oval;
    SkPath  path;

    WebCoreRectToSkiaRect(rect, &oval);
    if (!IsRectReasonable(m_data->canvas()->getTotalMatrix(), oval))
        return;

    path.addOval(oval, SkPath::kCCW_Direction);
    m_data->canvas()->clipPath(path, SkRegion::kDifference_Op);
}

void GraphicsContext::clipOut(const Path& p)
{
    if (paintingDisabled())
        return;

    const SkPath& path = *PathToSkPath(p);
    if (!IsPathReasonable(m_data->canvas()->getTotalMatrix(), path))
        return;
    m_data->canvas()->clipPath(path, SkRegion::kDifference_Op);
}

#if SVG_SUPPORT
KRenderingDeviceContext* GraphicsContext::createRenderingDeviceContext()
{
    return new KRenderingDeviceContextQuartz(platformContext());
}
#endif

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    SkDevice* old_device = m_data->canvas()->getDevice();
    const SkBitmap& old_bitmap = old_device->accessBitmap(false);

    // We need the "alpha" layer flag here because the base layer is opaque
    // (the surface of the page) but layers on top may have transparent parts.
    // Without explicitly setting the alpha flag, the layer will inherit the
    // opaque setting of the base and some things won't work properly.
    m_data->canvas()->saveLayerAlpha(
        NULL,
        static_cast<unsigned char>(opacity * 255),
        static_cast<SkCanvas::SaveFlags>(SkCanvas::kHasAlphaLayer_SaveFlag |
                                         SkCanvas::kFullColorLayer_SaveFlag));
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    m_data->canvas()->getTopPlatformDevice().fixupAlphaBeforeCompositing();
    m_data->canvas()->restore();
}

void GraphicsContext::setShadow(const IntSize& size, int blur, const Color& color)
{
    if (paintingDisabled())
        return;

    if (blur > 0)
    {
        SkColor c;
        
        if (color.isValid())
            c = color.rgb();
        else
            SkColorSetARGB(0xFF/3, 0, 0, 0);    // "std" apple shadow color
        
        SkDrawLooper* dl = new SkBlurDrawLooper(SkIntToScalar(blur),
                                                SkIntToScalar(size.width()),
                                                SkIntToScalar(size.height()),
                                                c);
        m_data->setDrawLooper(dl)->unref();
    }
    else
        m_data->setDrawLooper(NULL);
}

void GraphicsContext::clearShadow()
{
    if (paintingDisabled())
        return;

    m_data->setDrawLooper(NULL);
}

void GraphicsContext::drawFocusRing(const Color& color)
{
    if (paintingDisabled())
        return;
    const Vector<IntRect>&  rects = focusRingRects();
    unsigned                rectCount = rects.size();
    if (0 == rectCount)
        return;

    SkRegion exterior_region;
    const SkScalar exterior_offset = WebCoreFloatToSkScalar(0.5);
    for (unsigned i = 0; i < rectCount; i++)
    {
        SkIRect r;
        WebCoreRectToSkiaRect(rects[i], &r);
        r.inset(-exterior_offset, -exterior_offset);
        exterior_region.op(r, SkRegion::kUnion_Op);
    }

    SkPath path;
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);

    paint.setARGB(255, 229, 151, 0);
    paint.setStrokeWidth(exterior_offset * 2);
    paint.setPathEffect(new SkCornerPathEffect(exterior_offset * 2))->unref();
    exterior_region.getBoundaryPath(&path);
    m_data->canvas()->drawPath(path, paint);
}

PlatformGraphicsContext* GraphicsContext::platformContext() const
{
    ASSERT(!paintingDisabled());
    return reinterpret_cast<PlatformGraphicsContext*>(m_data->platformContext());
}

void GraphicsContext::setMiterLimit(float limit)
{
    m_data->setMiterLimit(limit);
}

void GraphicsContext::setAlpha(float alpha)
{
    m_data->setAlpha(alpha);
}

void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    m_data->setPorterDuffMode(WebCoreCompositeToSkiaComposite(op));
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    SkRect  r;
    WebCoreRectToSkiaRect(rect, &r);
    if (!IsRectReasonable(m_data->canvas()->getTotalMatrix(), r))
        ClipRectToCanvas(*m_data->canvas(), r, &r);

    m_data->setup_paint_fill(&paint);
    paint.setPorterDuffXfermode(SkPorterDuff::kClear_Mode);
    m_data->canvas()->drawRect(r, paint);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    SkRect  r;
    
    WebCoreRectToSkiaRect(rect, &r);
    if (!IsRectReasonable(m_data->canvas()->getTotalMatrix(), r))
        return;

    m_data->setup_paint_fill(&paint);
    paint.setStrokeWidth(WebCoreFloatToSkScalar(lineWidth));
    m_data->canvas()->drawRect(r, paint);
}

void GraphicsContext::setLineCap(LineCap cap)
{
    switch (cap) {
    case ButtCap:
        m_data->setLineCap(SkPaint::kButt_Cap);
        break;
    case RoundCap:
        m_data->setLineCap(SkPaint::kRound_Cap);
        break;
    case SquareCap:
        m_data->setLineCap(SkPaint::kSquare_Cap);
        break;
    default:
        SkDEBUGF(("GraphicsContext::setLineCap: unknown LineCap %d\n", cap));
        break;
    }
}

void GraphicsContext::setLineJoin(LineJoin join)
{
    switch (join) {
    case MiterJoin:
        m_data->setLineJoin(SkPaint::kMiter_Join);
        break;
    case RoundJoin:
        m_data->setLineJoin(SkPaint::kRound_Join);
        break;
    case BevelJoin:
        m_data->setLineJoin(SkPaint::kBevel_Join);
        break;
    default:
        SkDEBUGF(("GraphicsContext::setLineJoin: unknown LineJoin %d\n", join));
        break;
    }
}

void GraphicsContext::setFillRule(WindRule rule)
{
    switch (rule) {
    case RULE_NONZERO:
        m_data->setFillRule(SkPath::kWinding_FillType);
        break;
    case RULE_EVENODD:
        m_data->setFillRule(SkPath::kEvenOdd_FillType);
        break;
    default:
        SkDEBUGF(("GraphicsContext::setFillRule: unknown WindRule %d\n", rule));
        break;
    }
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;
    m_data->canvas()->scale(WebCoreFloatToSkScalar(size.width()), WebCoreFloatToSkScalar(size.height()));
}

void GraphicsContext::rotate(float angleInRadians)
{
    if (paintingDisabled())
        return;
    m_data->canvas()->rotate(WebCoreFloatToSkScalar(angleInRadians * (180.0f / 3.14159265f)));
}

void GraphicsContext::translate(float w, float h)
{
    if (paintingDisabled())
        return;
    m_data->canvas()->translate(WebCoreFloatToSkScalar(w), WebCoreFloatToSkScalar(h));
}

void GraphicsContext::concatCTM(const AffineTransform& xform)
{
    m_data->canvas()->concat(xform);
}

AffineTransform GraphicsContext::getCTM() const
{
    return m_data->canvas()->getTotalMatrix();
}


HDC GraphicsContext::getWindowsContext(bool supportAlphaBlend, const IntRect*) {
    if (paintingDisabled())
        return NULL;
    // No need to ever call endPlatformPaint() since it is a noop.
    return m_data->canvas()->beginPlatformPaint();
}

void GraphicsContext::releaseWindowsContext(HDC hdc, bool supportAlphaBlend, const IntRect*) {
    // noop, the DC will be lazily freed by the bitmap when no longer needed
}

static inline float square(float n)
{
    return n * n;
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect)
{
    // This logic is copied from GraphicsContextCG, eseidel 5/05/08

    // It is not enough just to round to pixels in device space. The rotation
    // part of the affine transform matrix to device space can mess with this
    // conversion if we have a rotating image like the hands of the world clock
    // widget. We just need the scale, so we get the affine transform matrix and
    // extract the scale.

    const SkMatrix& deviceMatrix = m_data->canvas()->getTotalMatrix();
    if (deviceMatrix.isIdentity())
        return rect;

    float deviceScaleX = sqrtf(square(deviceMatrix.getScaleX())
        + square(deviceMatrix.getSkewY()));
    float deviceScaleY = sqrtf(square(deviceMatrix.getSkewX())
        + square(deviceMatrix.getScaleY()));

    FloatPoint deviceOrigin(rect.x() * deviceScaleX, rect.y() * deviceScaleY);
    FloatPoint deviceLowerRight((rect.x() + rect.width()) * deviceScaleX,
        (rect.y() + rect.height()) * deviceScaleY);

    deviceOrigin.setX(roundf(deviceOrigin.x()));
    deviceOrigin.setY(roundf(deviceOrigin.y()));
    deviceLowerRight.setX(roundf(deviceLowerRight.x()));
    deviceLowerRight.setY(roundf(deviceLowerRight.y()));
    
    // Don't let the height or width round to 0 unless either was originally 0
    if (deviceOrigin.y() == deviceLowerRight.y() && rect.height() != 0)
        deviceLowerRight.move(0, 1);
    if (deviceOrigin.x() == deviceLowerRight.x() && rect.width() != 0)
        deviceLowerRight.move(1, 0);

    FloatPoint roundedOrigin(deviceOrigin.x() / deviceScaleX,
        deviceOrigin.y() / deviceScaleY);
    FloatPoint roundedLowerRight(deviceLowerRight.x() / deviceScaleX,
        deviceLowerRight.y() / deviceScaleY);
    return FloatRect(roundedOrigin, roundedLowerRight - roundedOrigin);
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
}

void GraphicsContext::setPlatformFillColor(const Color& color)
{
    m_data->setFillColor(color.rgb());
}

void GraphicsContext::setPlatformStrokeStyle(const StrokeStyle & strokestyle)
{
    m_data->setStrokeStyle(StrokeStyle2StrokeStyle(strokestyle));
}

void GraphicsContext::setPlatformStrokeColor(const Color& strokecolor)
{
    m_data->setStrokeColor(strokecolor.rgb());
}

void GraphicsContext::setPlatformStrokeThickness(float thickness)
{
    m_data->setStrokeThickness(thickness);
}

void GraphicsContext::addPath(const Path& path)
{
    m_data->addPath(*PathToSkPath(path));
}

void GraphicsContext::beginPath()
{
    m_data->beginPath();
}

void GraphicsContext::setUseAntialiasing(bool enable)
{
    if (paintingDisabled())
        return;
    notImplemented();
}

void GraphicsContext::setShouldDelete(bool should_delete)
{
    if (m_data)
        m_data->setShouldDelete(should_delete);
}

PlatformPath* GraphicsContext::currentPath()
{
    return m_data->currentPath();
}

}
