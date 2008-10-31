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
#include "GraphicsContextPlatformPrivate.h"
#include "GraphicsContextPrivate.h"
#include "wtf/MathExtras.h"

#include "Assertions.h"
#include "AffineTransform.h"
#include "Color.h"
#include "FloatRect.h"
#include "Gradient.h"
#include "IntRect.h"
#include "NativeImageSkia.h"
#include "NotImplemented.h"
#include "SkBlurDrawLooper.h"
#include "SkCornerPathEffect.h"
#include "SkiaUtils.h"

#include "SkBitmap.h"

#include "base/gfx/platform_canvas.h"

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
#if defined(COMPILER_MSVC)
    if (!_finite(coord))
#else
    if (!finite(coord))
#endif
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

}

////////////////////////////////////////////////////////////////////////////////////////////////

// This may be called with a NULL pointer to create a graphics context that has
// no painting.
GraphicsContext::GraphicsContext(PlatformGraphicsContext *gc)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate(gc))
{
    setPaintingDisabled(!gc || !platformContext()->canvas());
}

GraphicsContext::~GraphicsContext()
{
    delete m_data;
    this->destroyGraphicsContextPrivate(m_common);
}

void GraphicsContext::savePlatformState()
{
    // Save our private State.
    platformContext()->save();
}

void GraphicsContext::restorePlatformState()
{
    // Restore our private State.
    platformContext()->restore();
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;
    if (!IsRectReasonable(getCTM(), r)) {
        // See the fillRect below.
        ClipRectToCanvas(*platformContext()->canvas(), r, &r);
    }

    platformContext()->drawRect(r);
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
    SkPoint pts[2] = { (SkPoint)point1, (SkPoint)point2 };
    if (!IsPointReasonable(getCTM(), pts[0]) ||
        !IsPointReasonable(getCTM(), pts[1]))
        return;

    //we know these are vertical or horizontal lines, so the length will just be the sum of the 
    //displacement component vectors give or take 1 - probably worth the speed up of no square 
    //root, which also won't be exact
    SkPoint disp = pts[1] - pts[0];
    int length = SkScalarRound(disp.fX + disp.fY);
    int width = platformContext()->setup_paint_stroke(&paint, 0, length);
    
    // "borrowed" this comment and idea from GraphicsContextCG.cpp
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
   
    bool isVerticalLine = pts[0].fX == pts[1].fX;
    
    if (width & 1) //odd
    {
        if (isVerticalLine) {
            pts[0].fX = pts[0].fX + SK_ScalarHalf;
            pts[1].fX = pts[0].fX;
        } else                        //Horizontal line
        {
            pts[0].fY = pts[0].fY + SK_ScalarHalf;
            pts[1].fY = pts[0].fY;
        }
    }
    platformContext()->canvas()->drawPoints(SkCanvas::kLines_PointMode, 2, pts, paint);
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
    platformContext()->canvas()->drawRect(r, paint);
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
    platformContext()->canvas()->drawRect(rect, paint);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& elipseRect)
{
    if (paintingDisabled())
        return;

    SkRect rect = elipseRect;
    if (!IsRectReasonable(getCTM(), rect))
        return;

    SkPaint paint;
    if (fillColor().rgb() & 0xFF000000) {
        platformContext()->setup_paint_fill(&paint);
        platformContext()->canvas()->drawOval(rect, paint);
    }
    if (strokeStyle() != NoStroke) {
        paint.reset();
        platformContext()->setup_paint_stroke(&paint, &rect, 0);
        platformContext()->canvas()->drawOval(rect, paint);
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

    SkPaint paint;
    SkRect  oval = r;

    if (strokeStyle() == NoStroke) {
        platformContext()->setup_paint_fill(&paint);   // we want the fill color
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(WebCoreFloatToSkScalar(strokeThickness()));
    } else
        platformContext()->setup_paint_stroke(&paint, NULL, 0);

    // we do this before converting to scalar, so we don't overflow SkFixed
    startAngle = fast_mod(startAngle, 360);
    angleSpan = fast_mod(angleSpan, 360);

    SkPath path;
    path.addArc(oval, SkIntToScalar(-startAngle), SkIntToScalar(-angleSpan));    
    if (!IsPathReasonable(getCTM(), path))
        return;
    platformContext()->canvas()->drawPath(path, paint);
}

void GraphicsContext::drawConvexPolygon(size_t numPoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    SkPath  path;

    path.incReserve(numPoints);
    path.moveTo(WebCoreFloatToSkScalar(points[0].x()), WebCoreFloatToSkScalar(points[0].y()));
    for (size_t i = 1; i < numPoints; i++)
        path.lineTo(WebCoreFloatToSkScalar(points[i].x()), WebCoreFloatToSkScalar(points[i].y()));

    if (!IsPathReasonable(getCTM(), path))
        return;

    SkPaint paint;
    if (fillColor().rgb() & 0xFF000000) {
        platformContext()->setup_paint_fill(&paint);
        platformContext()->canvas()->drawPath(path, paint);
    }

    if (strokeStyle() != NoStroke) {
        paint.reset();
        platformContext()->setup_paint_stroke(&paint, NULL, 0);
        platformContext()->canvas()->drawPath(path, paint);
    }
}

void GraphicsContext::fillPath()
{
    if (paintingDisabled())
        return;
    const SkPath& path = *platformContext()->currentPath();
    if (!IsPathReasonable(getCTM(), path))
      return;

    const GraphicsContextState& state = m_common->state;
    ColorSpace colorSpace = state.fillColorSpace;

    if (colorSpace == SolidColorSpace && !fillColor().alpha())
        return;

    platformContext()->setFillRule(state.fillRule == RULE_EVENODD ?
      SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType);

    SkPaint paint;
    platformContext()->setup_paint_fill(&paint);

    if (colorSpace == PatternColorSpace) {
        SkShader* pat = state.fillPattern->createPlatformPattern(getCTM());
        paint.setShader(pat);
        pat->unref();
    } else if (colorSpace == GradientColorSpace)
        paint.setShader(state.fillGradient->platformGradient());

    platformContext()->canvas()->drawPath(path, paint);
}

void GraphicsContext::strokePath()
{
    if (paintingDisabled())
        return;
    const SkPath& path = *platformContext()->currentPath();
    if (!IsPathReasonable(getCTM(), path))
      return;

    const GraphicsContextState& state = m_common->state;
    ColorSpace colorSpace = state.strokeColorSpace;

    if (colorSpace == SolidColorSpace && !strokeColor().alpha())
        return;

    SkPaint paint;
    platformContext()->setup_paint_stroke(&paint, NULL, 0);

    if (colorSpace == PatternColorSpace) {
        SkShader* pat = state.strokePattern->createPlatformPattern(getCTM());
        paint.setShader(pat);
        pat->unref();
    } else if (colorSpace == GradientColorSpace)
        paint.setShader(state.strokeGradient->platformGradient());

    platformContext()->canvas()->drawPath(path, paint);
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;
    if (!IsRectReasonable(getCTM(), r)) {
        // See the other version of fillRect below.
        ClipRectToCanvas(*platformContext()->canvas(), r, &r);
    }

    const GraphicsContextState& state = m_common->state;
    ColorSpace colorSpace = state.fillColorSpace;

    if (colorSpace == SolidColorSpace && !fillColor().alpha())
        return;

    SkPaint paint;
    platformContext()->setup_paint_fill(&paint);

    if (colorSpace == PatternColorSpace) {
        SkShader* pat = state.fillPattern->createPlatformPattern(getCTM());
        paint.setShader(pat);
        pat->unref();
    } else if (colorSpace == GradientColorSpace)
        paint.setShader(state.fillGradient->platformGradient());

    platformContext()->canvas()->drawRect(r, paint);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (paintingDisabled())
        return;
    if (!IsRectReasonable(getCTM(), rect))
        return;

    const GraphicsContextState& state = m_common->state;
    ColorSpace colorSpace = state.strokeColorSpace;

    if (colorSpace == SolidColorSpace && !strokeColor().alpha())
        return;

    SkPaint paint;
    platformContext()->setup_paint_stroke(&paint, NULL, 0);
    paint.setStrokeWidth(WebCoreFloatToSkScalar(lineWidth));

    if (colorSpace == PatternColorSpace) {
        SkShader* pat = state.strokePattern->createPlatformPattern(getCTM());
        paint.setShader(pat);
        pat->unref();
    } else if (colorSpace == GradientColorSpace)
        paint.setShader(state.strokeGradient->platformGradient());

    platformContext()->canvas()->drawRect(rect, paint);
}

FloatRect GraphicsContext::getBoundingBoxForCurrentPath(bool includeStroke) const
{
    SkPath boundingPath;

    if (includeStroke) {
        SkPaint paint;
        platformContext()->setup_paint_stroke(&paint, NULL, 0);
        paint.getFillPath(*platformContext()->currentPath(), &boundingPath);
    } else
        boundingPath = *platformContext()->currentPath();

    SkRect r;
    boundingPath.computeBounds(&r, SkPath::kExact_BoundsType);

    return r;
}


bool GraphicsContext::strokeContains(const Path& path, const FloatPoint& point) const
{
    SkPaint paint;
    platformContext()->setup_paint_stroke(&paint, NULL, 0);

    SkPath strokePath;
    paint.getFillPath(*path.platformPath(), &strokePath);

    return SkPathContainsPoint(&strokePath, point, SkPath::kWinding_FillType);
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    if (color.rgb() & 0xFF000000) {
        SkRect  r = rect;
        if (!IsRectReasonable(getCTM(), r)) {
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
            ClipRectToCanvas(*platformContext()->canvas(), r, &r);
        }

        SkPaint paint;
        platformContext()->setup_paint_common(&paint);
        paint.setColor(color.rgb());
        platformContext()->canvas()->drawRect(r, paint);
    }
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
                                      const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;
    if (!IsRectReasonable(getCTM(), r)) {
        // See fillRect().
        ClipRectToCanvas(*platformContext()->canvas(), r, &r);
    }

    SkPath  path;
    add_corner_arc(&path, r, topRight, 270);
    add_corner_arc(&path, r, bottomRight, 0);
    add_corner_arc(&path, r, bottomLeft, 90);
    add_corner_arc(&path, r, topLeft, 180);

    SkPaint paint;
    platformContext()->setup_paint_fill(&paint);
    platformContext()->canvas()->drawPath(path, paint);
    return fillRect(rect, color);
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r(rect);
    if (!IsRectReasonable(getCTM(), r))
        return;

    platformContext()->canvas()->clipRect(r);
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;

    const SkPath& p = *path.platformPath();
    if (!IsPathReasonable(getCTM(), p))
        return;

    platformContext()->canvas()->clipPath(p);
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    if (paintingDisabled())
        return;

    SkRect r(rect);
    if (!IsRectReasonable(getCTM(), r))
        return;

    SkPath  path;
    path.addOval(r, SkPath::kCW_Direction);
    // only perform the inset if we won't invert r
    if (2*thickness < rect.width() && 2*thickness < rect.height()) {
        r.inset(SkIntToScalar(thickness) ,SkIntToScalar(thickness));
        path.addOval(r, SkPath::kCCW_Direction);
    }
    platformContext()->canvas()->clipPath(path);
}


void GraphicsContext::clipOut(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect  r(rect);
    if (!IsRectReasonable(getCTM(), r))
        return;

    platformContext()->canvas()->clipRect(r, SkRegion::kDifference_Op);
}

void GraphicsContext::clipOutEllipseInRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect  oval(rect);
    if (!IsRectReasonable(getCTM(), oval))
        return;

    SkPath  path;
    path.addOval(oval, SkPath::kCCW_Direction);
    platformContext()->canvas()->clipPath(path, SkRegion::kDifference_Op);
}

void GraphicsContext::clipOut(const Path& p)
{
    if (paintingDisabled())
        return;

    const SkPath& path = *p.platformPath();
    if (!IsPathReasonable(getCTM(), path))
        return;

    platformContext()->canvas()->clipPath(path, SkRegion::kDifference_Op);
}

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    // We need the "alpha" layer flag here because the base layer is opaque
    // (the surface of the page) but layers on top may have transparent parts.
    // Without explicitly setting the alpha flag, the layer will inherit the
    // opaque setting of the base and some things won't work properly.
    platformContext()->canvas()->saveLayerAlpha(
        NULL,
        static_cast<unsigned char>(opacity * 255),
        static_cast<SkCanvas::SaveFlags>(SkCanvas::kHasAlphaLayer_SaveFlag |
                                         SkCanvas::kFullColorLayer_SaveFlag));
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

#if PLATFORM(WIN_OS)
    platformContext()->canvas()->getTopPlatformDevice().fixupAlphaBeforeCompositing();
#endif
    platformContext()->canvas()->restore();
}

void GraphicsContext::setPlatformStrokeStyle(const StrokeStyle& stroke)
{
    platformContext()->setStrokeStyle(stroke);
}

void GraphicsContext::setPlatformShadow(const IntSize& size, int blur, const Color& color)
{
    if (paintingDisabled())
        return;

    if (blur > 0) {
        SkColor c;
        
        if (color.isValid())
            c = color.rgb();
        else
            SkColorSetARGB(0xFF/3, 0, 0, 0);    // "std" apple shadow color
        
        SkDrawLooper* dl = new SkBlurDrawLooper(SkIntToScalar(blur),
                                                SkIntToScalar(size.width()),
                                                SkIntToScalar(size.height()),
                                                c);
        platformContext()->setDrawLooper(dl)->unref();
    } else
        platformContext()->setDrawLooper(NULL);
}

void GraphicsContext::clearPlatformShadow()
{
    platformContext()->setDrawLooper(NULL);
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
    for (unsigned i = 0; i < rectCount; i++) {
        SkIRect r = rects[i];
        r.inset(-exterior_offset, -exterior_offset);
        exterior_region.op(r, SkRegion::kUnion_Op);
    }

    SkPath path;
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);

    paint.setColor(focusRingColor().rgb());
    paint.setStrokeWidth(exterior_offset * 2);
    paint.setPathEffect(new SkCornerPathEffect(exterior_offset * 2))->unref();
    exterior_region.getBoundaryPath(&path);
    platformContext()->canvas()->drawPath(path, paint);
}

PlatformGraphicsContext* GraphicsContext::platformContext() const
{
    ASSERT(!paintingDisabled());
    return m_data->context();
}

void GraphicsContext::setMiterLimit(float limit)
{
    platformContext()->setMiterLimit(limit);
}

void GraphicsContext::setAlpha(float alpha)
{
    platformContext()->setAlpha(alpha);
}

void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    platformContext()->setPorterDuffMode(WebCoreCompositeToSkiaComposite(op));
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;
    if (!IsRectReasonable(getCTM(), r))
        ClipRectToCanvas(*platformContext()->canvas(), r, &r);

    SkPaint paint;
    platformContext()->setup_paint_fill(&paint);
    paint.setPorterDuffXfermode(SkPorterDuff::kClear_Mode);
    platformContext()->canvas()->drawRect(r, paint);
}

void GraphicsContext::setLineCap(LineCap cap)
{
    switch (cap) {
    case ButtCap:
        platformContext()->setLineCap(SkPaint::kButt_Cap);
        break;
    case RoundCap:
        platformContext()->setLineCap(SkPaint::kRound_Cap);
        break;
    case SquareCap:
        platformContext()->setLineCap(SkPaint::kSquare_Cap);
        break;
    default:
        SkDEBUGF(("GraphicsContext::setLineCap: unknown LineCap %d\n", cap));
        break;
    }
}

void GraphicsContext::setLineDash(const DashArray& dashes, float dashOffset)
{
    // TODO(dglazkov): This is lifted directly off SkiaSupport, lines 49-74
    // so it is not guaranteed to work correctly. I made some minor cosmetic
    // refactoring, but not much else. Please fix this?
    size_t dashLength = dashes.size();
    if (!dashLength)
        return;

    size_t count = (dashLength % 2) == 0 ? dashLength : dashLength * 2;
    SkScalar* intervals = new SkScalar[count];

    for(unsigned int i = 0; i < count; i++)
        intervals[i] = dashes[i % dashLength];

    platformContext()->setDashPathEffect(new SkDashPathEffect(intervals,
                                                   count, dashOffset));

    delete[] intervals;
}

void GraphicsContext::setLineJoin(LineJoin join)
{
    switch (join) {
    case MiterJoin:
        platformContext()->setLineJoin(SkPaint::kMiter_Join);
        break;
    case RoundJoin:
        platformContext()->setLineJoin(SkPaint::kRound_Join);
        break;
    case BevelJoin:
        platformContext()->setLineJoin(SkPaint::kBevel_Join);
        break;
    default:
        SkDEBUGF(("GraphicsContext::setLineJoin: unknown LineJoin %d\n", join));
        break;
    }
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;
    platformContext()->canvas()->scale(WebCoreFloatToSkScalar(size.width()), WebCoreFloatToSkScalar(size.height()));
}

void GraphicsContext::rotate(float angleInRadians)
{
    if (paintingDisabled())
        return;
    platformContext()->canvas()->rotate(WebCoreFloatToSkScalar(angleInRadians * (180.0f / 3.14159265f)));
}

void GraphicsContext::translate(float w, float h)
{
    if (paintingDisabled())
        return;
    platformContext()->canvas()->translate(WebCoreFloatToSkScalar(w), WebCoreFloatToSkScalar(h));
}

void GraphicsContext::concatCTM(const AffineTransform& xform)
{
    platformContext()->canvas()->concat(xform);
}

AffineTransform GraphicsContext::getCTM() const
{
    return platformContext()->canvas()->getTotalMatrix();
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

    const SkMatrix& deviceMatrix = platformContext()->canvas()->getTotalMatrix();
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
    platformContext()->setFillColor(color.rgb());
}

void GraphicsContext::setPlatformStrokeColor(const Color& strokecolor)
{
    platformContext()->setStrokeColor(strokecolor.rgb());
}

void GraphicsContext::setPlatformStrokeThickness(float thickness)
{
    platformContext()->setStrokeThickness(thickness);
}

void GraphicsContext::setPlatformTextDrawingMode(int mode)
{
    platformContext()->setTextDrawingMode(mode);
}

void GraphicsContext::addPath(const Path& path)
{
    platformContext()->addPath(*path.platformPath());
}

void GraphicsContext::beginPath()
{
    platformContext()->beginPath();
}

void GraphicsContext::setUseAntialiasing(bool enable)
{
    if (paintingDisabled())
        return;
    platformContext()->setUseAntialiasing(enable);
}

// TODO(eseidel): This is needed for image masking and complex text fills
void GraphicsContext::clipToImageBuffer(const FloatRect& rect, const ImageBuffer* imageBuffer)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality)
{
    notImplemented();
}

}
