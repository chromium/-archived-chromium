// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_VECTOR_DEVICE_H_
#define SKIA_EXT_VECTOR_DEVICE_H_

#include "skia/ext/platform_device_win.h"
#include "SkMatrix.h"
#include "SkRegion.h"

namespace skia {

// A device is basically a wrapper around SkBitmap that provides a surface for
// SkCanvas to draw into. This specific device is not not backed by a surface
// and is thus unreadable. This is because the backend is completely vectorial.
// This device is a simple wrapper over a Windows device context (HDC) handle.
class VectorDevice : public PlatformDeviceWin {
 public:
  // Factory function. The DC is kept as the output context.
  static VectorDevice* create(HDC dc, int width, int height);

  VectorDevice(HDC dc, const SkBitmap& bitmap);
  virtual ~VectorDevice();

  virtual HDC getBitmapDC() {
    return hdc_;
  }

  virtual void drawPaint(const SkDraw& draw, const SkPaint& paint);
  virtual void drawPoints(const SkDraw& draw, SkCanvas::PointMode mode,
                          size_t count, const SkPoint[], const SkPaint& paint);
  virtual void drawRect(const SkDraw& draw, const SkRect& r,
                        const SkPaint& paint);
  virtual void drawPath(const SkDraw& draw, const SkPath& path,
                        const SkPaint& paint);
  virtual void drawBitmap(const SkDraw& draw, const SkBitmap& bitmap,
                          const SkMatrix& matrix, const SkPaint& paint);
  virtual void drawSprite(const SkDraw& draw, const SkBitmap& bitmap,
                          int x, int y, const SkPaint& paint);
  virtual void drawText(const SkDraw& draw, const void* text, size_t len,
                        SkScalar x, SkScalar y, const SkPaint& paint);
  virtual void drawPosText(const SkDraw& draw, const void* text, size_t len,
                           const SkScalar pos[], SkScalar constY,
                           int scalarsPerPos, const SkPaint& paint);
  virtual void drawTextOnPath(const SkDraw& draw, const void* text, size_t len,
                              const SkPath& path, const SkMatrix* matrix,
                              const SkPaint& paint);
  virtual void drawVertices(const SkDraw& draw, SkCanvas::VertexMode,
                            int vertexCount,
                            const SkPoint verts[], const SkPoint texs[],
                            const SkColor colors[], SkXfermode* xmode,
                            const uint16_t indices[], int indexCount,
                            const SkPaint& paint);
  virtual void drawDevice(const SkDraw& draw, SkDevice*, int x, int y,
                          const SkPaint&);


  virtual void setMatrixClip(const SkMatrix& transform, const SkRegion& region);
  virtual void drawToHDC(HDC dc, int x, int y, const RECT* src_rect);
  virtual bool IsVectorial() { return true; }

  void LoadClipRegion();

 private:
  // Applies the SkPaint's painting properties in the current GDI context, if
  // possible. If GDI can't support all paint's properties, returns false. It
  // doesn't execute the "commands" in SkPaint.
  bool ApplyPaint(const SkPaint& paint);

  // Selects a new object in the device context. It can be a pen, a brush, a
  // clipping region, a bitmap or a font. Returns the old selected object.
  HGDIOBJ SelectObject(HGDIOBJ object);

  // Creates a brush according to SkPaint's properties.
  bool CreateBrush(bool use_brush, const SkPaint& paint);

  // Creates a pen according to SkPaint's properties.
  bool CreatePen(bool use_pen, const SkPaint& paint);

  // Restores back the previous objects (pen, brush, etc) after a paint command.
  void Cleanup();

  // Creates a brush according to SkPaint's properties.
  bool CreateBrush(bool use_brush, COLORREF color);

  // Creates a pen according to SkPaint's properties.
  bool CreatePen(bool use_pen, COLORREF color, int stroke_width,
                 float stroke_miter, DWORD pen_style);

  // Draws a bitmap in the the device, using the currently loaded matrix.
  void InternalDrawBitmap(const SkBitmap& bitmap, int x, int y,
                          const SkPaint& paint);

  // The Windows Device Context handle. It is the backend used with GDI drawing.
  // This backend is write-only and vectorial.
  HDC hdc_;

  // Translation assigned to the DC: we need to keep track of this separately
  // so it can be updated even if the DC isn't created yet.
  SkMatrix transform_;

  // The current clipping
  SkRegion clip_region_;

  // Previously selected brush before the current drawing.
  HGDIOBJ previous_brush_;

  // Previously selected pen before the current drawing.
  HGDIOBJ previous_pen_;

  // Copy & assign are not supported.
  VectorDevice(const VectorDevice&);
  const VectorDevice& operator=(const VectorDevice&);
};

}  // namespace skia

#endif  // SKIA_EXT_VECTOR_DEVICE_H_

