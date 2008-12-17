// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/vector_device.h"

#include "base/gfx/gdi_util.h"
#include "base/logging.h"
#include "base/scoped_handle.h"
#include "skia/ext/skia_utils_win.h"

#include "SkUtils.h"

namespace skia {

VectorDevice* VectorDevice::create(HDC dc, int width, int height) {
  InitializeDC(dc);

  // Link the SkBitmap to the current selected bitmap in the device context.
  SkBitmap bitmap;
  HGDIOBJ selected_bitmap = GetCurrentObject(dc, OBJ_BITMAP);
  bool succeeded = false;
  if (selected_bitmap != NULL) {
    BITMAP bitmap_data;
    if (GetObject(selected_bitmap, sizeof(BITMAP), &bitmap_data) ==
        sizeof(BITMAP)) {
      // The context has a bitmap attached. Attach our SkBitmap to it.
      // Warning: If the bitmap gets unselected from the HDC, VectorDevice has
      // no way to detect this, so the HBITMAP could be released while SkBitmap
      // still has a reference to it. Be cautious.
      if (width == bitmap_data.bmWidth &&
          height == bitmap_data.bmHeight) {
        bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                         bitmap_data.bmWidth,
                         bitmap_data.bmHeight,
                         bitmap_data.bmWidthBytes);
        bitmap.setPixels(bitmap_data.bmBits);
        succeeded = true;
      }
    }
  }

  if (!succeeded)
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);

  return new VectorDevice(dc, bitmap);
}

VectorDevice::VectorDevice(HDC dc, const SkBitmap& bitmap)
    : PlatformDeviceWin(bitmap),
      hdc_(dc),
      previous_brush_(NULL),
      previous_pen_(NULL) {
  transform_.reset();
}

VectorDevice::~VectorDevice() {
  DCHECK(previous_brush_ == NULL);
  DCHECK(previous_pen_ == NULL);
}


void VectorDevice::drawPaint(const SkDraw& draw, const SkPaint& paint) {
  // TODO(maruel):  Bypass the current transformation matrix.
  SkRect rect;
  rect.fLeft = 0;
  rect.fTop = 0;
  rect.fRight = SkIntToScalar(width() + 1);
  rect.fBottom = SkIntToScalar(height() + 1);
  drawRect(draw, rect, paint);
}

void VectorDevice::drawPoints(const SkDraw& draw, SkCanvas::PointMode mode,
                              size_t count, const SkPoint pts[],
                              const SkPaint& paint) {
  if (!count)
    return;

  if (mode == SkCanvas::kPoints_PointMode) {
    NOTREACHED();
    return;
  }

  SkPaint tmp_paint(paint);
  tmp_paint.setStyle(SkPaint::kStroke_Style);

  // Draw a path instead.
  SkPath path;
  switch (mode) {
    case SkCanvas::kLines_PointMode:
      if (count % 2) {
        NOTREACHED();
        return;
      }
      for (size_t i = 0; i < count / 2; ++i) {
        path.moveTo(pts[2 * i]);
        path.lineTo(pts[2 * i + 1]);
      }
      break;
    case SkCanvas::kPolygon_PointMode:
      path.moveTo(pts[0]);
      for (size_t i = 1; i < count; ++i) {
        path.lineTo(pts[i]);
      }
      break;
    default:
      NOTREACHED();
      return;
  }
  // Draw the calculated path.
  drawPath(draw, path, tmp_paint);
}

void VectorDevice::drawRect(const SkDraw& draw, const SkRect& rect,
                            const SkPaint& paint) {
  if (paint.getPathEffect()) {
    // Draw a path instead.
    SkPath path_orginal;
    path_orginal.addRect(rect);

    // Apply the path effect to the rect.
    SkPath path_modified;
    paint.getFillPath(path_orginal, &path_modified);

    // Removes the path effect from the temporary SkPaint object.
    SkPaint paint_no_effet(paint);
    paint_no_effet.setPathEffect(NULL)->safeUnref();

    // Draw the calculated path.
    drawPath(draw, path_modified, paint_no_effet);
    return;
  }

  if (!ApplyPaint(paint)) {
    return;
  }
  HDC dc = getBitmapDC();
  if (!Rectangle(dc, SkScalarRound(rect.fLeft),
                 SkScalarRound(rect.fTop),
                 SkScalarRound(rect.fRight),
                 SkScalarRound(rect.fBottom))) {
    NOTREACHED();
  }
  Cleanup();
}

void VectorDevice::drawPath(const SkDraw& draw, const SkPath& path,
                            const SkPaint& paint) {
  if (paint.getPathEffect()) {
    // Apply the path effect forehand.
    SkPath path_modified;
    paint.getFillPath(path, &path_modified);

    // Removes the path effect from the temporary SkPaint object.
    SkPaint paint_no_effet(paint);
    paint_no_effet.setPathEffect(NULL)->safeUnref();

    // Draw the calculated path.
    drawPath(draw, path_modified, paint_no_effet);
    return;
  }

  if (!ApplyPaint(paint)) {
    return;
  }
  HDC dc = getBitmapDC();
  PlatformDeviceWin::LoadPathToDC(dc, path);
  switch (paint.getStyle()) {
    case SkPaint::kFill_Style: {
      BOOL res = StrokeAndFillPath(dc);
      DCHECK(res != 0);
      break;
    }
    case SkPaint::kStroke_Style: {
      BOOL res = StrokePath(dc);
      DCHECK(res != 0);
      break;
    }
    case SkPaint::kStrokeAndFill_Style: {
      BOOL res = StrokeAndFillPath(dc);
      DCHECK(res != 0);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  Cleanup();
}

void VectorDevice::drawBitmap(const SkDraw& draw, const SkBitmap& bitmap,
                              const SkMatrix& matrix, const SkPaint& paint) {
  // Load the temporary matrix. This is what will translate, rotate and resize
  // the bitmap.
  SkMatrix actual_transform(transform_);
  actual_transform.preConcat(matrix);
  LoadTransformToDC(hdc_, actual_transform);

  InternalDrawBitmap(bitmap, 0, 0, paint);

  // Restore the original matrix.
  LoadTransformToDC(hdc_, transform_);
}

void VectorDevice::drawSprite(const SkDraw& draw, const SkBitmap& bitmap,
                              int x, int y, const SkPaint& paint) {
  SkMatrix identity;
  identity.reset();
  LoadTransformToDC(hdc_, identity);

  InternalDrawBitmap(bitmap, x, y, paint);

  // Restore the original matrix.
  LoadTransformToDC(hdc_, transform_);
}

void VectorDevice::drawText(const SkDraw& draw, const void* text, size_t byteLength,
                            SkScalar x, SkScalar y, const SkPaint& paint) {
  // This function isn't used in the code. Verify this assumption.
  NOTREACHED();
}

void VectorDevice::drawPosText(const SkDraw& draw, const void* text, size_t len,
                               const SkScalar pos[], SkScalar constY,
                               int scalarsPerPos, const SkPaint& paint) {
  // This function isn't used in the code. Verify this assumption.
  NOTREACHED();
}

void VectorDevice::drawTextOnPath(const SkDraw& draw, const void* text,
                                  size_t len,
                                  const SkPath& path, const SkMatrix* matrix,
                                  const SkPaint& paint) {
  // This function isn't used in the code. Verify this assumption.
  NOTREACHED();
}

void VectorDevice::drawVertices(const SkDraw& draw, SkCanvas::VertexMode vmode,
                                int vertexCount,
                                const SkPoint vertices[], const SkPoint texs[],
                                const SkColor colors[], SkXfermode* xmode,
                                const uint16_t indices[], int indexCount,
                                const SkPaint& paint) {
  // This function isn't used in the code. Verify this assumption.
  NOTREACHED();
}

void VectorDevice::drawDevice(const SkDraw& draw, SkDevice* device, int x,
                              int y, const SkPaint& paint) {
  // TODO(maruel):  http://b/1183870 Playback the EMF buffer at printer's dpi if
  // it is a vectorial device.
  drawSprite(draw, device->accessBitmap(false), x, y, paint);
}

bool VectorDevice::ApplyPaint(const SkPaint& paint) {
  // Note: The goal here is to transfert the SkPaint's state to the HDC's state.
  // This function does not execute the SkPaint drawing commands. These should
  // be executed in drawPaint().

  SkPaint::Style style = paint.getStyle();
  if (!paint.getAlpha())
    style = SkPaint::kStyleCount;

  switch (style) {
    case SkPaint::kFill_Style:
      if (!CreateBrush(true, paint) ||
          !CreatePen(false, paint))
        return false;
      break;
    case SkPaint::kStroke_Style:
      if (!CreateBrush(false, paint) ||
          !CreatePen(true, paint))
        return false;
      break;
    case SkPaint::kStrokeAndFill_Style:
      if (!CreateBrush(true, paint) ||
          !CreatePen(true, paint))
        return false;
      break;
    default:
      if (!CreateBrush(false, paint) ||
          !CreatePen(false, paint))
        return false;
      break;
  }

  /*
  getFlags();
    isAntiAlias();
    isDither()
    isLinearText()
    isSubpixelText()
    isUnderlineText()
    isStrikeThruText()
    isFakeBoldText()
    isDevKernText()
    isFilterBitmap()

  // Skia's text is not used. This should be fixed.
  getTextAlign()
  getTextScaleX()
  getTextSkewX()
  getTextEncoding()
  getFontMetrics()
  getFontSpacing()
  */

  // BUG 1094907: Implement shaders. Shaders currently in use:
  //  SkShader::CreateBitmapShader
  //  SkGradientShader::CreateRadial
  //  SkGradientShader::CreateLinear
  // DCHECK(!paint.getShader());

  // http://b/1106647 Implement loopers and mask filter. Looper currently in
  // use:
  //   SkBlurDrawLooper is used for shadows.
  // DCHECK(!paint.getLooper());
  // DCHECK(!paint.getMaskFilter());

  // http://b/1165900 Implement xfermode.
  // DCHECK(!paint.getXfermode());

  // The path effect should be processed before arriving here.
  DCHECK(!paint.getPathEffect());

  // These aren't used in the code. Verify this assumption.
  DCHECK(!paint.getColorFilter());
  DCHECK(!paint.getRasterizer());
  // Reuse code to load Win32 Fonts.
  DCHECK(!paint.getTypeface());
  return true;
}

void VectorDevice::setMatrixClip(const SkMatrix& transform,
                                 const SkRegion& region) {
  transform_ = transform;
  LoadTransformToDC(hdc_, transform_);
  clip_region_ = region;
  if (!clip_region_.isEmpty())
    LoadClipRegion();
}

void VectorDevice::drawToHDC(HDC dc, int x, int y, const RECT* src_rect) {
  NOTREACHED();
}

void VectorDevice::LoadClipRegion() {
  SkMatrix t;
  t.reset();
  LoadClippingRegionToDC(hdc_, clip_region_, t);
}

bool VectorDevice::CreateBrush(bool use_brush, COLORREF color) {
  DCHECK(previous_brush_ == NULL);
  // We can't use SetDCBrushColor() or DC_BRUSH when drawing to a EMF buffer.
  // SetDCBrushColor() calls are not recorded at all and DC_BRUSH will use
  // WHITE_BRUSH instead.

  if (!use_brush) {
    // Set the transparency.
    if (0 == SetBkMode(hdc_, TRANSPARENT)) {
      NOTREACHED();
      return false;
    }

    // Select the NULL brush.
    previous_brush_ = SelectObject(GetStockObject(NULL_BRUSH));
    return previous_brush_ != NULL;
  }

  // Set the opacity.
  if (0 == SetBkMode(hdc_, OPAQUE)) {
    NOTREACHED();
    return false;
  }

  // Create and select the brush.
  previous_brush_ = SelectObject(CreateSolidBrush(color));
  return previous_brush_ != NULL;
}

bool VectorDevice::CreatePen(bool use_pen, COLORREF color, int stroke_width,
                             float stroke_miter, DWORD pen_style) {
  DCHECK(previous_pen_ == NULL);
  // We can't use SetDCPenColor() or DC_PEN when drawing to a EMF buffer.
  // SetDCPenColor() calls are not recorded at all and DC_PEN will use BLACK_PEN
  // instead.

  // No pen case
  if (!use_pen) {
    previous_pen_ = SelectObject(GetStockObject(NULL_PEN));
    return previous_pen_ != NULL;
  }

  // Use the stock pen if the stroke width is 0.
  if (stroke_width == 0) {
    // Create a pen with the right color.
    previous_pen_ = SelectObject(::CreatePen(PS_SOLID, 0, color));
    return previous_pen_ != NULL;
  }

  // Load a custom pen.
  LOGBRUSH brush;
  brush.lbStyle = BS_SOLID;
  brush.lbColor = color;
  brush.lbHatch = 0;
  HPEN pen = ExtCreatePen(pen_style, stroke_width, &brush, 0, NULL);
  DCHECK(pen != NULL);
  previous_pen_ = SelectObject(pen);
  if (previous_pen_ == NULL)
    return false;

  if (!SetMiterLimit(hdc_, stroke_miter, NULL)) {
    NOTREACHED();
    return false;
  }
  return true;
}

void VectorDevice::Cleanup() {
  if (previous_brush_) {
    HGDIOBJ result = SelectObject(previous_brush_);
    previous_brush_ = NULL;
    if (result) {
      BOOL res = DeleteObject(result);
      DCHECK(res != 0);
    }
  }
  if (previous_pen_) {
    HGDIOBJ result = SelectObject(previous_pen_);
    previous_pen_ = NULL;
    if (result) {
      BOOL res = DeleteObject(result);
      DCHECK(res != 0);
    }
  }
  // Remove any loaded path from the context.
  AbortPath(hdc_);
}

HGDIOBJ VectorDevice::SelectObject(HGDIOBJ object) {
  HGDIOBJ result = ::SelectObject(hdc_, object);
  DCHECK(result != HGDI_ERROR);
  if (result == HGDI_ERROR)
    return NULL;
  return result;
}

bool VectorDevice::CreateBrush(bool use_brush, const SkPaint& paint) {
  // Make sure that for transparent color, no brush is used.
  if (paint.getAlpha() == 0) {
    // Test if it ever happen.
    NOTREACHED();
    use_brush = false;
  }

  return CreateBrush(use_brush, SkColorToCOLORREF(paint.getColor()));
}

bool VectorDevice::CreatePen(bool use_pen, const SkPaint& paint) {
  // Make sure that for transparent color, no pen is used.
  if (paint.getAlpha() == 0) {
    // Test if it ever happen.
    NOTREACHED();
    use_pen = false;
  }

  DWORD pen_style = PS_GEOMETRIC | PS_SOLID;
  switch (paint.getStrokeJoin()) {
    case SkPaint::kMiter_Join:
      // Connects path segments with a sharp join.
      pen_style |= PS_JOIN_MITER;
      break;
    case SkPaint::kRound_Join:
      // Connects path segments with a round join.
      pen_style |= PS_JOIN_ROUND;
      break;
    case SkPaint::kBevel_Join:
      // Connects path segments with a flat bevel join.
      pen_style |= PS_JOIN_BEVEL;
      break;
    default:
      NOTREACHED();
      break;
  }
  switch (paint.getStrokeCap()) {
    case SkPaint::kButt_Cap:
      // Begin/end contours with no extension.
      pen_style |= PS_ENDCAP_FLAT;
      break;
    case SkPaint::kRound_Cap:
      // Begin/end contours with a semi-circle extension.
      pen_style |= PS_ENDCAP_ROUND;
      break;
    case SkPaint::kSquare_Cap:
      // Begin/end contours with a half square extension.
      pen_style |= PS_ENDCAP_SQUARE;
      break;
    default:
      NOTREACHED();
      break;
  }

  return CreatePen(use_pen,
                   SkColorToCOLORREF(paint.getColor()),
                   SkScalarRound(paint.getStrokeWidth()),
                   paint.getStrokeMiter(),
                   pen_style);
}

void VectorDevice::InternalDrawBitmap(const SkBitmap& bitmap, int x, int y,
                                      const SkPaint& paint) {
  uint8 alpha = paint.getAlpha();
  if (alpha == 0)
    return;

  bool is_translucent;
  if (alpha != 255) {
    // ApplyPaint expect an opaque color.
    SkPaint tmp_paint(paint);
    tmp_paint.setAlpha(255);
    if (!ApplyPaint(tmp_paint))
      return;
    is_translucent = true;
  } else {
    if (!ApplyPaint(paint))
      return;
    is_translucent = false;
  }
  int src_size_x = bitmap.width();
  int src_size_y = bitmap.height();
  if (!src_size_x || !src_size_y)
    return;

  // Create a BMP v4 header that we can serialize.
  BITMAPV4HEADER bitmap_header;
  gfx::CreateBitmapV4Header(src_size_x, src_size_y, &bitmap_header);
  HDC dc = getBitmapDC();
  SkAutoLockPixels lock(bitmap);
  DCHECK_EQ(bitmap.getConfig(), SkBitmap::kARGB_8888_Config);
  const uint32_t* pixels = static_cast<const uint32_t*>(bitmap.getPixels());
  if (pixels == NULL) {
    NOTREACHED();
    return;
  }

  if (!is_translucent) {
    int row_length = bitmap.rowBytesAsPixels();
    // There is no quick way to determine if an image is opaque.
    for (int y2 = 0; y2 < src_size_y; ++y2) {
      for (int x2 = 0; x2 < src_size_x; ++x2) {
        if (SkColorGetA(pixels[(y2 * row_length) + x2]) != 255) {
          is_translucent = true;
          y2 = src_size_y;
          break;
        }
      }
    }
  }

  BITMAPINFOHEADER hdr;
  gfx::CreateBitmapHeader(src_size_x, src_size_y, &hdr);
  if (is_translucent) {
    // The image must be loaded as a bitmap inside a device context.
    ScopedHDC bitmap_dc(::CreateCompatibleDC(dc));
    void* bits = NULL;
    ScopedBitmap hbitmap(::CreateDIBSection(
        bitmap_dc, reinterpret_cast<const BITMAPINFO*>(&hdr),
        DIB_RGB_COLORS, &bits, NULL, 0));
    memcpy(bits, pixels, bitmap.getSize());
    DCHECK(hbitmap);
    HGDIOBJ old_bitmap = ::SelectObject(bitmap_dc, hbitmap);
    DeleteObject(old_bitmap);

    // After some analysis of IE7's behavior, this is the thing to do. I was
    // sure IE7 was doing so kind of bitmasking due to the way translucent image
    // where renderered but after some windbg tracing, it is being done by the
    // printer driver after all (mostly HP printers). IE7 always use AlphaBlend
    // for bitmasked images. The trick seems to switch the stretching mode in
    // what the driver expects.
    DWORD previous_mode = GetStretchBltMode(dc);
    BOOL result = SetStretchBltMode(dc, COLORONCOLOR);
    DCHECK(result);
    // Note that this function expect premultiplied colors (!)
    BLENDFUNCTION blend_function = {AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA};
    result = GdiAlphaBlend(dc,
                           x, y,  // Destination origin.
                           src_size_x, src_size_y,  // Destination size.
                           bitmap_dc,
                           0, 0,  // Source origin.
                           src_size_x, src_size_y,  // Source size.
                           blend_function);
    DCHECK(result);
    result = SetStretchBltMode(dc, previous_mode);
    DCHECK(result);
  } else {
    BOOL result = StretchDIBits(dc,
                                x, y,  // Destination origin.
                                src_size_x, src_size_y,
                                0, 0,  // Source origin.
                                src_size_x, src_size_y,  // Source size.
                                pixels,
                                reinterpret_cast<const BITMAPINFO*>(&hdr),
                                DIB_RGB_COLORS,
                                SRCCOPY);
    DCHECK(result);
  }
  Cleanup();
}

}  // namespace skia

