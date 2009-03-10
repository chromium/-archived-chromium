// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_PAINTER_H__
#define CHROME_VIEWS_PAINTER_H__

#include <vector>

#include "base/basictypes.h"
#include "skia/include/SkColor.h"

class ChromeCanvas;
class SkBitmap;

namespace views {

// Painter, as the name implies, is responsible for painting in a particular
// region. Think of Painter as a Border or Background that can be painted
// in any region of a View.
class Painter {
 public:
  // A convenience method for painting a Painter in a particular region.
  // This translates the canvas to x/y and paints the painter.
  static void PaintPainterAt(int x, int y, int w, int h,
                             ChromeCanvas* canvas, Painter* painter);

  // Creates a painter that draws a gradient between the two colors.
  static Painter* CreateHorizontalGradient(SkColor c1, SkColor c2);
  static Painter* CreateVerticalGradient(SkColor c1, SkColor c2);

  virtual ~Painter() {}

  // Paints the painter in the specified region.
  virtual void Paint(int w, int h, ChromeCanvas* canvas) = 0;
};

// ImagePainter paints 8 (or 9) images into a box. The four corner
// images are drawn at the size of the image, the top/left/bottom/right
// images are tiled to fit the area, and the center (if rendered) is
// stretched.
class ImagePainter : public Painter {
 public:
  enum BorderElements {
    BORDER_TOP_LEFT = 0,
    BORDER_TOP,
    BORDER_TOP_RIGHT,
    BORDER_RIGHT,
    BORDER_BOTTOM_RIGHT,
    BORDER_BOTTOM,
    BORDER_BOTTOM_LEFT,
    BORDER_LEFT,
    BORDER_CENTER
  };

  // Constructs a new ImagePainter loading the specified image names.
  // The images must be in the order defined by the BorderElements.
  // If draw_center is false, there must be 8 image names, if draw_center
  // is true, there must be 9 image names with the last giving the name
  // of the center image.
  ImagePainter(const int image_resource_names[],
               bool draw_center);

  virtual ~ImagePainter() {}

  // Paints the images.
  virtual void Paint(int w, int h, ChromeCanvas* canvas);

  // Returns the specified image. The returned image should NOT be deleted.
  SkBitmap* GetImage(BorderElements element) {
    return images_[element];
  }

 private:
  bool tile_;
  bool draw_center_;
  bool tile_center_;
  // NOTE: the images are owned by ResourceBundle. Don't free them.
  std::vector<SkBitmap*> images_;

  DISALLOW_EVIL_CONSTRUCTORS(ImagePainter);
};

// HorizontalPainter paints 3 images into a box: left, center and right. The
// left and right images are drawn to size at the left/right edges of the
// region. The center is tiled in the remaining space. All images must have the
// same height.
class HorizontalPainter : public Painter {
 public:
  // Constructs a new HorizontalPainter loading the specified image names.
  // The images must be in the order left, right and center.
  explicit HorizontalPainter(const int image_resource_names[]);

  virtual ~HorizontalPainter() {}

  // Paints the images.
  virtual void Paint(int w, int h, ChromeCanvas* canvas);

  // Height of the images.
  int height() const { return height_; }

 private:
  // The image chunks.
  enum BorderElements {
    LEFT,
    CENTER,
    RIGHT
  };

  // The height.
  int height_;
  // NOTE: the images are owned by ResourceBundle. Don't free them.
  SkBitmap* images_[3];

  DISALLOW_EVIL_CONSTRUCTORS(HorizontalPainter);
};

}  // namespace views

#endif  // CHROME_VIEWS_PAINTER_H__
