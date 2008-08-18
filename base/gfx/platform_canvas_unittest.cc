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

#include <windows.h>

#include "base/gfx/platform_canvas_win.h"
#include "base/gfx/platform_device_win.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "SkColor.h"

namespace gfx {

namespace {

// Return true if the canvas is filled to canvas_color,
// and contains a single rectangle filled to rect_color.
bool VerifyRect(const PlatformCanvasWin& canvas,
                uint32_t canvas_color, uint32_t rect_color,
                int x, int y, int w, int h) {
  PlatformDeviceWin& device = canvas.getTopPlatformDevice();
  const SkBitmap& bitmap = device.accessBitmap(false);
  SkAutoLockPixels lock(bitmap);

  for (int cur_y = 0; cur_y < bitmap.height(); cur_y++) {
    for (int cur_x = 0; cur_x < bitmap.width(); cur_x++) {
      if (cur_x >= x && cur_x < x + w &&
          cur_y >= y && cur_y < y + h) {
        // Inside the square should be rect_color
        if (*bitmap.getAddr32(cur_x, cur_y) != rect_color)
          return false;
      } else {
        // Outside the square should be canvas_color
        if (*bitmap.getAddr32(cur_x, cur_y) != canvas_color)
          return false;
      }
    }
  }
  return true;
}

// Checks whether there is a white canvas with a black square at the given
// location in pixels (not in the canvas coordinate system).
// TODO(ericroman): rename Square to Rect
bool VerifyBlackSquare(const PlatformCanvasWin& canvas, int x, int y, int w, int h) {
  return VerifyRect(canvas, SK_ColorWHITE, SK_ColorBLACK, x, y, w, h);
}

// Check that every pixel in the canvas is a single color.
bool VerifyCanvasColor(const PlatformCanvasWin& canvas, uint32_t canvas_color) {
  return VerifyRect(canvas, canvas_color, 0, 0, 0, 0, 0);
}

void DrawGDIRect(PlatformCanvasWin& canvas, int x, int y, int w, int h) {
  HDC dc = canvas.beginPlatformPaint();

  RECT inner_rc;
  inner_rc.left = x;
  inner_rc.top = y;
  inner_rc.right = x + w;
  inner_rc.bottom = y + h;
  FillRect(dc, &inner_rc, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

  canvas.endPlatformPaint();
}

// Clips the contents of the canvas to the given rectangle. This will be
// intersected with any existing clip.
void AddClip(PlatformCanvasWin& canvas, int x, int y, int w, int h) {
  SkRect rect;
  rect.set(SkIntToScalar(x), SkIntToScalar(y),
           SkIntToScalar(x + w), SkIntToScalar(y + h));
  canvas.clipRect(rect);
}

class LayerSaver {
 public:
  LayerSaver(PlatformCanvasWin& canvas, int x, int y, int w, int h)
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
    canvas_.getTopPlatformDevice().fixupAlphaBeforeCompositing();
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
  PlatformCanvasWin& canvas_;
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
TEST(PlatformCanvasWin, SkLayer) {
  // Create the canvas initialized to opaque white.
  PlatformCanvasWin canvas(16, 16, true);
  canvas.drawColor(SK_ColorWHITE);

  // Make a layer and fill it completely to make sure that the bounds are
  // correct.
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    canvas.drawColor(SK_ColorBLACK);
  }
  EXPECT_TRUE(VerifyBlackSquare(canvas, kLayerX, kLayerY, kLayerW, kLayerH));
}

// Test the GDI clipping.
TEST(PlatformCanvasWin, GDIClipRegion) {
  // Initialize a white canvas
  PlatformCanvasWin canvas(16, 16, true);
  canvas.drawColor(SK_ColorWHITE);
  EXPECT_TRUE(VerifyCanvasColor(canvas, SK_ColorWHITE));

  // Test that initially the canvas has no clip region, by filling it
  // with a black rectangle.
  // Note: Don't use LayerSaver, since internally it sets a clip region.
  DrawGDIRect(canvas, 0, 0, 16, 16);
  canvas.getTopPlatformDevice().fixupAlphaBeforeCompositing();
  EXPECT_TRUE(VerifyCanvasColor(canvas, SK_ColorBLACK));

  // Test that intersecting disjoint clip rectangles sets an empty clip region
  canvas.drawColor(SK_ColorWHITE);
  EXPECT_TRUE(VerifyCanvasColor(canvas, SK_ColorWHITE));
  {
    LayerSaver layer(canvas, 0, 0, 16, 16);
    AddClip(canvas, 2, 3, 4, 5);
    AddClip(canvas, 4, 9, 10, 10);
    DrawGDIRect(canvas, 0, 0, 16, 16);
  }
  EXPECT_TRUE(VerifyCanvasColor(canvas, SK_ColorWHITE));
}

// Test the layers get filled properly by GDI.
TEST(PlatformCanvasWin, GDILayer) {
  // Create the canvas initialized to opaque white.
  PlatformCanvasWin canvas(16, 16, true);

  // Make a layer and fill it completely to make sure that the bounds are
  // correct.
  canvas.drawColor(SK_ColorWHITE);
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawGDIRect(canvas, 0, 0, 100, 100);
  }
  EXPECT_TRUE(VerifyBlackSquare(canvas, kLayerX, kLayerY, kLayerW, kLayerH));

  // Make a layer and fill it partially to make sure the translation is correct.
  canvas.drawColor(SK_ColorWHITE);
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawGDIRect(canvas, kInnerX, kInnerY, kInnerW, kInnerH);
  }
  EXPECT_TRUE(VerifyBlackSquare(canvas, kInnerX, kInnerY, kInnerW, kInnerH));

  // Add a clip on the layer and fill to make sure clip is correct.
  canvas.drawColor(SK_ColorWHITE);
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    canvas.save();
    AddClip(canvas, kInnerX, kInnerY, kInnerW, kInnerH);
    DrawGDIRect(canvas, 0, 0, 100, 100);
    canvas.restore();
  }
  EXPECT_TRUE(VerifyBlackSquare(canvas, kInnerX, kInnerY, kInnerW, kInnerH));

  // Add a clip and then make the layer to make sure the clip is correct.
  canvas.drawColor(SK_ColorWHITE);
  canvas.save();
  AddClip(canvas, kInnerX, kInnerY, kInnerW, kInnerH);
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawGDIRect(canvas, 0, 0, 100, 100);
  }
  canvas.restore();
  EXPECT_TRUE(VerifyBlackSquare(canvas, kInnerX, kInnerY, kInnerW, kInnerH));
}

// Test that translation + make layer works properly.
TEST(PlatformCanvasWin, GDITranslateLayer) {
  // Create the canvas initialized to opaque white.
  PlatformCanvasWin canvas(16, 16, true);

  // Make a layer and fill it completely to make sure that the bounds are
  // correct.
  canvas.drawColor(SK_ColorWHITE);
  canvas.save();
  canvas.translate(1, 1);
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawGDIRect(canvas, 0, 0, 100, 100);
  }
  canvas.restore();
  EXPECT_TRUE(VerifyBlackSquare(canvas, kLayerX + 1, kLayerY + 1,
                                kLayerW, kLayerH));

  // Translate then make the layer.
  canvas.drawColor(SK_ColorWHITE);
  canvas.save();
  canvas.translate(1, 1);
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawGDIRect(canvas, kInnerX, kInnerY, kInnerW, kInnerH);
  }
  canvas.restore();
  EXPECT_TRUE(VerifyBlackSquare(canvas, kInnerX + 1, kInnerY + 1,
                                kInnerW, kInnerH));

  // Make the layer then translate.
  canvas.drawColor(SK_ColorWHITE);
  canvas.save();
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    canvas.translate(1, 1);
    DrawGDIRect(canvas, kInnerX, kInnerY, kInnerW, kInnerH);
  }
  canvas.restore();
  EXPECT_TRUE(VerifyBlackSquare(canvas, kInnerX + 1, kInnerY + 1,
                                kInnerW, kInnerH));

  // Translate both before and after, and have a clip.
  canvas.drawColor(SK_ColorWHITE);
  canvas.save();
  canvas.translate(1, 1);
  {
    LayerSaver layer(canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    canvas.translate(1, 1);
    AddClip(canvas, kInnerX, kInnerY, kInnerW, kInnerH);
    DrawGDIRect(canvas, 0, 0, 100, 100);
  }
  canvas.restore();
  EXPECT_TRUE(VerifyBlackSquare(canvas, kInnerX + 2, kInnerY + 2,
                                kInnerW, kInnerH));
}

}  // namespace
