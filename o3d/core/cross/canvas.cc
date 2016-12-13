/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the implementation of the Canvas class.

#include "core/cross/precompile.h"
#include "core/cross/canvas.h"
#include "core/cross/canvas_paint.h"
#include "core/cross/canvas_utils.h"
#include "core/cross/client.h"
#include "core/cross/error.h"

#include "third_party/skia/include/core/SkPath.h"

namespace o3d {

O3D_DEFN_CLASS(Canvas, ParamObject);

Canvas::Canvas(ServiceLocator* service_locator)
    : ParamObject(service_locator),
      width_(0),
      height_(0) {
  // Initialize a 0x0 bitmap
  sk_bitmap_.setConfig(SkBitmap::kARGB_8888_Config, 0, 0);
  sk_canvas_.setBitmapDevice(sk_bitmap_);
}

Canvas::~Canvas() {
}

bool Canvas::SetSize(int width, int height) {
  width_ = width;
  height_ = height;

  sk_bitmap_.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  if (!sk_bitmap_.allocPixels()) {
    DLOG(ERROR) << "Failed to allocate Skia bitmap";
    return false;
  }
  sk_canvas_.setBitmapDevice(sk_bitmap_);
  // Translate and flip our canvas to change from o3d coordinates
  // (where the lower left is (0,0)) to skia coordinates (where the
  // upper left is (0,0))
  sk_canvas_.translate(0, SkIntToScalar(sk_bitmap_.height()));
  sk_canvas_.scale(SK_Scalar1, -SK_Scalar1);

  return true;
}

void Canvas::Clear(float red, float green, float blue, float alpha) {
  sk_bitmap_.eraseColor(Float4ToSkColor(Float4(red, green, blue, alpha)));
}

void Canvas::DrawRect(float left,
                      float top,
                      float right,
                      float bottom,
                      CanvasPaint* paint) {
  sk_canvas_.drawRectCoords(SkFloatToScalar(left),
                            SkFloatToScalar(top),
                            SkFloatToScalar(right),
                            SkFloatToScalar(bottom),
                            paint->GetNativePaint());
}

void Canvas::DrawText(const String& text,
                      float x,
                      float y,
                      CanvasPaint* paint) {
#ifndef OS_LINUX
  sk_canvas_.drawText(text.c_str(),
                      text.length(),
                      SkFloatToScalar(x),
                      SkFloatToScalar(y),
                      paint->GetNativePaint());
#else
  O3D_ERROR(service_locator()) << "Text is not yet supported on Linux";
#endif
}

void Canvas::DrawTextOnPath(const String& text,
                            std::vector<Float2> positions,
                            float horizontal_offset,
                            float vertical_offset,
                            CanvasPaint* paint) {
#ifndef OS_LINUX
  unsigned int size = positions.size();
  if (size < 2) {
    O3D_ERROR(service_locator()) << "Must provide at least two positions"
                                 << " for drawTextOnPath!";
    return;
  }

  SkPath path;

  path.moveTo(SkFloatToScalar(positions[0].getX()),
              SkFloatToScalar(positions[0].getY()));
  for (unsigned int i = 1; i < size; i++) {
    path.lineTo(SkFloatToScalar(positions[i].getX()),
                SkFloatToScalar(positions[i].getY()));
  }
  SkPaint nativePaint = paint->GetNativePaint();

  sk_canvas_.drawTextOnPathHV(text.c_str(),
                              text.length(),
                              path,
                              SkFloatToScalar(horizontal_offset),
                              SkFloatToScalar(vertical_offset),
                              nativePaint);
#else
  O3D_ERROR(service_locator()) << "Text is not yet supported on Linux";
#endif
}

void Canvas::DrawBitmap(Texture2D* texture2d,
                        float left,
                        float bottom) {
  if (!texture2d)
    return;

  if (texture2d->format() != Texture2D::ARGB8 &&
      texture2d->format() != Texture2D::XRGB8) {
    O3D_ERROR(service_locator()) <<
        "Texture format must be ARGB8 or XRGB8 for drawBitmap";
    return;
  }

  void* texture_data;
  Texture2DLockHelper lock_helper(texture2d, 0);
  texture_data = lock_helper.GetData();
  if (!texture_data) {
    return;
  }

  int height = texture2d->height();
  int width = texture2d->width();

  // Create a temporary bitmap to copy the texture data into.
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);

  // Allocate enough space to copy the pixels over to the bitmap.
  if (!bitmap.allocPixels()) {
    O3D_ERROR(service_locator()) << "Unable to allocate bitmap";
    return;
  }

  unsigned char* bitmap_data = static_cast<unsigned char*>(
      bitmap.getPixels());

  memcpy(bitmap_data, texture_data, width * height * 4);

  if (texture2d->format() == Texture2D::XRGB8) {
    // Set the alpha to 1
    for (int ii = 0; ii < width * height; ii++) {
      bitmap_data[3] = 0xff;
      bitmap_data += 4;
    }
  } else {
    // Pre-multiply (i.e. multiply the color by alpha) the color since the
    // Skia bitmap expects it in that format.
    for (int ii = 0; ii < width * height; ii++) {
      unsigned char* b = (bitmap_data + 0);
      unsigned char* g = (bitmap_data + 1);
      unsigned char* r = (bitmap_data + 2);
      unsigned char a = bitmap_data[3];

      *r = SkMulDiv255Round(*r, a);
      *g = SkMulDiv255Round(*g, a);
      *b = SkMulDiv255Round(*b, a);
      bitmap_data += 4;
    }
  }

  // Now copy from the temporary bitmap to the canvas bitmap.
  // Note that we scale Y by -1 to flip the image vertically.  The reason is
  // that in O3D textures the first byte is the bottom left corner whereas
  // in Skia the first byte is the top left of a bitmap.
  SaveMatrix();
  Scale(1, -1);

  sk_canvas_.drawBitmap(bitmap,
                        SkFloatToScalar(left),
                        SkFloatToScalar(-bottom),
                        NULL);
  RestoreMatrix();
}

void Canvas::Rotate(float degrees) {
  sk_canvas_.rotate(SkFloatToScalar(degrees));
}

void Canvas::Scale(float sx, float sy) {
  sk_canvas_.scale(SkFloatToScalar(sx), SkFloatToScalar(sy));
}

void Canvas::Translate(float dx, float dy) {
  sk_canvas_.translate(SkFloatToScalar(dx), SkFloatToScalar(dy));
}

// Copy the contents of the local bitmap to a Texture object.
bool Canvas::CopyToTexture(Texture2D* texture_2d) {
  DCHECK(texture_2d);

  if (texture_2d->width() != sk_bitmap_.width() ||
      texture_2d->height() != sk_bitmap_.height() ||
      (texture_2d->format() != Texture2D::ARGB8 &&
       texture_2d->format() != Texture2D::XRGB8)) {
    O3D_ERROR(service_locator())
        << "Texture format and size doesn't match Canvas";
    return false;
  }

  int width = sk_bitmap_.width();
  int height = sk_bitmap_.height();
  Texture2DLockHelper lock_helper_0(texture_2d, 0);
  void* texture_data = lock_helper_0.GetData();
  if (!texture_data) {
    return false;
  }

  memcpy(texture_data, sk_bitmap_.getPixels(), width * height * 4);

  // Fill in all the mipmap levels of the texture by drawing scaled down
  // versions of the canvas bitmap contents.
  SkBitmap bitmap;
  int levels = texture_2d->levels();

  for (int i = 1; (!levels && width > 1 && height > 1) || i < levels; i++) {
    width = width >> 1;
    height = height >> 1;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);

    Texture2DLockHelper lock_helper_n(texture_2d, i);
    texture_data = lock_helper_n.GetData();
    if (!texture_data) {
      return false;
    }

    bitmap.setPixels(texture_data);
    SkCanvas canvas(bitmap);
    SkScalar scaleFactor = SkScalarDiv(SK_Scalar1, SkIntToScalar(1 << i));
    canvas.scale(scaleFactor, scaleFactor);
    canvas.drawBitmap(sk_bitmap_, 0, 0);
  }

  return true;
}

ObjectBase::Ref Canvas::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new Canvas(service_locator));
}



}  // namespace o3d
