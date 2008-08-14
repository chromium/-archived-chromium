// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BASE_GFX_VECTOR_DEVICE_H__
#define BASE_GFX_VECTOR_DEVICE_H__

#include "base/basictypes.h"
#include "base/gfx/platform_device_win.h"
#include "SkMatrix.h"
#include "SkRegion.h"

namespace gfx {

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
  virtual void drawPoints(const SkDraw& draw, SkCanvas::PointMode mode, size_t count,
                          const SkPoint[], const SkPaint& paint);
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
  virtual void drawVertices(const SkDraw& draw, SkCanvas::VertexMode, int vertexCount,
                            const SkPoint verts[], const SkPoint texs[],
                            const SkColor colors[], SkXfermode* xmode,
                            const uint16_t indices[], int indexCount,
                            const SkPaint& paint);
  virtual void drawDevice(const SkDraw& draw, SkDevice*, int x, int y,
                          const SkPaint&);


  virtual void setMatrixClip(const SkMatrix& transform, const SkRegion& region);
  virtual void setDeviceOffset(int x, int y);
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

  int offset_x_;
  int offset_y_;

  DISALLOW_EVIL_CONSTRUCTORS(VectorDevice);
};

}  // namespace gfx

#endif  // BASE_GFX_VECTOR_DEVICE_H__
