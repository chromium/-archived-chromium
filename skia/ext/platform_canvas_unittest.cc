// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(awalker): clean up the const/non-const reference handling in this test

#include "build/build_config.h"

#if !defined(OS_WIN)
#include <unistd.h>
#endif

#include "skia/ext/platform_canvas.h"
#include "skia/ext/platform_device.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "SkColor.h"

// TODO(maruel): Removes once notImplemented() is not necessary anymore.
#include "NotImplemented.h"

namespace skia {

namespace {

// Return true if the canvas is filled to canvas_color, and contains a single
// rectangle filled to rect_color. This function ignores the alpha channel,
// since Windows will sometimes clear the alpha channel when drawing, and we
// will fix that up later in cases it's necessary.
bool VerifyRect(const PlatformCanvas& canvas,
                uint32_t canvas_color, uint32_t rect_color,
                int x, int y, int w, int h) {
  PlatformDevice& device = canvas.getTopPlatformDevice();
  const SkBitmap& bitmap = device.accessBitmap(false);
  SkAutoLockPixels lock(bitmap);

  // For masking out the alpha values.
  uint32_t alpha_mask = 0xFF << SK_A32_SHIFT;

  for (int cur_y = 0; cur_y < bitmap.height(); cur_y++) {
    for (int cur_x = 0; cur_x < bitmap.width(); cur_x++) {
      if (cur_x >= x && cur_x < x + w &&
          cur_y >= y && cur_y < y + h) {
        // Inside the square should be rect_color
        if ((*bitmap.getAddr32(cur_x, cur_y) | alpha_mask) !=
            (rect_color | alpha_mask))
          return false;
      } else {
        // Outside the square should be canvas_color
        if ((*bitmap.getAddr32(cur_x, cur_y) | alpha_mask) !=
            (canvas_color | alpha_mask))
          return false;
      }
    }
  }
  return true;
}

// Checks whether there is a white canvas with a black square at the given
// location in pixels (not in the canvas coordinate system).
bool VerifyBlackRect(const PlatformCanvas& canvas, int x, int y, int w, int h) {
  return VerifyRect(canvas, SK_ColorWHITE, SK_ColorBLACK, x, y, w, h);
}

// Check that every pixel in the canvas is a single color.
bool VerifyCanvasColor(const PlatformCanvas& canvas, uint32_t canvas_color) {
  return VerifyRect(canvas, canvas_color, 0, 0, 0, 0, 0);
}

#if defined(OS_WIN)
void DrawNativeRect(PlatformCanvas& canvas, int x, int y, int w, int h) {
  HDC dc = canvas.beginPlatformPaint();

  RECT inner_rc;
  inner_rc.left = x;
  inner_rc.top = y;
  inner_rc.right = x + w;
  inner_rc.bottom = y + h;
  FillRect(dc, &inner_rc, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

  canvas.endPlatformPaint();
}
#elif defined(OS_MACOSX)
void DrawNativeRect(PlatformCanvas& canvas, int x, int y, int w, int h) {
  CGContextRef context = canvas.beginPlatformPaint();
  
  CGRect inner_rc = CGRectMake(x, y, w, h);
  // RGBA opaque black
  CGColorRef black = CGColorCreateGenericRGB(0.0, 0.0, 0.0, 1.0);
  CGContextSetFillColorWithColor(context, black);
  CGColorRelease(black);
  CGContextFillRect(context, inner_rc);
  
  canvas.endPlatformPaint();
}
#else
void DrawNativeRect(PlatformCanvas& canvas, int x, int y, int w, int h) {
  notImplemented();
}
#endif

// Clips the contents of the canvas to the given rectangle. This will be
// intersected with any existing clip.
void AddClip(PlatformCanvas& canvas, int x, int y, int w, int h) {
  SkRect rect;
  rect.set(SkIntToScalar(x), SkIntToScalar(y),
           SkIntToScalar(x + w), SkIntToScalar(y + h));
  canvas.clipRect(rect);
}

class LayerSaver {
 public:
  LayerSaver(PlatformCanvas& canvas, int x, int y, int w, int h)
      : canvas_(canvas),
        x_(x),
        y_(y),
        w_(w),
        h_(h) {
    SkRect bounds;
    bounds.set(SkIntToScalar(x_), SkIntToScalar(y_),
               SkIntToScalar(right()), SkIntToScalar(bottom()));
    canvas_.saveLayer(&bounds, NULL);
  }

  ~LayerSaver() {
    canvas_.restore();
  }

  int x() const { return x_; }
  int y() const { return y_; }
  int w() const { return w_; }
  int h() const { return h_; }

  // Returns the EXCLUSIVE far bounds of the layer.
  int right() const { return x_ + w_; }
  int bottom() const { return y_ + h_; }

 private:
  PlatformCanvas& canvas_;
  int x_, y_, w_, h_;
};

// Size used for making layers in many of the below tests.
const int kLayerX = 2;
const int kLayerY = 3;
const int kLayerW = 9;
const int kLayerH = 7;

// Size used by some tests to draw a rectangle inside the layer.
const int kInnerX = 4;
const int kInnerY = 5;
const int kInnerW = 2;
const int kInnerH = 3;

}

// This just checks that our checking code is working properly, it just uses
// regular skia primitives.
TEST(PlatformCanvas, SkLayer) {
  // Create the canvas initialized to opaque white.
  PlatformCanvas canvas(16, 16, true);
  canvas.drawColor(SK_ColorWHITE);

  // Make a layer and fill it completely to make sure that the bounds are
  // correct.
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    canvas.drawColor(SK_ColorBLACK);
  }
  EXPECT_TRUE(VerifyBlackRect(canvas, kLayerX, kLayerY, kLayerW, kLayerH));
}

// Test native clipping.
TEST(PlatformCanvas, ClipRegion) {
  // Initialize a white canvas
  PlatformCanvas canvas(16, 16, true);
  canvas.drawColor(SK_ColorWHITE);
  EXPECT_TRUE(VerifyCanvasColor(canvas, SK_ColorWHITE));

  // Test that initially the canvas has no clip region, by filling it
  // with a black rectangle.
  // Note: Don't use LayerSaver, since internally it sets a clip region.
  DrawNativeRect(canvas, 0, 0, 16, 16);
  EXPECT_TRUE(VerifyCanvasColor(canvas, SK_ColorBLACK));

  // Test that intersecting disjoint clip rectangles sets an empty clip region
  canvas.drawColor(SK_ColorWHITE);
  EXPECT_TRUE(VerifyCanvasColor(canvas, SK_ColorWHITE));
  {
    LayerSaver layer(canvas, 0, 0, 16, 16);
    AddClip(canvas, 2, 3, 4, 5);
    AddClip(canvas, 4, 9, 10, 10);
    DrawNativeRect(canvas, 0, 0, 16, 16);
  }
  EXPECT_TRUE(VerifyCanvasColor(canvas, SK_ColorWHITE));
}

// Test the layers get filled properly by native rendering.
TEST(PlatformCanvas, FillLayer) {
  // Create the canvas initialized to opaque white.
  PlatformCanvas canvas(16, 16, true);

  // Make a layer and fill it completely to make sure that the bounds are
  // correct.
  canvas.drawColor(SK_ColorWHITE);
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawNativeRect(canvas, 0, 0, 100, 100);
#if defined(OS_WIN)
    canvas.getTopPlatformDevice().makeOpaque(0, 0, 100, 100);
#endif
  }
  EXPECT_TRUE(VerifyBlackRect(canvas, kLayerX, kLayerY, kLayerW, kLayerH));

  // Make a layer and fill it partially to make sure the translation is correct.
  canvas.drawColor(SK_ColorWHITE);
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawNativeRect(canvas, kInnerX, kInnerY, kInnerW, kInnerH);
#if defined(OS_WIN)
    canvas.getTopPlatformDevice().makeOpaque(kInnerX, kInnerY,
                                             kInnerW, kInnerH);
#endif
  }
  EXPECT_TRUE(VerifyBlackRect(canvas, kInnerX, kInnerY, kInnerW, kInnerH));

  // Add a clip on the layer and fill to make sure clip is correct.
  canvas.drawColor(SK_ColorWHITE);
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    canvas.save();
    AddClip(canvas, kInnerX, kInnerY, kInnerW, kInnerH);
    DrawNativeRect(canvas, 0, 0, 100, 100);
#if defined(OS_WIN)
    canvas.getTopPlatformDevice().makeOpaque(
        kInnerX, kInnerY, kInnerW, kInnerH);
#endif
    canvas.restore();
  }
  EXPECT_TRUE(VerifyBlackRect(canvas, kInnerX, kInnerY, kInnerW, kInnerH));

  // Add a clip and then make the layer to make sure the clip is correct.
  canvas.drawColor(SK_ColorWHITE);
  canvas.save();
  AddClip(canvas, kInnerX, kInnerY, kInnerW, kInnerH);
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawNativeRect(canvas, 0, 0, 100, 100);
#if defined(OS_WIN)
    canvas.getTopPlatformDevice().makeOpaque(0, 0, 100, 100);
#endif
  }
  canvas.restore();
  EXPECT_TRUE(VerifyBlackRect(canvas, kInnerX, kInnerY, kInnerW, kInnerH));
}

// Test that translation + make layer works properly.
TEST(PlatformCanvas, TranslateLayer) {
  // Create the canvas initialized to opaque white.
  PlatformCanvas canvas(16, 16, true);

  // Make a layer and fill it completely to make sure that the bounds are
  // correct.
  canvas.drawColor(SK_ColorWHITE);
  canvas.save();
  canvas.translate(1, 1);
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawNativeRect(canvas, 0, 0, 100, 100);
#if defined(OS_WIN)
    canvas.getTopPlatformDevice().makeOpaque(0, 0, 100, 100);
#endif
  }
  canvas.restore();
  EXPECT_TRUE(VerifyBlackRect(canvas, kLayerX + 1, kLayerY + 1,
                              kLayerW, kLayerH));

  // Translate then make the layer.
  canvas.drawColor(SK_ColorWHITE);
  canvas.save();
  canvas.translate(1, 1);
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawNativeRect(canvas, kInnerX, kInnerY, kInnerW, kInnerH);
#if defined(OS_WIN)
    canvas.getTopPlatformDevice().makeOpaque(kInnerX, kInnerY,
                                             kInnerW, kInnerH);
#endif
  }
  canvas.restore();
  EXPECT_TRUE(VerifyBlackRect(canvas, kInnerX + 1, kInnerY + 1,
                              kInnerW, kInnerH));

  // Make the layer then translate.
  canvas.drawColor(SK_ColorWHITE);
  canvas.save();
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    canvas.translate(1, 1);
    DrawNativeRect(canvas, kInnerX, kInnerY, kInnerW, kInnerH);
#if defined(OS_WIN)
    canvas.getTopPlatformDevice().makeOpaque(kInnerX, kInnerY,
                                             kInnerW, kInnerH);
#endif
  }
  canvas.restore();
  EXPECT_TRUE(VerifyBlackRect(canvas, kInnerX + 1, kInnerY + 1,
                              kInnerW, kInnerH));

  // Translate both before and after, and have a clip.
  canvas.drawColor(SK_ColorWHITE);
  canvas.save();
  canvas.translate(1, 1);
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    canvas.translate(1, 1);
    AddClip(canvas, kInnerX, kInnerY, kInnerW, kInnerH);
    DrawNativeRect(canvas, 0, 0, 100, 100);
#if defined(OS_WIN)
    canvas.getTopPlatformDevice().makeOpaque(kInnerX, kInnerY,
                                             kInnerW, kInnerH);
#endif
  }
  canvas.restore();
  EXPECT_TRUE(VerifyBlackRect(canvas, kInnerX + 2, kInnerY + 2,
                              kInnerW, kInnerH));
}

}  // namespace skia
